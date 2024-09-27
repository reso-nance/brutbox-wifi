#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <OSCMessage.h>

//#define SERIAL_DEBUG // if defined debug messages will be printed via the serial port @115200 bauds
//#define SHOCK_THREESHOLD 154 // if defined, a /shock message will be sent every time the value changes drastically
#define MAX_RATE 10 // minimum time between two OSC messages (in milliseconds). If set too high or too low (<30ms), some latency will appear
#define TOLERANCE 1 // minimal change (0~1023) that will trigger an OSC message, depends on the sensor (min ~5 for a potentiometer or an FSR, ~40 for a sharp distance sensor, ~10 for an LDR)
// BB_NAME : pres, lum, dis, rot, xyz, btn
#define THIS_BB_NAME "dis" // name of this brutbox, will be used in OSC path and hostname

#ifdef SERIAL_DEBUG
  #define debugPrint(x)  Serial.print (x)
  #define debugPrintln(x)  Serial.println (x)
#else
  #define debugPrint(x)
  #define debugPrintln(x)
#endif
#define OSCPREFIX(x) x"_"

const String MACaddress = WiFi.macAddress().substring(9); // get the unique MAC address of this ESP to avoid conflicting names, remove the manufacturer ID (first 9 characters) from the MAC
String hostname = OSCPREFIX(THIS_BB_NAME)+MACaddress; // 
static const char* PSK = "brutbox123";
static const char* SSID = "brutbox";
static const uint16_t oscOutPort = 8000;
IPAddress targetIP = IPAddress({10,0,0,255});
static const uint8_t rgbPins[] = {D8, D7, D6};
unsigned long nextPeriod = 0;
uint16_t lastValue = 0;
WiFiUDP udpserver;

#ifdef SHOCK_THREESHOLD
bool isShock = false;
#endif

// Moving average variables
const int numReadings = 10;
int readings[numReadings];      // the readings from the sensor
int readIndex = 0;              // the index of the current reading
int total = 0;                  // the running total
int average = 0;                // the average

void setup() {
  #ifdef SERIAL_DEBUG
  Serial.begin(115200);
  #endif

  // Initialize all readings to 0
  for (int i = 0; i < numReadings; i++) {
    readings[i] = 0;
  }

  for (uint8_t i=0; i<3; i++) {
    pinMode(rgbPins[i], OUTPUT);
  }

  hostname.replace(":", ""); // remove the : from the MAC address
  char hostnameAsChar[hostname.length()+1];
  hostname.toCharArray(hostnameAsChar, hostname.length()+1);
  connectToWifi(hostnameAsChar, SSID, PSK);
  nextPeriod = millis();
}

void loop() {
  if (millis() >= nextPeriod) {
    // Subtract the last reading from the total
    total = total - readings[readIndex];

    // Read the sensor and add the new reading
    readings[readIndex] = analogRead(0);

    // Add the new reading to the total
    total = total + readings[readIndex];

    // Move to the next index in the array
    readIndex = (readIndex + 1) % numReadings;

    // Calculate the average
    average = total / numReadings;

    // Check for tolerance and send data if there's a significant change
    if (average < TOLERANCE) average = 0;
    if (average != lastValue) {
      if (average < lastValue - TOLERANCE || average > lastValue + TOLERANCE) {
        #ifdef SHOCK_THREESHOLD
        isShock = false;
        if (average - lastValue > SHOCK_THREESHOLD) isShock = true;
        #endif
        sendData(targetIP, oscOutPort, average);
        nextPeriod = millis() + MAX_RATE;
        lastValue = average;
      }
    }
  }

  yield();
  ArduinoOTA.handle();
}

void sendData(IPAddress targetIP, const uint16_t port, uint16_t value) {
  debugPrint("sending data "); debugPrintln(value);
  String oscAddress = "/" + hostname;
  char oscAddressChar[oscAddress.length() + 1];
  oscAddress.toCharArray(oscAddressChar, oscAddress.length() + 1);
  OSCMessage* message = new OSCMessage(oscAddressChar);
  message->add((float) value / 1023.0f);

  #ifdef SHOCK_THREESHOLD
  if (isShock) message->add((float) value / 1023.0f);
  debugPrintln("shock");
  #endif

  sendOsc(message, targetIP, port);
  delete(message);
}

void sendOsc(OSCMessage *msg, IPAddress ip, const uint16_t port) {
  udpserver.beginPacket(ip, port);
  msg->send(udpserver);
  udpserver.endPacket();
  ESP.wdtFeed();
  yield();
}

void connectToWifi(const char *Hostname, const char* ssid, const char* passphrase) {
  RGBled(0, 0, 1); // blue
  while (true) {
    debugPrintln("\n\nConnecting to " + String(ssid) + " ...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, passphrase);
    WiFi.hostname(Hostname);
    ESP.wdtFeed();
    yield();
    if (WiFi.waitForConnectResult() == WL_CONNECTED) {break;}
  }
  RGBled(0, 1, 0); // green
  debugPrint("... connected as ");
  debugPrint(Hostname);
  debugPrint(" at ");
  debugPrintln(WiFi.localIP());
  ArduinoOTA.setPort(8266); // default OTA port
  ArduinoOTA.setHostname(Hostname);
  ArduinoOTA.begin();
}

void RGBled(bool red, bool green, bool blue) {
  digitalWrite(rgbPins[0], red);
  digitalWrite(rgbPins[1], green);
  digitalWrite(rgbPins[2], blue);
}
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <OSCMessage.h>

//#define SERIAL_DEBUG // if defined debug messages will be printed via the serial port @115200 bauds
#define MAX_RATE 10 // minimum time between two OSC messages (in milliseconds). If set to high or too low (<30ms), some latency will appear
#define THIS_BB_NAME "btn" // name of this brutbox, will be used in OSC path and hostname

#ifdef SERIAL_DEBUG
  #define debugPrint(x)  Serial.print (x)
  #define debugPrintln(x)  Serial.println (x)
#else
  #define debugPrint(x)
  #define debugPrintln(x)
#endif
#define OSCPREFIX(x) x"_"

struct button {
  String name;
  uint8_t pin;
  bool lastState;
  bool currentState;
  long lastTriggered;
};

const String MACaddress = WiFi.macAddress().substring(9); // remove the manufacturer ID (first 9 characters) from the MAC
String hostname = OSCPREFIX(THIS_BB_NAME)+MACaddress;
static const char* PSK = "brutbox123";
static const char* SSID = "brutbox";
static const uint16_t oscOutPort = 8000;
IPAddress targetIP = IPAddress({10,0,0,255});
static const uint8_t rgbPins[] = {D8, D7, D6};
static const uint8_t debounce = 10; // in milliseconds
unsigned long nextPeriod = 0;

// multiple buttons in one brutbox
/*
button buttons[] = { button {"fish", D1 , LOW, HIGH, millis()},
                     button {"dragon", D2 , LOW, HIGH, millis()},
                     button {"bird", D5 , LOW, HIGH, millis()},
                     button {"elephant", D6 , LOW, HIGH, millis()} 
            };
*/

// single button on pin 0
button buttons[] = {button {"button", 0, LOW, HIGH, millis()}};

uint8_t buttonsCount = sizeof(buttons)/sizeof(button);

WiFiUDP udpserver;

void setup() {
  #ifdef SERIAL_DEBUG
  Serial.begin(115200);
  #endif
  for (uint8_t i=0; i<3; i++) {pinMode(rgbPins[i], OUTPUT);}
  hostname.replace(":", ""); // remove the : from the MAC address
  char hostnameAsChar[hostname.length()+1];
  hostname.toCharArray(hostnameAsChar, hostname.length()+1);
  connectToWifi(hostnameAsChar, SSID, PSK);
  for (uint8_t i=0; i<buttonsCount; i++) { pinMode(buttons[i].pin, INPUT_PULLUP); }
}

void loop() {
  if (millis() >= nextPeriod) {
    for (uint8_t i = 0; i < buttonsCount; i++) {
      bool reading = digitalRead(buttons[i].pin);
      long currentTime = millis();

      // Check if the button state has changed
      if (reading != buttons[i].lastState) {
        // Reset the debounce timer
        buttons[i].lastTriggered = currentTime;
      }

      // Only update the current state if the state has been stable for debounce period
      if ((currentTime - buttons[i].lastTriggered) > debounce) {
        // If the button state has changed (debounced), send OSC message
        if (reading != buttons[i].currentState) {
          buttons[i].currentState = reading;
          sendData(targetIP, oscOutPort, i);
          nextPeriod = millis() + MAX_RATE;
          debugPrintln("sent status for button " + buttons[i].name);
        }
      }
      
      buttons[i].lastState = reading;
    }
  }
  ArduinoOTA.handle();
  yield();
}

void sendData(IPAddress targetIP, const uint16_t port, uint8_t buttonIndex) {
  String oscAddress = "";
  if (buttonsCount > 1) oscAddress = "/"+hostname+"/"+buttons[buttonIndex].name;
  else oscAddress = "/"+hostname;
  char oscAddressChar[oscAddress.length()+1];
  oscAddress.toCharArray(oscAddressChar, oscAddress.length()+1);
  OSCMessage* message = new OSCMessage(oscAddressChar);
  message->add((float) !buttons[buttonIndex].currentState); // inverted because of the pullups
  sendOsc(message, targetIP, port);
  delete(message);
  yield();
}

void sendOsc(OSCMessage *msg, IPAddress ip, const uint16_t port ){
    udpserver.beginPacket(ip, port);
    msg->send(udpserver);
    udpserver.endPacket();
    ESP.wdtFeed();
    yield();
}

void connectToWifi(const char *Hostname, const char* ssid, const char* passphrase) {
  RGBled(0,0,1); // blue
  while (true) {
    debugPrintln("\n\nConnecting to " + String(ssid) + " ...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, passphrase);
    WiFi.hostname(Hostname);
    ESP.wdtFeed();
    yield();
    if ( WiFi.waitForConnectResult() == WL_CONNECTED ) {break;}
  }
  RGBled(0,1,0); // green
  debugPrintln("... connected");
  ArduinoOTA.setPort(8266); //default OTA port
  ArduinoOTA.setHostname(Hostname);// No authentication by default, can be set with : ArduinoOTA.setPassword((const char *)"passphrase");
  ArduinoOTA.begin();
 }

 void RGBled(bool red, bool green, bool blue) {
  digitalWrite(rgbPins[0], red);
  digitalWrite(rgbPins[1], green);
  digitalWrite(rgbPins[2], blue);
 }

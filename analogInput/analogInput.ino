#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <OSCMessage.h>

//#define SERIAL_DEBUG
//#define SHOCK_THREESHOLD 154
//#define MAX_RATE 10
#define LOOP_DELAY 8 // keep this > 2 to avoid malformed OSC messages

#ifdef SERIAL_DEBUG
  #define debugPrint(x)  Serial.print (x)
  #define debugPrintln(x)  Serial.println (x)
#else
  #define debugPrint(x)
  #define debugPrintln(x)
#endif

const String MACaddress = WiFi.macAddress().substring(9); // get the unique MAC address of this ESP to avoid conflicting names, remove the manufacturer ID (first 9 characters) from the MAC
String hostname = "potard_"+MACaddress; // 
static char* PSK = "malinette666";
//static char* PSK = "apacitron123";
static char* SSID = "malinette";
//static char* SSID = "rirififiloulou";
static const uint16_t oscOutPort = 8000;
IPAddress targetIP = IPAddress({10,0,0,255});
static const uint8_t rgbPins[] = {D8, D7, D6};
static const uint8_t tolerance = 30;
//long lastSent = 0;
uint16_t lastValue = 0;
WiFiUDP udpserver;
#ifdef SHOCK_THREESHOLD
bool isShock = false;
#endif

void setup() {
  #ifdef SERIAL_DEBUG
  Serial.begin(115200);
  #endif
  for (uint8_t i=0; i<3; i++) {pinMode(rgbPins[i], OUTPUT);}
  hostname.replace(":", ""); // remove the : from the MAC address
  char hostnameAsChar[hostname.length()+1];
  hostname.toCharArray(hostnameAsChar, hostname.length()+1);
  connectToWifi(hostnameAsChar, SSID, PSK);
}

void loop() {
  //if (millis()-lastSent>MAX_RATE) {
    int currentValue = analogRead(0);
    //debugPrintln(currentValue);
    if (currentValue != lastValue) {
      if (currentValue < lastValue-tolerance || currentValue > lastValue+tolerance || currentValue < tolerance ) {
        #ifdef SHOCK_THREESHOLD
        isShock = false;
        if (currentValue - lastValue > SHOCK_THREESHOLD) isShock = true;
        #endif
        sendData(targetIP,oscOutPort, currentValue);
        //debugPrint("new value : "); debugPrintln(currentValue);
        lastValue = currentValue;
      }
    }
 // }
  yield();
  delay(LOOP_DELAY); // remove this delay and everything will fall apart
  ArduinoOTA.handle();
}

void sendData(IPAddress targetIP, const uint16_t port, uint16_t value) {
  debugPrint("sending data "); debugPrintln(value);
  String oscAddress = "/"+hostname;
  char oscAddressChar[oscAddress.length()+1];
  oscAddress.toCharArray(oscAddressChar, oscAddress.length()+1);
  OSCMessage* message = new OSCMessage(oscAddressChar);
  message->add((float) value/1023.0f);
  #ifdef SHOCK_THREESHOLD
  if (isShock) message->add((float) value/1023.0f);
  debugPrintln("shock");
  #endif
  sendOsc(message, targetIP, port);
  //message->empty();
  delete(message);
  //lastSent = millis();
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
  debugPrint("... connected as ");
  debugPrint(Hostname);
  debugPrint(" at ");
  debugPrintln(WiFi.localIP());
  ArduinoOTA.setPort(8266); //default OTA port
  ArduinoOTA.setHostname(Hostname);// No authentication by default, can be set with : ArduinoOTA.setPassword((const char *)"passphrase");
  ArduinoOTA.begin();
 }

 void RGBled(bool red, bool green, bool blue) {
  digitalWrite(rgbPins[0], red);
  digitalWrite(rgbPins[1], green);
  digitalWrite(rgbPins[2], blue);
 }

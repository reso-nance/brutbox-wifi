#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <OSCMessage.h>

struct button {
  String name;
  uint8_t pin;
  bool lastState;
  bool currentState;
  long lastTriggered;
};

const String MACaddress = WiFi.macAddress().substring(9); // remove the manufacturer ID (first 9 characters) from the MAC
String hostname = "digital_"+MACaddress;
static char* PSK = "zincZN30!";
static char* SSID = "ZINC FABLAB";
static const uint16_t oscOutPort = 8000;
IPAddress targetIP = IPAddress({10,0,0,255});
static const uint8_t rgbPins[] = {D8, D7, D6};
static const uint8_t debounce = 10; // in milliseconds

button buttons[] = { button {"fish", D1 , LOW, HIGH, millis()},
                     button {"dragon", D2 , LOW, HIGH, millis()},
                     button {"bird", D5 , LOW, HIGH, millis()},
                     button {"elephant", D6 , LOW, HIGH, millis()} 
            };
uint8_t buttonsCount = sizeof(buttons)/sizeof(button);

WiFiUDP udpserver;

void setup() {
  Serial.begin(115200);
  for (uint8_t i=0; i<3; i++) {pinMode(rgbPins[i], OUTPUT);}
  hostname.replace(":", ""); // remove the : from the MAC address
  char hostnameAsChar[hostname.length()+1];
  hostname.toCharArray(hostnameAsChar, hostname.length()+1);
  connectToWifi(hostnameAsChar, SSID, PSK);
  for (uint8_t i=0; i<buttonsCount; i++) { pinMode(buttons[i].pin, INPUT_PULLUP); }
}

void loop() {
   for (uint8_t i=0; i<buttonsCount; i++) {
    buttons[i].currentState = digitalRead(buttons[i].pin);
    long currentTime = millis();
    if (buttons[i].currentState != buttons[i].lastState && currentTime - buttons[i].lastTriggered > debounce) {
      buttons[i].lastTriggered =  currentTime;
      sendData(targetIP, oscOutPort, i);
      Serial.println("sent status for button "+buttons[i].name);
    }
    buttons[i].lastState = buttons[i].currentState;
   }
   ArduinoOTA.handle();
   yield();
   
}

void sendData(IPAddress targetIP, const uint16_t port, uint8_t buttonIndex) {
  String oscAddress = "/"+hostname+"/"+buttons[buttonIndex].name;
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
    Serial.println("\n\nConnecting to " + String(ssid) + " ...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, passphrase);
    WiFi.hostname(Hostname);
    ESP.wdtFeed();
    yield();
    if ( WiFi.waitForConnectResult() == WL_CONNECTED ) {break;}
  }
  RGBled(0,1,0); // green
  Serial.println("... connected");
  ArduinoOTA.setPort(8266); //default OTA port
  ArduinoOTA.setHostname(Hostname);// No authentication by default, can be set with : ArduinoOTA.setPassword((const char *)"passphrase");
  ArduinoOTA.begin();
 }

 void RGBled(bool red, bool green, bool blue) {
  digitalWrite(rgbPins[0], red);
  digitalWrite(rgbPins[1], green);
  digitalWrite(rgbPins[2], blue);
 }

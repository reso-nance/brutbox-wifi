#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <OSCMessage.h>

const String MACaddress = WiFi.macAddress();
const String hostname = "analog_"+MACaddress;
static char* PSK = "malinette666";
static char* SSID = "malinette";
static const uint16_t oscOutPort = 8000;
IPAddress targetIP = IPAddress({10,0,0,255});
static const uint8_t rgbPins[] = {D8, D7, D6};
static const uint8_t tolerance = 2;
uint16_t lastValue = 0;

WiFiUDP udpserver;

void setup() {
  Serial.begin(115200);
  for (uint8_t i=0; i<3; i++) {pinMode(rgbPins[i], OUTPUT);}
  char hostnameAsChar[hostname.length()+1];
  hostname.toCharArray(hostnameAsChar, hostname.length()+1);
  connectToWifi(hostnameAsChar, SSID, PSK);
}

void loop() {
  int currentValue = analogRead(0);
  //Serial.println(currentValue);
  if (currentValue < lastValue-tolerance || currentValue > lastValue+tolerance) {
    sendData(targetIP,oscOutPort, currentValue);
    //Serial.print("new value : "); Serial.println(currentValue);
    lastValue = currentValue;
  }
  yield();
  ArduinoOTA.handle();
}

void sendData(IPAddress targetIP, const uint16_t port, uint16_t value) {
  String oscAddress = "/"+hostname;
  char oscAddressChar[oscAddress.length()+1];
  oscAddress.toCharArray(oscAddressChar, oscAddress.length()+1);
  OSCMessage* message = new OSCMessage(oscAddressChar);
  message->add((float) value/1024.0f);
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

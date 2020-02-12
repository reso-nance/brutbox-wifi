/*  Copyright 2019 Reso-nance Numérique <laurent@reso-nance.org>
#  
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#  
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#  
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
#  MA 02110-1301, USA.
#  
#  For testing purposes, this sketch waits for a random int
#  between 0 and 1024 followed by "/n" to be sent to the serial port.
#  The esp8266 respond by sending an osc message back containing the same int.
#  Most of this code is keeped as similar as possible to the rest of the
#  brutbox-wifi code to ensure this simulation stays as close to production conditions as possible
*/

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <OSCMessage.h>

//#define MAX_RATE 10
#define debugPrint(x)  Serial.print (x)
#define debugPrintln(x)  Serial.println (x)
  
const String MACaddress = WiFi.macAddress().substring(9); // get the unique MAC address of this ESP to avoid conflicting names, remove the manufacturer ID (first 9 characters) from the MAC
String hostname = "pression_"+MACaddress; // 
static char* PSK = "malinette666";
//static char* PSK = "apacitron123";
static char* SSID = "malinette";
//static char* SSID = "rirififiloulou";
static const uint16_t oscOutPort = 8000;
IPAddress targetIP = IPAddress({10,0,0,75});
static const uint8_t rgbPins[] = {D8, D7, D6};
static const uint8_t tolerance = 10;
//long lastSent = 0;
uint16_t lastValue = 0;
WiFiUDP udpserver;

void setup() {
  Serial.begin(115200);
  for (uint8_t i=0; i<3; i++) {pinMode(rgbPins[i], OUTPUT);}
  hostname.replace(":", ""); // remove the : from the MAC address
  char hostnameAsChar[hostname.length()+1];
  hostname.toCharArray(hostnameAsChar, hostname.length()+1);
  connectToWifi(hostnameAsChar, SSID, PSK);
}

void loop() {
   while (Serial.available() > 0) {
     int code = Serial.parseInt();
     if (Serial.read() == '\n') {
      sendData(targetIP, oscOutPort, code);
      Serial.flush();
     }
     //processIncomingByte(Serial.read());
   }
  yield();
  delay(10); // remove this delay and everything will fall apart
  ArduinoOTA.handle();
}

void sendData(IPAddress targetIP, const uint16_t port, uint16_t value) {
  debugPrint("sending data "); debugPrintln(value);
  String oscAddress = "/"+hostname;
  char oscAddressChar[oscAddress.length()+1];
  oscAddress.toCharArray(oscAddressChar, oscAddress.length()+1);
  OSCMessage* message = new OSCMessage(oscAddressChar);
  message->add(value);
  sendOsc(message, targetIP, port);
  delete(message);
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

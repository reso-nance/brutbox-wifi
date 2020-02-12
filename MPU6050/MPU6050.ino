#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <OSCMessage.h>
#include "I2Cdev.h"
//#include <Wire.h>
#include "MPU6050_6Axis_MotionApps20.h"

#define XYZ
//#define RAW_ACCELEROMETER
//#define RAW_GYROSCOPE
//#define JERK

const String MACaddress = WiFi.macAddress();
const String hostname = "acc_"+MACaddress;
static char* PSK = "malinette666";
static char* SSID = "malinette";
static const uint16_t oscOutPort = 8000;
IPAddress targetIP = IPAddress({10,0,0,255});
static const uint8_t rgbPins[] = {D8, D7, D6};
static const uint8_t interruptPin = D5;
static const int16_t accelOffsets[] = {-1343, -1155, 1033};
static const int16_t gyroOffsets[] = {19, -27, 16};
static const float maxJerk = 50000; // used to scale the value between 0 and 1
static const float maxAcceleration = 0;
static const float maxAngularSpeed = 0;

WiFiUDP udpserver;
MPU6050 mpu;
uint16_t packetSize;
uint16_t fifoCount;
uint8_t fifoBuffer[64];
Quaternion q;
VectorFloat gravity;
float ypr[3];
int16_t ax, ay, az;
int16_t gx, gy, gz;
float jerk;
volatile bool mpuInterrupt = false;     // indicates whether MPU interrupt pin has gone high
uint8_t mpuIntStatus;   // holds actual interrupt status byte from MPU
bool dmpReady = false;  // set true if DMP init was successful
void initialiseMPU6050(const int16_t a[3], const int16_t b[3]);
void dmpDataReady();

void setup() {
  Serial.begin(115200);
  for (uint8_t i=0; i<3; i++) {pinMode(rgbPins[i], OUTPUT);}
  char hostnameAsChar[hostname.length()+1];
  hostname.toCharArray(hostnameAsChar, hostname.length()+1);
  connectToWifi(hostnameAsChar, SSID, PSK);
  initialiseMPU6050(gyroOffsets, accelOffsets);
}

void loop() {
   while (!mpuInterrupt && fifoCount < packetSize) {
        if (mpuInterrupt && fifoCount < packetSize) {
          fifoCount = mpu.getFIFOCount(); // try to get out of the infinite loop 
        }
        sendData(targetIP, oscOutPort);
        delay(1);
    }
    processMPU6050();
    ArduinoOTA.handle();
}


void IRAM_ATTR dmpDataReady() {
    mpuInterrupt = true;
}

void IRAM_ATTR initialiseMPU6050(const int16_t gyroOffsets[3],const int16_t accelOffsets[3] ){
  Wire.begin();
    Wire.setClock(400000L);
    mpu.initialize();
    mpu.dmpInitialize();
    mpu.setXAccelOffset(accelOffsets[0]);
    mpu.setYAccelOffset(accelOffsets[1]);
    mpu.setZAccelOffset(accelOffsets[2]);
    mpu.setXGyroOffset(gyroOffsets[0]);
    mpu.setYGyroOffset(gyroOffsets[1]);
    mpu.setZGyroOffset(gyroOffsets[2]);
    mpu.setDMPEnabled(true);
    packetSize = mpu.dmpGetFIFOPacketSize();
    fifoCount = mpu.getFIFOCount();
    mpu.setDMPEnabled(true);
    // enable interrupt detection
    attachInterrupt(digitalPinToInterrupt(interruptPin), dmpDataReady, RISING);
    mpuIntStatus = mpu.getIntStatus();
    // set our DMP Ready flag so the main loop() function knows it's okay to use it
    Serial.println(F("DMP ready! Waiting for first interrupt..."));
    dmpReady = true;
    packetSize = mpu.dmpGetFIFOPacketSize();// get expected DMP packet size for later comparison
}

void processMPU6050(){
  mpuInterrupt = false; // reset interrupt flag and get INT_STATUS byte
    mpuIntStatus = mpu.getIntStatus();
    fifoCount = mpu.getFIFOCount(); // get current FIFO count
    
    // check for overflow (this should never happen unless our code is too inefficient)
    if ((mpuIntStatus & _BV(MPU6050_INTERRUPT_FIFO_OFLOW_BIT)) || fifoCount >= 1024) {
        // reset so we can continue cleanly
        mpu.resetFIFO();
        fifoCount = mpu.getFIFOCount();
        Serial.println(F("FIFO overflow!"));

    // otherwise, check for DMP data ready interrupt (this should happen frequently)
    } else if (mpuIntStatus & _BV(MPU6050_INTERRUPT_DMP_INT_BIT)) {
        // wait for correct available data length, should be a VERY short wait
        while (fifoCount < packetSize) fifoCount = mpu.getFIFOCount();

        mpu.getFIFOBytes(fifoBuffer, packetSize);// read a packet from FIFO
        
        // track FIFO count here in case there is > 1 packet available
        // (this lets us immediately read more without waiting for an interrupt)
        fifoCount -= packetSize;
        mpu.dmpGetQuaternion(&q,fifoBuffer);
        mpu.dmpGetGravity(&gravity,&q);
        mpu.dmpGetYawPitchRoll(ypr,&q,&gravity);    
        mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
        jerk = (abs(gx)+abs(gy)+abs(gz))/maxJerk;
        if (jerk > 1) jerk=1.0f;
    }
}

void sendData(IPAddress targetIP, const uint16_t port) {
  
  #ifdef XYZ
  String oscAddress = "/"+hostname+"/XYZ";
  char oscAddressChar[oscAddress.length()+1];
  oscAddress.toCharArray(oscAddressChar, oscAddress.length()+1);
  OSCMessage* message = new OSCMessage(oscAddressChar);
  for (uint8_t i=0; i<3; i++) {
    float value = (float) (ypr[i]*180/PI);
    value = (180.0f + value) /360.0f;
    message->add((float) value);}
  sendOsc(message, targetIP, port);
  delete(message);
  yield();
  #endif

  #ifdef RAW_ACCELEROMETER
  String oscAddress = "/"+hostname+"/accelXYZ";
  char oscAddressChar[oscAddress.length()+1];
  oscAddress.toCharArray(oscAddressChar, oscAddress.length()+1);
  OSCMessage* message = new OSCMessage(oscAddressChar);
  message->add(ax);
  message->add(ay);
  message->add(az);
  sendOsc(message, targetIP, port);
  delete(message);
  yield;
  #endif
  
  #ifdef RAW_GYROSCOPE
  String oscAddress = "/"+hostname+"/gyroXYZ";
  char oscAddressChar[oscAddress.length()+1];
  oscAddress.toCharArray(oscAddressChar, oscAddress.length()+1);
  OSCMessage* message = new OSCMessage(oscAddressChar);
  message->add(gx);
  message->add(gy);
  message->add(gz);
  sendOsc(message, targetIP, port);
  delete(message);
  yield;
  #endif
    
  #ifdef JERK
  String oscAddress = "/"+hostname+"/secousses";//50000
  char oscAddressChar[oscAddress.length()+1];
  oscAddress.toCharArray(oscAddressChar, oscAddress.length()+1);
  OSCMessage* message = new OSCMessage(oscAddressChar);
  message->add((float) jerk);
  sendOsc(message, targetIP, port);
  delete(message);
  yield;
  #endif
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

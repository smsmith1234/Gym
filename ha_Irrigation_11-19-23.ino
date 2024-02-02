// Get ESP8266 going with Arduino IDE
// - https://github.com/esp8266/Arduino#installing-with-boards-manager
// Required libraries (sketch -> include library -> manage libraries)
// - PubSubClient by Nick ‘O Leary
// - DHT sensor library by Adafruit

//OTA - MQTT
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
/********************************************************************/
//Display and sensor
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define wifi_ssid "Lajes"
#define wifi_password "S&R@kingdom21"

#define mqtt_server "192.168.1.125"
#define mqtt_user "homeassistant"
#define mqtt_password "Fah9BohquuNiXa1ieCh3pohquohsh6thee6ii7ja3xee4eexie7EeHen4OoX6dee"

#define temperature_topic "sensor/outdoor/temperature"
#define pressure_topic "sensor/outdoor/pump/pressure"
#define IP_topic "sensor/outdoor/IP"
#define RSSI_topic "sensor/outdoor/RSSI"
#define MAC_topic "sensor/outdoor/MAC"
#define SSID_topic "sensor/outdoor/SSID"

#define MIN_PRESSURE 10
#define LP_TURN_ON 20


#define ONE_WIRE_BUS 3
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

Adafruit_SSD1306 display(-1);
WiFiClient espClient;
PubSubClient client(espClient);
String strTopic;
String strPayload;
byte sr = 0b00000000;
int latchPin = 15; // pin D8 on NodeMCU boards
int clockPin = 14; // pin D5 on NodeMCU boards
int dataPin = 13;  // pin D7 on NodeMCU
long lastMsg = 0;
float temp1 = 0.0;
float diff = 0.1;
const int analogInPin = A0;
int sensorValue = 0;
//int lastDisplayed = 0;
long lastDisplayUpdateTime = 0;
int maxPressure = 0;
int pressure = 0;
int HPCutOff = 0;
int pumpStatus = 0;
boolean autoRun = false;
/********************************************************************/
void setup() {
  Serial.begin(9600);
  pinMode(latchPin, OUTPUT);
  pinMode(dataPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(ONE_WIRE_BUS, INPUT);
  delay(100);
  updateShiftRegister();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  sensors.begin();

  setup_wifi();
  ArduinoOTA.begin();
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}
/**********************************************************************************/
void loop() {
  ArduinoOTA.handle();
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  long now = millis();
  if (now - lastMsg >= 1000) {
    lastMsg = now;
    publishTemp();
    publishPressure();
    publishRelayStatus();
    client.publish(SSID_topic, String(WiFi.SSID()).c_str(), true);
    //updateDisplay();
    displaySR();
  }
  if (autoRun) {
    autoPressure();
  }
  long displayUpdateTime = millis();
/*  if (displayUpdateTime - lastDisplayUpdateTime >= 1000) {
    lastDisplayUpdateTime = displayUpdateTime;
    updateDisplay;
  }*/
}
/**********************************************************************************/
void callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  strTopic = String((char*)topic);

  for (int i = 0; i < 8; i++) {
    String myTopic = "ha/switch";
    myTopic += i;
    if (strTopic == myTopic) {
      if (String((char*)payload) == "ON") {
        bitSet(sr, i);
      }
      else {
        bitClear(sr, i);
      }
    }
  }
  updateShiftRegister();

  if (strTopic == "ha/switch_auto") {
    if (String((char*)payload) == "ON") {
      autoRun = true;
      client.publish("irrigation/auto_state", "ON", true);
    }
    if (String((char*)payload) == "OFF") {
      autoRun = false;
      client.publish("irrigation/auto_state", "OFF", true);
    }
  }
}
/**********************************************************************************/
String getIrrigationStatus() {
  String myStatus = "Standby";
  byte myBit;
  for (int i = 0; i <= 8  ; i++) {
    myBit = bitRead(sr, i);
    switch (i) {
      case 0:
        if (myBit) {
          myStatus = "Zone ";
          break;
        }
      case 1:
        if (myBit) {
          myStatus += "1 ";
          break;
        }
      case 2:
        if (myBit) {
          myStatus += "2 ";
          break;
        }
      case 3:
        if (myBit) {
          myStatus += "3 ";
          break;
        }
      case 4:
        if (myBit) {
          myStatus += "4 ";
          break;
        }
      case 5:
        if (myBit) {
          myStatus += "5 ";
          break;
        }
      case 6:
        if (myBit) {
          myStatus += "6 ";
          break;
        }
      case 7:
        if (myBit) {
          myStatus += "7 ";
          break;
        }
    }
    if (autoRun) {
      myStatus = "Auto";
    }
  }
  return (myStatus);
}
/**********************************************************************************/
void updateShiftRegister() {
  digitalWrite(latchPin, LOW);
  shiftOut(dataPin, clockPin, LSBFIRST, ~sr);
  digitalWrite(latchPin, HIGH);
}
/**********************************************************************************/
bool checkBound(float newValue, float prevValue, float maxDiff) {
  return !isnan(newValue) &&
         (newValue < prevValue - maxDiff || newValue > prevValue + maxDiff);
}
/**********************************************************************************/
void displayTemp() {
  sensors.requestTemperatures();
  float temperatureF = sensors.getTempFByIndex(0);
  display.setTextSize(2);
  //display.clearDisplay();
  display.setCursor(0, 42);
  display.print(String(temperatureF).c_str());
  display.print(" F");
  display.display();
}
/**********************************************************************************/
void publishTemp() {
  sensors.requestTemperatures();
  float temperatureF = sensors.getTempFByIndex(0);
  client.publish(temperature_topic, String(temperatureF).c_str(), true);
}
/**********************************************************************************/
void displayPressure() {
  sensorValue = analogRead(analogInPin);
  sensorValue = map(sensorValue, 100, 700, 0, 55);
  //display.clearDisplay();
  //display.display();
  display.setCursor(0, 21);
  display.print(sensorValue);
  display.print(" PSI");
  display.display();
}
/**********************************************************************************/
void publishPressure() {
  sensorValue = analogRead(analogInPin);
  sensorValue = map(sensorValue, 100, 700, 0, 55);
  client.publish(pressure_topic, String(sensorValue).c_str(), true);
}
/**********************************************************************************/
void displayStatus() {
  display.setTextSize(2);
  //display.clearDisplay();
  display.setCursor(0, 0);
  display.print(getIrrigationStatus().c_str());
  display.display();
}
/**********************************************************************************/
void publishRelayStatus() {
  for (int i = 0; i < 8; i++) {
    String mySensorTopic = "irrigation/relay_";
    mySensorTopic += i;
    mySensorTopic += "_state";
    if ((bitRead(sr, i)) == 1) {
      client.publish(mySensorTopic.c_str() , "ON", true);
    }
    else {
      client.publish(mySensorTopic.c_str() , "OFF", true);
    }
  }
}
/**********************************************************************************/
void updateDisplay() {
  display.clearDisplay();
  displayTemp();
  displayPressure();
  displayStatus();
  display.display();
  /*  if (lastDisplayed >= 4) {
      lastDisplayed = 0;
    }
    switch (lastDisplayed) {
      case 0:
        display.clearDisplay();
        display.display();
        displayTemp();
        lastDisplayed ++;
        break;
      case 1:
        displayPressure();
        lastDisplayed ++;
        break;
      case 2:
        displayStatus();
        lastDisplayed ++;
        break;
      case 3:
        displayPumpDetails();
        lastDisplayed ++;
        break;
    }*/
}
/**********************************************************************************/
void autoPressure() {

  if (pumpStatus == 0) {
    maxPressure = getMaxPressure();
    HPCutOff = static_cast<int>(static_cast<float>(maxPressure) * .9);
  }
  pressure = GetPressure();

  if (pressure > maxPressure) {
    sr = 0b00000000;
    updateShiftRegister();
    pumpStatus = 5;  // Overpressure error - turn off pump
  }
  else if (pressure <= MIN_PRESSURE) {
    sr = 0b00000000;
    updateShiftRegister();
    pumpStatus = 4;  // Underpressure error - turn off pump
  }
  else if (pressure >= HPCutOff) {
    sr = 0b00000000;
    updateShiftRegister();
    pumpStatus = 3;  // Pressure above cut off - turn off pump
  }
  else if (pressure <= LP_TURN_ON) { //
    sr = 0b00000001;
    updateShiftRegister();
    delay(10000);  // Let pump run for 10 seconds
    pumpStatus = 2;  // Pressure below cut on - turn on pump
  }
  else if (pressure > LP_TURN_ON && pressure < HPCutOff) {
    pumpStatus = 1;  // Pressure good - no change to pump
  }
  else {
    sr = 0b00000000;
    updateShiftRegister();
    pumpStatus = 6;  // Undetermined error - turn off pump
  }
  
}
/**********************************************************************************/
int getMaxPressure() {
  display.setTextSize(1);
  display.clearDisplay();
  display.setCursor(0, 28);
  display.print("Max Pressure ");
  sr = 0b00000001;
  displaySR();
  updateShiftRegister();
  delay(10000);
  sr = 0b00000000;
  displaySR();
  updateShiftRegister();
  delay(5000);
  display.print(map(analogRead(analogInPin), 100, 700, 0, 55));
  display.display();
  return map(analogRead(analogInPin), 100, 700, 0, 55);
}
/**********************************************************************************/
void displayPumpDetails() {
  display.setTextSize(2);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Pump -");
  display.setCursor(0, 28);
  display.setTextSize(1);
  display.print("Pump status: ");
  display.println(pumpStatus);
  display.print("Max pres: ");
  display.println(maxPressure);
  display.print("HPCO pres: ");
  display.println(HPCutOff);
  display.display();
}
/**********************************************************************************/
void displaySR() {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
  display.print("SR: ");
  display.print(sr);
  display.display();
}
/**********************************************************************************/
int GetPressure() {
  return map(analogRead(analogInPin), 100, 700, 0, 55);
  //inputValue = analogRead(inputPin);
  //PSI = (inputValue - 102) / 15.367;
  //return static_cast<int>(PSI);
  //return inputValue;
}
/**********************************************************************************/
void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);

  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.println(WiFi.localIP());
    display.setCursor(0, 28);
    display.print("WIFI");
    display.println("connecting");
    display.display();
    delay(500);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println(WiFi.localIP());
  display.setCursor(0, 28);
  display.print(WiFi.RSSI());
  display.print(" dBm");
  display.setCursor(0, 56);
  display.print(wifi_ssid);
  display.display();
  delay(1000);
  display.clearDisplay();
  display.display();
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}
/**********************************************************************************/
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client10", mqtt_user, mqtt_password)) {
      Serial.println("connected");
      publishNetworkData();
      // Once connected, publish an announcement...
      client.subscribe("ha/#", 1);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
/**********************************************************************************/
void publishNetworkData() {
  client.publish(IP_topic, WiFi.localIP().toString().c_str(), true);
  client.publish(RSSI_topic, String(WiFi.RSSI()).c_str(), true);
  client.publish(MAC_topic, String(WiFi.macAddress()).c_str(), true);
  client.publish(SSID_topic, String(WiFi.SSID()).c_str(), true);
}
/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/
/*
  // function to print a device address
  void printAddress(DeviceAddress deviceAddress) {
  for (uint8_t i = 0; i < 8; i++) {
    if (deviceAddress[i] < 16) Serial.print("0");
    display.print(deviceAddress[i], HEX);
  }
  }

  void detectI2C ()
  { // Grab a count of devices on the wire
  // Number of temperature devices found
  int numberOfDevices;

  // We'll use this variable to store a found device address
  DeviceAddress tempDeviceAddress;
  numberOfDevices = sensors.getDeviceCount();
  // locate devices on the bus
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.print("Loc devices:");
  //display.print("Found ");
  display.print(numberOfDevices, DEC);
  display.display();
  delay(5000);
  display.clearDisplay();
  display.display();
  //display.println(" devices.");
  // Loop through each device, print out address
  for (int i = 0; i < numberOfDevices; i++) {
    // Search the wire for address
    if (sensors.getAddress(tempDeviceAddress, i)) {
      //display.print("Found device ");
      display.print(i, DEC);
      //display.print(" with address: ");
      printAddress(tempDeviceAddress);
      display.println();
      display.display();
      delay(5000);
      display.clearDisplay();
      display.display();
    } else {
      //display.print("Found ghost device at ");
      //display.print(i, DEC);
      //display.print(" but could not detect address. Check power and cabling");
    }
  }
  }
*/
/**********************************************************************************/

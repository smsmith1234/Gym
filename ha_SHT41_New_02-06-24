#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_SHT4x.h>

#define wifi_ssid "Lajes"
#define wifi_password "S&R@kingdom21"
#define mqtt_server "192.168.1.125"
#define mqtt_user "homeassistant"
#define mqtt_password "Fah9BohquuNiXa1ieCh3pohquohsh6thee6ii7ja3xee4eexie7EeHen4OoX6dee"
#define humidity_topic "sensor/sensorName/humidity"
#define temperature_topic "sensor/sensorName/temperature"
#define lux_topic "sensor/sensorName/lux"
#define IP_topic "sensor/sensorName/IP"
#define RSSI_topic "sensor/sensorName/RSSI"
#define MAC_topic "sensor/sensorName/MAC"
#define SSID_topic "sensor/sensorName/SSID"

#define DEBUG_PRINT_ENABLE true

Adafruit_SSD1306 display(128, 64, &Wire, -1);
Adafruit_SHT4x sht4 = Adafruit_SHT4x();
WiFiClient espClient;
PubSubClient client(espClient);

String myMAC = "";
long lastMsg = 0;
float oldTemp = 0.0;
float newTemp = 0.0;
float oldHum = 0.0;
float oldLux = 0.0;
float diff = 0.05;
float luxDiff = 1.0;
float newLux = 0.0;
float oldRSSI = 0.0;
float newRSSI = 0.0;
float RSSIDiff = 3.0;

// Helper function for debug printing
void debugPrint(String message) {
  if (DEBUG_PRINT_ENABLE) {
    Serial.println(message);
  }
}

void setup() {
  Serial.begin(115200);
  debugPrint("Initializing...");

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  debugPrint("Display initialized");

  client.setServer(mqtt_server, 1883);
  debugPrint("MQTT server set");

  sht4.begin();
  sht4.setPrecision(SHT4X_MED_PRECISION);
  sht4.setHeater(SHT4X_LOW_HEATER_100MS);
  debugPrint("SHT4x sensor initialized");

  delay(2000); // Pause for 2 seconds
  debugPrint("Waiting...");

  // Clear the buffer
  display.clearDisplay();
  debugPrint("Display buffer cleared");

  // Display "Hello World" message
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("Hello World");
  display.display();
  debugPrint("Hello World displayed");

  delay(2000);
  display.clearDisplay();
  debugPrint("Display cleared");

  connectToWifi();
  debugPrint("Wi-Fi connected");

  ArduinoOTA.begin();
  debugPrint("OTA initialized");

  ArduinoOTA.onStart([]() {
    debugPrint("Start updating...");
  });
  ArduinoOTA.onEnd([]() {
    debugPrint("Update complete");
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
}

void loop() {
  ArduinoOTA.handle();

  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 15000) {
    lastMsg = now;

    sensors_event_t humidity, temp;
    sht4.getEvent(&humidity, &temp);
    newTemp = (((temp.temperature * 9) / 5) + 32);
    float newHum = (humidity.relative_humidity);

    debugPrint("Temperature: " + String(newTemp));
    debugPrint("Humidity: " + String(newHum));

    if (valueHasChanged(newRSSI, oldRSSI, RSSIDiff)) {
      oldRSSI = newRSSI;
      client.publish(RSSI_topic, String(WiFi.RSSI()).c_str(), true);
      debugPrint("RSSI: " + String(WiFi.RSSI()));
    }
    if (valueHasChanged(newTemp, oldTemp, diff)) {
      oldTemp = newTemp;
      displayTemp();
      client.publish(temperature_topic, String(newTemp).c_str(), true);
    }
    if (valueHasChanged(newHum, oldHum, diff)) {
      oldHum = newHum;
      client.publish(humidity_topic, String(newHum).c_str(), true);
    }
  }
}

void connectToWifi() {
  while (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(wifi_ssid, wifi_password);
    if (WiFi.status() == WL_CONNECTED) {
      displayWifiConnectionData();
      myMAC = WiFi.macAddress();
      myMAC.replace(":", "");
      myMAC = myMAC.substring(6);
    } else {
      delay(5000);
    }
  }
}

void reconnectMQTT() {
  // Loop until we're reconnected
  while (!client.connected()) {
    String myClient = "ESP-" + myMAC;
    if (client.connect(myClient.c_str(), mqtt_user, mqtt_password)) {
      publishNetworkData();
    } else {
      delay(5000);
    }
  }
}

bool valueHasChanged(float newValue, float prevValue, float maxDiff) {
  float outOfBoundDiff = 1;
  if (prevValue == 0) {
    return !isnan(newValue) && (newValue < prevValue - maxDiff || newValue > prevValue + maxDiff);
  } else {
    return !isnan(newValue) && (newValue < prevValue - maxDiff || newValue > prevValue + maxDiff) && (fabs(newValue - prevValue) < outOfBoundDiff);
  }
}

void publishNetworkData() {
  client.publish(IP_topic, WiFi.localIP().toString().c_str(), true);
  client.publish(RSSI_topic, String(WiFi.RSSI()).c_str(), true);
  client.publish(MAC_topic, String(WiFi.macAddress()).c_str(), true);
  client.publish(SSID_topic, String(WiFi.SSID()).c_str(), true);
}

void displayTemp() {
  display.setTextSize(2);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print(String(newTemp).c_str());
   display.print(" F");
  display.display();
}
void displayWifiConnectionData(){
 display.setTextSize(2);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println(WiFi.localIP());
  display.setCursor(0, 28);
  display.print(WiFi.RSSI());
  display.print(" dBm");
  display.display();
}

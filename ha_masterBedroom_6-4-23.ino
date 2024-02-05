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
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_AHTX0.h>
#include <hp_BH1750.h>

#define wifi_ssid "Lajes"
#define wifi_password "S&R@kingdom21"

#define mqtt_server "192.168.1.125"
#define mqtt_user "homeassistant"
#define mqtt_password "Fah9BohquuNiXa1ieCh3pohquohsh6thee6ii7ja3xee4eexie7EeHen4OoX6dee"

#define humidity_topic "sensor/masterBedroom/humidity"
#define temperature_topic "sensor/masterBedroom/temperature"
#define lux_topic "sensor/masterBedroom/lux"
#define IP_topic "sensor/masterBedroom/IP"
#define RSSI_topic "sensor/masterBedroom/RSSI"
#define MAC_topic "sensor/masterBedroom/MAC"
#define SSID_topic "sensor/masterBedroom/SSID"

/*#define DHTTYPE DHT22
  #define DHTPIN  14
  DHT dht(DHTPIN, DHTTYPE, 11); // 11 works fine for ESP8266*/
Adafruit_SSD1306 display(-1);
Adafruit_AHTX0 aht;
hp_BH1750 BH1750;
WiFiClient espClient;
PubSubClient client(espClient);
const int buzzerPin =  14;
String myMAC = "";
String strTopic;
/********************************************************************/
void setup() {
  Serial.begin(115200);
  pinMode(buzzerPin, OUTPUT);
  //dht.begin();
  if (! aht.begin()) {
    Serial.println("Could not find AHT? Check wiring");
    while (1) delay(10);
  }
  Serial.println("AHT10 or AHT20 found");
  bool avail = BH1750.begin(BH1750_TO_GROUND);
  BH1750.calibrateTiming();
  BH1750.start();

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  setup_wifi();
  myMAC = WiFi.macAddress();
  Serial.println(myMAC);
  myMAC.replace(":", "");
  myMAC = myMAC.substring(6);

  display.setCursor(0, 0);
  display.println(WiFi.localIP());
  display.setCursor(0, 28);
  display.print(WiFi.RSSI());
  display.print(" dBm");
  display.display();
  delay(10000);
  display.clearDisplay();
  display.display();
  ArduinoOTA.begin();
  /********************************************************************/
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

  /********************************************************************/
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
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
    delay(500);
    Serial.print(".");
  }

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
    // If you do not want to use a username and password, change next line to
    //if (client.connect("ESP8266Client")) {
    String myClient = "ESP-" + myMAC;
    Serial.println(myClient);
    if (client.connect(myClient.c_str(), mqtt_user, mqtt_password)) {
      Serial.println("connected");
      client.subscribe("ha/#", 1);
      publishNetworkData();
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
void callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0';
  strTopic = String((char*)topic);
  if (strTopic == "ha/masterBedroom/alarm") {
    if (String((char*)payload) == "ON") {
      digitalWrite(buzzerPin, HIGH);
      client.publish("sensor/masterBedroom/alarm_state", "ON", true);
    }
    if (String((char*)payload) == "OFF") {
      digitalWrite(buzzerPin, LOW);
      client.publish("sensor/masterBedroom/alarm_state", "OFF", true);
    }
  }
}
/**********************************************************************************/
bool checkBound(float newValue, float prevValue, float maxDiff) {
  //Serial.println((fabs(newValue - prevValue)));
  Serial.print("*");
  float outOfBoundDiff = 1;
  if (prevValue == 0) {
    return !isnan(newValue) && (newValue < prevValue - maxDiff || newValue > prevValue + maxDiff);
  }
  else {
    return !isnan(newValue) && (newValue < prevValue - maxDiff || newValue > prevValue + maxDiff) && (fabs(newValue - prevValue) < outOfBoundDiff);
  }
}
/**********************************************************************************/
bool checkBoundLux(float newValue, float prevValue, float maxDiff) {
  //Serial.println((fabs(newValue - prevValue)));
  Serial.print("*");
  return !isnan(newValue) && (newValue < prevValue - maxDiff || newValue > prevValue + maxDiff);
}
/**********************************************************************************/
void publishNetworkData() {
  client.publish(IP_topic, WiFi.localIP().toString().c_str(), true);
  client.publish(RSSI_topic, String(WiFi.RSSI()).c_str(), true);
  client.publish(MAC_topic, String(WiFi.macAddress()).c_str(), true);
  client.publish(SSID_topic, String(WiFi.SSID()).c_str(), true);
}
/**********************************************************************************/
long lastMsg = 0;
float oldTemp = 0.0;
float oldHum = 0.0;
float oldLux = 0.0;
float diff = 0.05;
float luxDiff = 1.0;
float newLux = 0.0;
float oldRSSI = 0.0;
float newRSSI = 0.0;
float RSSIDiff = 3.0;

/**********************************************************************************/
void loop() {
  ArduinoOTA.handle();
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 5000) {
    lastMsg = now;
    BH1750.start();
    newLux = BH1750.getLux();
    /*Serial.print("newLux");
      Serial.print(newLux);
      Serial.print("oldLux");
      Serial.println(oldLux);  */
    sensors_event_t humidity, temp;
    aht.getEvent(&humidity, &temp);
    float newTemp = (((temp.temperature * 9) / 5) + 32);
    float newHum = (humidity.relative_humidity);
    //float newTemp = dht.readTemperature(true);
    //float newHum = dht.readHumidity();

    if (checkBound(newRSSI, oldRSSI, RSSIDiff)) {
      oldRSSI = newRSSI;
      client.publish(RSSI_topic, String(WiFi.RSSI()).c_str(), true);
    }
    if (checkBound(newTemp, oldTemp, diff)) {
      oldTemp = newTemp;
      display.setTextSize(2);
      display.clearDisplay();
      display.setCursor(0, 0);
      display.println("New Temp:");
      display.setCursor(0, 28);
      display.print(String(newTemp).c_str());
      display.print(" F");
      display.display();
      Serial.println();
      Serial.print("New temperature:");
      Serial.println(String(newTemp).c_str());
      client.publish(temperature_topic, String(newTemp).c_str(), true);
    }
    if (checkBound(newHum, oldHum, diff)) {
      oldHum = newHum;
      Serial.println();
      Serial.print("New humidity:");
      Serial.println(String(newHum).c_str());
      client.publish(humidity_topic, String(newHum).c_str(), true);
    }
    if (checkBoundLux(newLux, oldLux, luxDiff)) {
      oldLux = newLux;
      Serial.println();
      Serial.print("New Lux:");
      Serial.println(String(newLux).c_str());
      client.publish(lux_topic, String(newLux).c_str(), true);
    }
  }
}

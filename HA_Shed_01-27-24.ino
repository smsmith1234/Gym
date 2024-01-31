// BIG SHED
// Get ESP8266 going with Arduino IDE
// - https://github.com/esp8266/Arduino#installing-with-boards-manager
// Required libraries (sketch -> include library -> manage libraries)
// - PubSubClient by Nick ‘O Leary
// - DHT sensor library by Adafruit

//OTA - MQTT
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
/********************************************************************/
//Display and sensor
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define wifi_ssid "Lajes"
#define wifi_password "S&R@kingdom21"

#define mqtt_server "192.168.1.125"
#define mqtt_user "homeassistant"
#define mqtt_password "Fah9BohquuNiXa1ieCh3pohquohsh6thee6ii7ja3xee4eexie7EeHen4OoX6dee"

#define humidity_topic "sensor/shed/humidity"
#define temperature_topic "sensor/shed/temperature"
#define IP_topic "sensor/shed/IP"
#define RSSI_topic "sensor/shed/RSSI"
#define MAC_topic "sensor/shed/MAC"
#define SSID_topic "sensor/shed/SSID"

#define DHTTYPE DHT22
#define DHTPIN  14
String myMAC = "";
byte flip = true;


DHT dht(DHTPIN, DHTTYPE, 11); // 11 works fine for ESP8266
Adafruit_SSD1306 display(-1);
//
ESP8266WiFiMulti wifiMulti;
WiFiClient espClient;
PubSubClient client(espClient);
/********************************************************************/
void setup() {
  Serial.begin(9600);
  dht.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  // Clear the buffer.
  display.clearDisplay();

  // Display Text
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 28);
  display.println("Hello world!");
  display.display();
  delay(2000);
  display.clearDisplay();
  wifiMulti.addAP("Lajes", "S&R@kingdom21");
  wifiMulti.addAP("Lajes_EXT", "S&R@kingdom21");
  wifiMulti.addAP("ATT2GXy9ZH", "S&R@kingdom21");
  while (wifiMulti.run() != WL_CONNECTED) { // Wait for the Wi-Fi to connect: scan for Wi-Fi networks, and connect to the strongest of the networks above
    delay(1000);

    if (flip == true) {
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(0, 0);
      display.print("Connecting");
      display.display();
      Serial.print('+');
      flip = false;
    }
    else {
      display.clearDisplay();
      display.display();
      Serial.print("-");
      flip = true;

    }
    Serial.print('.');
  }
  //wifiMulti.run();
  //delay(5000);
  myMAC = WiFi.macAddress();
  Serial.println(myMAC);
  delay(5000);
  myMAC.replace(":", "");
  myMAC = myMAC.substring(6);
  display.setCursor(0, 0);
  display.println(WiFi.localIP());
  display.setCursor(0, 28);
  display.print(WiFi.RSSI());
  display.print(" dBm");
  display.display();
  delay(1000);
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
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
}

/**********************************************************************************/

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
bool checkBound(float newValue, float prevValue, float maxDiff) {
  //Serial.println((fabs(newValue - prevValue)));
  float outOfBoundDiff = 1;
  if (prevValue == 0) {
    return !isnan(newValue) && (newValue < prevValue - maxDiff || newValue > prevValue + maxDiff);
  }
  else {
    return !isnan(newValue) && (newValue < prevValue - maxDiff || newValue > prevValue + maxDiff) && (fabs(newValue - prevValue) < outOfBoundDiff);
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
long lastMsg = 0;
float temp = 0.0;
float hum = 0.0;
float diff = 0.05;
float oldRSSI = 0.0;
float newRSSI = 0.0;
float RSSIDiff = 3.0;

/**********************************************************************************/
void loop() {
  while (wifiMulti.run() != WL_CONNECTED) { // Wait for the Wi-Fi to connect: scan for Wi-Fi networks, and connect to the strongest of the networks above
    delay(1000);

    if (flip == true) {
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(0, 0);
      display.print("Connecting");
      display.display();
      Serial.print('+');
      flip = false;
    }
    else {
      display.clearDisplay();
      display.display();
      Serial.print("-");
      flip = true;

    }
    Serial.print('.');
  }
  ArduinoOTA.handle();
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 5000) {
    lastMsg = now;
    display.setTextSize(1);
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println(WiFi.localIP());
    display.print(WiFi.SSID());
    //display.print("-");
    display.print(WiFi.RSSI());
    display.println(" dBm");

    float newTemp = dht.readTemperature(true);
    float newHum = dht.readHumidity();
    
    if (checkBound(newRSSI, oldRSSI, RSSIDiff)) {
      oldRSSI = newRSSI;
      client.publish(RSSI_topic, String(WiFi.RSSI()).c_str(), true);
    }
    //if (checkBound(newTemp, temp, diff)) {
    temp = newTemp;
    display.print("Temp:");
    display.print(String(temp).c_str());
    display.println(" F");

    Serial.print("New temperature:");
    Serial.println(String(temp).c_str());
    client.publish(temperature_topic, String(temp).c_str(), true);

    //if (checkBound(newHum, hum, diff)) {
    hum = newHum;

    display.print("RH:");

    display.print(String(hum).c_str());
    display.print(" %");
    display.display();
    Serial.print("New humidity:");
    Serial.println(String(hum).c_str());
    client.publish(humidity_topic, String(hum).c_str(), true);
    //}
  }
}

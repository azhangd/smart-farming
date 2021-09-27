#include <WiFiNINA.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <ArduinoJson.h>
#include "SCD30.h"
#include "AHT20.h"

AHT20 AHT;

 // Define credentials
const char* wifiSSID = "";
const char* wifiPassword = "";
const char* mqttServer = "";
const int mqttPort = 1883;
const char* mqttUsername = "";
const char* mqttPassword = "";
char device_id[18];

WiFiClient wifiClient;
PubSubClient client(wifiClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

void connectToMQTT() {
  // Loop until connected to MQTT
  while (!client.connected()) {
    if (client.connect("test", mqttUsername, mqttPassword)) {
      Serial.println("MQTT: Connected");
    } else {
      delay(5000);
    }
  }
}

void connectToWifi() {
  Serial.println("Connecting to " + String(wifiSSID));
  WiFi.begin(wifiSSID, wifiPassword);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("...");
    delay(1000);
  }
  Serial.println("WiFi: Connected");
  byte mac[6];
   // 12 digits, 5 colons, and a '\0' at the end
  WiFi.macAddress(mac);
  macToString(mac, device_id);
  Serial.print("Device ID: ");
  Serial.println(device_id);
}

void macToString(byte *mac, char *str) {
  for(int i = 0; i<6; i++) {
    byte digit;
    digit = (*mac >> 8) & 0xF;
    *str++ = (digit < 10 ? '0' : 'A'-10) + digit;
    digit = (*mac) & 0xF;
    *str++ = (digit < 10 ? '0' : 'A'-10) + digit;
    *str++ = ':';
    mac++;
  }
  // replace the final colon with a nul terminator
  str[-1] = '\0';
}

void setup() {
  Serial.begin(115200);
  connectToWifi();
  client.setServer(mqttServer, mqttPort);
  timeClient.begin();
  AHT.begin();
  scd30.initialize();
  scd30.setAutoSelfCalibration(1);
}

void loop() {
  if (!client.connected()) {
    connectToMQTT();
  }

  // Read and store AHT20 values
  float h;
  float t;
  AHT.getSensor(&h, &t);
  float humidity_percent_aht = h * 100;
  float temperature_f_aht = (t * 9.0) / 5.0 + 32;
  
  // Read and store SCD30 values
  float result[3] = {0};
  scd30.getCarbonDioxideConcentration(result);
  float co2_ppm = result[0];
  float temperature_f = (result[1] * 9.0) / 5.0 + 32;
  float humidity_percent = result[2];

  // Get epoch time from NTP Client
  timeClient.update();
  unsigned long epoch_time = timeClient.getEpochTime();

    //    {
    //      "time": epoch_time,
    //      "device_id": device_id,
    //      "weather_aht20": {
    //        "co2_ppm": result[0],
    //        "temperature_f": (result[1] * 9.0) / 5.0 + 32,
    //        "humidity_percent": result[2]
    //      },
    //      "weather_scd30": {
    //        "co2_ppm": result[0],
    //        "temperature_f": (result[1] * 9.0) / 5.0 + 32,
    //        "humidity_percent": result[2]
    //      }
    //    }
    
    StaticJsonDocument<256> message;
    message["time"] = epoch_time;
    message["device_id"] = device_id;

    JsonObject weather_aht20 = message.createNestedObject("weather_aht20");
    weather_aht20["temperature_f"] = temperature_f_aht;
    weather_aht20["humidity_percent"] = humidity_percent_aht;
    
    JsonObject weather_scd30 = message.createNestedObject("weather_scd30");
    weather_scd30["co2_ppm"] = co2_ppm;
    weather_scd30["temperature_f"] = temperature_f;
    weather_scd30["humidity_percent"] = humidity_percent;

    // Serialize message to a temporary buffer
    char buffer[256]; // Save a few CPU cycles by passing the size of the payload to publish()
    size_t n = serializeJson(message, buffer);

  // Publish message to MQTT topic
  if (client.publish("sensors/weather", buffer, n)) {
      Serial.print("[");
      Serial.print(device_id);
      Serial.println("] Message published successfully.");
  }

  // 5 second sleep
  delay(5000);
  client.loop();
}

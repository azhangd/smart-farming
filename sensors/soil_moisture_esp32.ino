#include <WiFi.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <ArduinoJson.h>

// Define credentials
const char* wifiSSID = "";
const char* wifiPassword = "";
const char* mqttServer = "";
const int mqttPort = 1883;
const char* mqttUsername = "";
const char* mqttPassword = "";

// Define analog pins
int soil_moisture_capacitive_pin = A0;
int soil_moisture_pin = A1;
int soil_moisture = 0;
int soil_moisture_capacitive = 0;

WiFiClient wifiClient;
PubSubClient client(wifiClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
String device_id;

void connectToMQTT() {
  // Loop until connected to MQTT
  while (!client.connected()) {
    if (client.connect(device_id.c_str(), mqttUsername, mqttPassword)) {
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
  device_id = WiFi.macAddress();
  Serial.println("Device ID: " + device_id);
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(10);
  connectToWifi();
  client.setServer(mqttServer, mqttPort);
  timeClient.begin();
}

void loop() {
  if (!client.connected()) {
    connectToMQTT();
  }

  // Read and store sensor values
  soil_moisture = analogRead(soil_moisture_pin);
  soil_moisture_capacitive = analogRead(soil_moisture_capacitive_pin);

  // Get epoch time from NTP Client
  timeClient.update();
  unsigned long epoch_time = timeClient.getEpochTime();

  //    {
  //      "time": epoch_time,
  //      "device_id": device_id,
  //      "soil": {
  //        "soil_moisture": analogRead(soil_moisture_pin),
  //        "soil_moisture_capacitive": analogRead(soil_moisture_capacitive_pin)
  //      }
  //    }

  StaticJsonDocument<256> message;
  message["time"] = epoch_time;
  message["device_id"] = device_id;

  JsonObject soil = message.createNestedObject("soil");
  soil["soil_moisture"] = soil_moisture;
  soil["soil_moisture_capacitive"] = soil_moisture_capacitive;

  // Serialize message to a temporary buffer
  char buffer[256]; // Save a few CPU cycles by passing the size of the payload to publish()
  size_t n = serializeJson(message, buffer);

  // Publish message to MQTT topic
  if (client.publish("sensors/soil", buffer, n)) {
      Serial.print("[");
      Serial.print(device_id);
      Serial.println("] Message published successfully.");
  }

  // 5 second sleep
  delay(5000);
  client.loop();
}

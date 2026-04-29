#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "broker.emqx.io";
const char* topic = "test/jsondb/data"; // Use a unique topic

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  client.setServer(mqtt_server, 1883);
}

void reconnect() {
  while (!client.connected()) {
    if (client.connect("ESP32ClientElamaran")) {
      client.publish("test/jsondb/status", "ESP32 Connected");
    } else {
      delay(5000);
    }
  }
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  JsonDocument doc;
  doc["device"] = "ESP32_Wokwi";
  doc["val"] = random(0, 100);
  doc["status"] = "OK";

  char buffer[128];
  serializeJson(doc, buffer);
  
  Serial.print("Sending: ");
  Serial.println(buffer);
  client.publish(topic, buffer);
  
  delay(5000);
}
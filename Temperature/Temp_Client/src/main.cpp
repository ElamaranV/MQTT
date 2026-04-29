#include <WiFi.h>
#include <PubSubClient.h>
#include "DHTesp.h"

const char* ssid = "Wokwi-GUEST";
const char* password = "";

const char* mqtt_server = "172.18.168.57";   // YOUR PC IP

WiFiClient espClient;
PubSubClient client(espClient);

DHTesp dht;

void setup_wifi() {
  delay(10);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void reconnect() {
  while (!client.connected()) {
    if (client.connect("ESP32Client")) {
    } else {
      delay(2000);
    }
  }
}

void setup() {

  Serial.begin(115200);

  dht.setup(15, DHTesp::DHT22);

  setup_wifi();

  client.setServer(mqtt_server, 1883);
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }

  client.loop();

  TempAndHumidity data = dht.getTempAndHumidity();

  float temp = data.temperature;
  float hum = data.humidity;

  char tempString[8];
  char humString[8];

  dtostrf(temp, 1, 2, tempString);
  dtostrf(hum, 1, 2, humString);

  client.publish("sensor/temp", tempString);
  client.publish("sensor/humidity", humString);

  Serial.println(tempString);
  Serial.println(humString);

  delay(5000);
}
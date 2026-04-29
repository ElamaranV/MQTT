#include <WiFi.h>
#include <PubSubClient.h>

const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "host.wokwi.internal";
const int LED_PIN = 2; 

WiFiClient espClient;
PubSubClient client(espClient);

void setup_wifi() {
  delay(10);
  Serial.println("\nConnecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("Message_Arrived:1"); // Trigger for Plotter
  Serial.print(" Topic:[");
  Serial.print(topic);
  Serial.print("] Msg:");
  Serial.println(message);

  if (String(topic) == "wokwi/test/in") {
    if (message == "ON") {
      digitalWrite(LED_PIN, HIGH);
      client.publish("wokwi/test/status", "LED_ON");
    } else if (message == "OFF") {
      digitalWrite(LED_PIN, LOW);
      client.publish("wokwi/test/status", "LED_OFF");
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("MQTT_Connecting...");
    String clientId = "ESP32-Control-" + String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("Connected!");
      client.subscribe("wokwi/test/in");
    } else {
      delay(5000);
    }
  }
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Serial Plotter Data (Heartbeat)
  static unsigned long lastPlot = 0;
  if (millis() - lastPlot > 1000) {
    lastPlot = millis();
    // Format for Serial Plotter: "Variable_Name:Value"
    Serial.print("Uptime_Secs:"); 
    Serial.println(millis() / 1000);
  }
}
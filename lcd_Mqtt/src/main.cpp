#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ===== LCD Setup (Address 0x27, 16 columns, 2 rows) =====
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ===== WiFi Credentials =====
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// ===== MQTT Broker =====
const char* mqtt_server = "host.wokwi.internal";
const int mqtt_port = 1883;

// ===== Topics =====
const char* pubTopic = "elamaran/device/data";
const char* subTopic = "wokwi/test/in";

WiFiClient espClient;
PubSubClient client(espClient);

// ===== Callback Function =====
void callback(char* topic, byte* payload, unsigned int length) {

  String message = "";

  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  // Serial Output
  Serial.println("Message Received:");
  Serial.println(message);
  Serial.println("----------------");

  // LCD Output
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Msg:");
  lcd.setCursor(0, 1);

  // Limit to 16 characters
  if (message.length() > 16) {
    lcd.print(message.substring(0, 16));
  } else {
    lcd.print(message);
  }
}

// ===== Reconnect Function =====
void reconnect() {

  while (!client.connected()) {

    Serial.print("Connecting to MQTT... ");

    if (client.connect("ESP32_LCD_Client", NULL, NULL, NULL, 0, false, NULL, true)) {

      Serial.println("Connected!");
      client.subscribe(subTopic);
      Serial.println("Subscribed to control topic.");

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("MQTT Connected");

    } else {

      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Retry in 3 sec");
      delay(3000);
    }
  }
}

// ===== Setup =====
void setup() {

  Serial.begin(115200);

  // Initialize LCD
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  Serial.println("WiFi Connected");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Connected");

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

// ===== Loop =====
void loop() {

  if (!client.connected()) {
    reconnect();
  }

  client.loop();

  // Publish every 5 seconds
  static unsigned long lastMsg = 0;
  unsigned long now = millis();

  if (now - lastMsg > 5000) {

    lastMsg = now;

    String message = "This is the Message from Elamaran";

    if (client.publish(pubTopic, message.c_str())) {

      Serial.println("Published:");
      Serial.println(message);

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Published:");
      lcd.setCursor(0, 1);
      lcd.print(message);

    } else {
      Serial.println("Publish failed");
    }

    Serial.println("----------------");
  }
}
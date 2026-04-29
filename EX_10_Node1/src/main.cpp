#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>

const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "172.18.168.57"; 

WiFiClient espClient;
PubSubClient client(espClient);
LiquidCrystal_I2C lcd(0x27, 20, 4);
Servo gateServo;

const int ARRIVAL_BTN_PIN = 19;
const int SERVO_PIN = 18;

String spotNames[5] = {"A1", "A2", "A3", "A4", "A5"};
String lotState = "11111"; 
int freeCount = 5;
bool lastBtnState = HIGH;

void callback(char* topic, byte* payload, unsigned int length) {
  lotState = "";
  freeCount = 0;
  for (int i = 0; i < length; i++) {
    char state = (char)payload[i];
    lotState += state;
    if (state == '1') freeCount++;
  }
  lcd.setCursor(0, 1);
  lcd.print("Free Spaces: " + String(freeCount) + "   ");
}

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(0)); 
  
  pinMode(ARRIVAL_BTN_PIN, INPUT_PULLUP);
  gateServo.attach(SERVO_PIN);
  gateServo.write(0); 
  
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Smart Parking Gate");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    while (!client.connected()) {
      lcd.setCursor(0, 2);
      lcd.print("MQTT Connecting...  ");
      String clientId = "ESP32_Gate_" + String(random(0xffff), HEX);
      if (client.connect(clientId.c_str())) { 
        lcd.setCursor(0, 2);
        lcd.print("System Online!      ");
        client.subscribe("parking/status"); 
      } else {
        delay(5000);
      }
    }
  }
  client.loop(); 

  bool btnState = digitalRead(ARRIVAL_BTN_PIN);
  if (btnState == LOW && lastBtnState == HIGH) { 
    if (freeCount > 0) {
      int freeIndices[5];
      int count = 0;
      for (int i = 0; i < 5; i++) {
        if (lotState[i] == '1') {
          freeIndices[count] = i;
          count++;
        }
      }
      
      int assignedSpotIndex = freeIndices[random(0, count)];
      String assignedSpotName = spotNames[assignedSpotIndex];
      
      // GENERATE UNIQUE TICKET ID & GET CURRENT TIME (in seconds)
      int pinCode = random(1000, 10000); 
      unsigned long entryTime = millis() / 1000; 
      
      // Send the Index, PIN, and Entry Time over MQTT
      String payload = String(assignedSpotIndex) + "," + String(pinCode) + "," + String(entryTime);
      client.publish("parking/assign", payload.c_str());
      
      lcd.clear();
      lcd.setCursor(2, 0);
      lcd.print("TICKET ISSUED");
      lcd.setCursor(0, 1);
      lcd.print("Spot: " + assignedSpotName);
      lcd.setCursor(0, 2);
      lcd.print("PIN:  " + String(pinCode)); 
      lcd.setCursor(0, 3);
      lcd.print("Gate Opening...     ");
      
      gateServo.write(90); 
      delay(4000); 
      
      lcd.setCursor(0, 3);
      lcd.print("Gate Closing...     ");
      gateServo.write(0);  
      delay(1000); 
      
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Smart Parking Gate");
      lcd.setCursor(0, 1);
      lcd.print("Free Spaces: " + String(freeCount) + "   ");
      lcd.setCursor(0, 2);
      lcd.print("System Online!      ");
      
    } else {
      lcd.setCursor(0, 2);
      lcd.print("LOT FULL!           ");
      lcd.setCursor(0, 3);
      lcd.print("PLEASE WAIT.        ");
      delay(3000);
      lcd.setCursor(0, 2);
      lcd.print("System Online!      ");
      lcd.setCursor(0, 3);
      lcd.print("                    ");
    }
  }
  lastBtnState = btnState;
  delay(50); 
}
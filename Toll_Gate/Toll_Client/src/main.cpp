#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>
#include <MFRC522.h>
#include <Keypad.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- Configuration ---
const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "172.18.168.57"; // Your local Mosquitto in WSL

// --- UNIQUE TOPICS ---
const char* req_topic = "elamaran/toll/request";
const char* res_topic = "elamaran/toll/response";
const char* bypass_topic = "elamaran/toll/bypass"; 

// --- Pin Definitions ---
#define PIN_SERVO      13
#define PIN_TRIG       2
#define PIN_ECHO       4
#define PIN_EMERGENCY  17 
#define PIN_CAR_ARRIVE 0  
#define SS_PIN         5
#define RST_PIN        15 

// --- Function Prototypes ---
void setup_wifi();
void reconnect();
void callback(char* topic, byte* payload, unsigned int length);
void openGate(bool isEmergency);
float getDistance();
void updateOLED(String line1, String line2 = "", bool force = false);

// --- Objects ---
WiFiClient espClient;
PubSubClient client(espClient);
Servo gateServo;
MFRC522 rfid(SS_PIN, RST_PIN);

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

const byte ROWS = 4; 
const byte COLS = 4; 
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {12, 14, 27, 26}; 
byte colPins[COLS] = {25, 33, 32, 16}; 
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// --- State Variables ---
String inputBuffer = "";
bool isGateOpen = false;
bool carPresent = false; 

String currentLine1 = "";
String currentLine2 = "";

void updateOLED(String line1, String line2, bool force) {
  if (!force && line1 == currentLine1 && line2 == currentLine2) return;
  currentLine1 = line1;
  currentLine2 = line2;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10);
  display.println(line1);
  
  if (line2 != "") {
    display.setTextSize(2); 
    display.setCursor(0, 30);
    display.println(line2);
  }
  display.display();
}

void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  
  updateOLED("System Starting...", "", true);

  gateServo.attach(PIN_SERVO);
  gateServo.write(0);

  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_EMERGENCY, INPUT_PULLUP);
  pinMode(PIN_CAR_ARRIVE, INPUT_PULLUP); 

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void setup_wifi() {
  delay(10);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }
}

void reconnect() {
  while (!client.connected()) {
    updateOLED("Connecting...", "MQTT");
    if (client.connect("Elamaran_Toll_Client_001")) {
      client.subscribe(res_topic);
      updateOLED("Connected!", "", true);
      delay(500);
    } else {
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  JsonDocument doc; 
  DeserializationError error = deserializeJson(doc, payload, length);

  if (error) {
    Serial.println("Failed to read JSON from Server!");
    return;
  }

  if (String(topic) == res_topic) {
    String status = doc["status"] | "ERROR";
    int balance = doc["bal"] | 0;
    String v_num = doc["v_num"] | "UNKNOWN";

    if (status == "OK") {
      updateOLED(v_num + " Approved", "Bal: " + String(balance), true);
      delay(1500);
      openGate(false); 
    } else {
      String errorMsg = doc["msg"] | "Network Error"; 
      updateOLED("Access Denied", errorMsg, true); 
      delay(3000);
      carPresent = false; 
    }
  }
}

void openGate(bool isEmergency) {
  isGateOpen = true;
  gateServo.write(90); 
  
  if (isEmergency) {
    updateOLED("EMERGENCY MODE", "GATE OPEN", true);
    delay(10000);
  } else {
    updateOLED("Proceeding...", "Drive Safe", true);
    long timeout = millis();
    while (getDistance() > 20 && (millis() - timeout < 5000)) { delay(50); }
    timeout = millis();
    while (getDistance() < 40 && (millis() - timeout < 5000)) { delay(50); }
    delay(500); 
  }

  gateServo.write(0); 
  isGateOpen = false;
  carPresent = false; 
}

float getDistance() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  long duration = pulseIn(PIN_ECHO, HIGH);
  return duration * 0.034 / 2;
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  // 1. Car Arrival Simulator
  if (digitalRead(PIN_CAR_ARRIVE) == LOW && !isGateOpen) {
    if (!carPresent) {
      carPresent = true;
      updateOLED("Vehicle Arrived", "Gate Demo...", true);
      gateServo.write(90); delay(2000); gateServo.write(0);  
      updateOLED("Vehicle Detected", "Scan Tag", true);
    }
    delay(200); 
  }

  // 2. Emergency Override
  if (digitalRead(PIN_EMERGENCY) == LOW) {
    client.publish(bypass_topic, "{\"type\":\"EMERGENCY\"}");
    openGate(true);
  }

  // 3. Keypad Administrator Override
  char key = keypad.getKey();
  if (key) {
    if (key == '#') {
      if (inputBuffer == "1234") {
        updateOLED("Manual Override", "OPENING", true);
        client.publish(bypass_topic, "{\"type\":\"KEYPAD\"}");
        openGate(false);
      } else {
        updateOLED("Error", "Wrong PIN", true);
        delay(1000);
      }
      inputBuffer = "";
    } else if (key == '*') {
      inputBuffer = "";
    } else {
      inputBuffer += key;
      updateOLED("Enter PIN:", inputBuffer, true);
    }
  }

  // 4. RFID Scanning
  if (carPresent && rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    String uid = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      uid += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
      uid += String(rfid.uid.uidByte[i], HEX);
      if (i < rfid.uid.size - 1) uid += ":"; 
    }
    uid.toUpperCase();

    JsonDocument doc;
    doc["uid"] = uid;
    String requestString;
    serializeJson(doc, requestString);
    client.publish(req_topic, requestString.c_str());
    
    updateOLED("Tag Scanned", "Processing", true);
    rfid.PICC_HaltA();
    delay(1000);
  } else if (!carPresent && rfid.PICC_IsNewCardPresent()) {
    rfid.PICC_ReadCardSerial(); 
    rfid.PICC_HaltA();
  }

  // 5. Idle Screen Update
  if (!isGateOpen && inputBuffer == "") {
    if (carPresent) {
      updateOLED("Vehicle Detected", "Scan Tag");
    } else {
      updateOLED("SMART TOLL GATE", "WAITING...");
    }
  }
}
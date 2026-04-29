#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <time.h>
#include <Preferences.h>

// --- Configuration ---
const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "172.18.168.57"; // Your local Mosquitto in WSL

// --- UNIQUE TOPICS ---
const char* req_topic = "elamaran/toll/request";
const char* res_topic = "elamaran/toll/response";
const char* bypass_topic = "elamaran/toll/bypass"; 

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 19800; // IST
const int   daylightOffset_sec = 0;

// --- Objects ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

WiFiClient espClient;
PubSubClient client(espClient);
Preferences preferences;

// --- THE VEHICLE DATABASE MAPPING ---
struct Vehicle {
  String uid;
  String plateNumber;
  int balance;
};

const int NUM_VEHICLES = 3;
Vehicle database[NUM_VEHICLES] = {
  {"01:02:03:04", "TN-38-BQ-1122", 500},  
  {"55:66:77:88", "TN-38-AZ-9988", 40},   
  {"11:22:33:44", "TN-38-XY-5555", 1000}  
};

int totalRevenue = 0;
int totalVehicles = 0;

// Log Memory
String lastLog1 = "System Booting...";
String lastLog2 = "";

// --- Function Prototypes ---
void updateDashboard(String log1 = "", String log2 = "");
void reconnect();
int calculateToll();
void callback(char* topic, byte* payload, unsigned int length);

// --- New Split-Screen Dashboard ---
void updateDashboard(String log1, String log2) {
  // Update log memory if new logs are provided
  if (log1 != "") lastLog1 = log1;
  if (log2 != "") lastLog2 = log2;
  else if (log1 != "") lastLog2 = ""; // Clear second line if only 1 string sent

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // TOP HALF: Permanent Stats
  display.setCursor(0, 0);
  display.print("Veh: "); display.print(totalVehicles);
  display.print(" | Rs: "); display.print(totalRevenue);
  
  // Divider Line
  display.drawLine(0, 12, 128, 12, SSD1306_WHITE);

  // BOTTOM HALF: Live Event Log
  display.setCursor(0, 18);
  display.print("--- LATEST EVENT ---");
  
  display.setCursor(0, 32);
  display.println(lastLog1);
  
  display.setCursor(0, 44);
  display.println(lastLog2);

  display.display();
}

void setup() {
  Serial.begin(115200);
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  
  updateDashboard("Initializing...", "Reading Flash Mem");

  // Load Permanent Data from Flash Memory
  preferences.begin("toll_db", false); 
  totalRevenue = preferences.getInt("revenue", 0);
  totalVehicles = preferences.getInt("vehicles", 0);
  
  for (int i = 0; i < NUM_VEHICLES; i++) {
    database[i].balance = preferences.getInt(database[i].uid.c_str(), database[i].balance);
  }

  updateDashboard("Connecting WiFi...", "");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }
  
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void reconnect() {
  while (!client.connected()) {
    updateDashboard("Connecting MQTT...", "Broker: 172.18...");
    if (client.connect("Elamaran_Server_Admin")) {
      client.subscribe(req_topic);
      client.subscribe(bypass_topic);
      updateDashboard("ONLINE & READY", "Waiting for scans");
    } else {
      delay(5000);
    }
  }
}

int calculateToll() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){ return 50; }
  
  int hour = timeinfo.tm_hour;
  if ((hour >= 8 && hour <= 10) || (hour >= 17 && hour <= 19)) {
    return 100; 
  }
  return 50; 
}

void callback(char* topic, byte* payload, unsigned int length) {
  JsonDocument doc;
  deserializeJson(doc, payload, length);
  
  // HANDLE EMERGENCY & KEYPAD BYPASS
  if (String(topic) == bypass_topic) {
    String type = doc["type"].as<String>();
    totalVehicles++; 
    preferences.putInt("vehicles", totalVehicles); 
    
    // Push Log to OLED
    updateDashboard("FREE PASS LOGGED", "Type: " + type);
    return;
  }

  // HANDLE RFID SCANS
  if (String(topic) == req_topic) {
    String scannedUid = doc["uid"].as<String>();
    scannedUid.trim(); 
    
    bool found = false;
    JsonDocument responseDoc;

    for (int i = 0; i < NUM_VEHICLES; i++) {
      String dbUid = database[i].uid;
      dbUid.trim(); 

      if (dbUid.equalsIgnoreCase(scannedUid)) { 
        found = true;
        int currentToll = calculateToll();

        if (database[i].balance >= currentToll) {
          database[i].balance -= currentToll;
          totalRevenue += currentToll;
          totalVehicles++; 

          // Save to Flash
          preferences.putInt(database[i].uid.c_str(), database[i].balance);
          preferences.putInt("revenue", totalRevenue);
          preferences.putInt("vehicles", totalVehicles);
          
          responseDoc["status"] = "OK";
          responseDoc["bal"] = database[i].balance;
          responseDoc["v_num"] = database[i].plateNumber;
          
          // Push Success Log to OLED
          updateDashboard("AUTH SUCCESS", database[i].plateNumber);
        } else {
          responseDoc["status"] = "FAIL";
          responseDoc["bal"] = database[i].balance;
          responseDoc["v_num"] = database[i].plateNumber;
          responseDoc["msg"] = "Low Balance";
          
          // Push Fail Log to OLED
          updateDashboard("AUTH REJECTED", "Low Bal: " + database[i].plateNumber);
        }
        break;
      }
    }

    if (!found) {
      responseDoc["status"] = "FAIL";
      responseDoc["bal"] = 0;
      responseDoc["v_num"] = "UNKNOWN";
      responseDoc["msg"] = "Unregistered";
      
      // Push Unregistered Log to OLED
      updateDashboard("UNKNOWN TAG", "UID: " + scannedUid);
    }

    String responseString;
    serializeJson(responseDoc, responseString);
    client.publish(res_topic, responseString.c_str());
  }
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();
}
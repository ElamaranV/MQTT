#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "172.18.168.57";

WiFiClient espClient;
PubSubClient client(espClient);
LiquidCrystal_I2C lcd(0x27, 20, 4);

const int NUM_SPOTS = 5;
const int redPins[NUM_SPOTS] = {13, 12, 14, 27, 26};
const int greenPins[NUM_SPOTS] = {25, 33, 32, 23, 0};

bool spotFree[NUM_SPOTS] = {true, true, true, true, true}; 
String spotPINs[NUM_SPOTS] = {"", "", "", "", ""};
unsigned long spotEntryTimes[NUM_SPOTS] = {0, 0, 0, 0, 0}; 
String lastLotState = ""; 
const int PARKING_RATE = 2; 
const String MASTER_PIN = "9999"; 

// Keypad Setup
const byte ROWS = 4; 
const byte COLS = 4; 
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {19, 18, 5, 17}; 
byte colPins[COLS] = {16, 4, 2, 15}; 
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

enum UIState { UI_IDLE, UI_ENTER_PIN };
UIState currentState = UI_IDLE;
int selectedSpot = -1;
String inputBuffer = "";

void setSpotColor(int spotIdx) {
  if (spotFree[spotIdx]) {
    digitalWrite(redPins[spotIdx], LOW); 
    digitalWrite(greenPins[spotIdx], HIGH);
  } else {
    digitalWrite(redPins[spotIdx], HIGH);
    digitalWrite(greenPins[spotIdx], LOW);
  }
}

void updateLCD() {
  lcd.clear();
  if (currentState == UI_IDLE) {
    lcd.setCursor(0, 0); lcd.print("Lot Status Node");
    lcd.setCursor(0, 1); lcd.print("Press 1-5 to select");
    lcd.setCursor(0, 2); lcd.print("a parking spot.");
  } else if (currentState == UI_ENTER_PIN) {
    lcd.setCursor(0, 0); lcd.print("Spot A" + String(selectedSpot + 1) + " Selected");
    lcd.setCursor(0, 1); lcd.print("Enter Ticket PIN:");
    lcd.setCursor(0, 2); lcd.print(inputBuffer);
    lcd.setCursor(0, 3); lcd.print("*=Cancel  #=Submit");
  }
}

void publishStatus() {
  String currentLotState = "";
  for (int i = 0; i < NUM_SPOTS; i++) {
    currentLotState += spotFree[i] ? "1" : "0";
  }
  if (currentLotState != lastLotState) {
    lastLotState = currentLotState;
    client.publish("parking/status", currentLotState.c_str(), true); 
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (int i = 0; i < length; i++) msg += (char)payload[i];
  
  if (String(topic) == "parking/assign") {
    int comma1 = msg.indexOf(',');
    int comma2 = msg.indexOf(',', comma1 + 1);
    
    if (comma1 > 0 && comma2 > 0) {
      int spotIdx = msg.substring(0, comma1).toInt();
      String pin = msg.substring(comma1 + 1, comma2);
      unsigned long entryTime = msg.substring(comma2 + 1).toInt();
      
      spotPINs[spotIdx] = pin; 
      // Note: spotEntryTimes is usually set when the car actually parks, 
      // but we store the assigned PIN here.
      Serial.println("Spot A" + String(spotIdx + 1) + " PIN Assigned: " + pin);
    }
  }
}

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < NUM_SPOTS; i++) {
    pinMode(redPins[i], OUTPUT);
    pinMode(greenPins[i], OUTPUT);
    setSpotColor(i); 
  }
  lcd.init();
  lcd.backlight();
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  updateLCD();
}

void loop() {
  if (!client.connected()) {
    while (!client.connected()) {
      String clientId = "ESP32_Lot_" + String(random(0xffff), HEX);
      if (client.connect(clientId.c_str())) { 
        client.subscribe("parking/assign"); 
        publishStatus(); 
      } else {
        delay(5000);
      }
    }
  }
  client.loop(); 

  char key = keypad.getKey();
  if (key) {
    if (currentState == UI_IDLE) {
      if (key >= '1' && key <= '5') {
        selectedSpot = (key - '0') - 1; 
        inputBuffer = "";
        currentState = UI_ENTER_PIN;
        updateLCD();
      }
    } 
    else if (currentState == UI_ENTER_PIN) {
      if (key >= '0' && key <= '9' && inputBuffer.length() < 4) {
        inputBuffer += key;
        updateLCD();
      } 
      else if (key == '*') {
        currentState = UI_IDLE;
        updateLCD();
      } 
      else if (key == '#') {
        bool isUserPIN = (inputBuffer == spotPINs[selectedSpot] && spotPINs[selectedSpot] != "");
        bool isMasterPIN = (inputBuffer == MASTER_PIN);

        if (isUserPIN || isMasterPIN) {
          lcd.clear();
          
          if (!spotFree[selectedSpot]) { 
            // VEHICLE IS EXITING
            unsigned long exitTime = millis() / 1000;
            unsigned long entryTime = spotEntryTimes[selectedSpot];
            
            lcd.setCursor(0, 0); 
            lcd.print(isMasterPIN ? "ADMIN OVERRIDE" : "Vehicle Exited!");
            
            if (entryTime > 0) {
              unsigned long duration = exitTime - entryTime;
              unsigned long totalCost = duration * PARKING_RATE;
              lcd.setCursor(0, 1); lcd.print("Time: " + String(duration) + "s");
              lcd.setCursor(0, 2); lcd.print("Fee: Rs." + String(totalCost));
            }
            
            spotPINs[selectedSpot] = ""; 
            spotEntryTimes[selectedSpot] = 0;
            spotFree[selectedSpot] = true; // Mark as free
            delay(4000); 
          } 
          else { 
            // VEHICLE IS PARKING
            spotEntryTimes[selectedSpot] = millis() / 1000; 
            spotFree[selectedSpot] = false; // Mark as occupied
            
            lcd.setCursor(0, 0); lcd.print("Access Granted!");
            lcd.setCursor(0, 1); lcd.print("Vehicle Parked.");
            delay(2000);
          }

          setSpotColor(selectedSpot); // Update the LEDs
          publishStatus();            // Notify MQTT
        } else {
          lcd.clear();
          lcd.setCursor(0, 1); lcd.print("  INVALID PIN!  ");
          delay(2000);
        }
        currentState = UI_IDLE;
        updateLCD();
      }
    }
  }
}
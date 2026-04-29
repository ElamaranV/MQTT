#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <SPI.h>
#include <MFRC522.h>
#include "config.h"

// LCD setup
LiquidCrystal_I2C lcd(0x27, 20, 4);

// Keypad setup
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {13, 12, 14, 27};
byte colPins[COLS] = {26, 25, 33, 32};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// RFID setup
#define SS_PIN 5
#define RST_PIN 4
MFRC522 rfid(SS_PIN, RST_PIN);

// MQTT setup
WiFiClient espClient;
PubSubClient client(espClient);

// Transaction state
enum State { MAIN_MENU, SELECT_TYPE, INPUT_DETAILS, INPUT_PIN, WAITING_RES, INPUT_OTP, SHOW_RESULT };
State currentState = MAIN_MENU;
String paymentType = "";
String inputAmount = "";
String inputID = ""; 
String inputPIN = "";
String inputOTP = "";
String lastReqID = "";

// Forward Declarations
void displayInputScreen(String title, String currentInput);
void displayResult(String title, String msg);
void displayMainMenu();

void setup_wifi() {
  lcd.clear();
  lcd.print("Connecting WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    lcd.print(".");
  }
  lcd.clear();
  lcd.print("WiFi Connected!");
  delay(1000);
}

void callback(char* topic, byte* payload, unsigned int length) {
  // Modern V7 syntax
  JsonDocument doc;
  deserializeJson(doc, payload, length);

  String status = doc["status"];
  String msg = doc["message"];
  String reqID = doc["req_id"];

  if (reqID != lastReqID) return;

  if (status == "NEED_OTP") {
    currentState = INPUT_OTP;
    inputOTP = "";
    displayInputScreen("Enter OTP:", "");
  } else if (status == "SUCCESS") {
    float bal = doc["balance"];
    String resultMsg = "Bal: $" + String(bal, 2);
    displayResult("Payment Success!", resultMsg);
  } else if (status == "FAILED") {
    displayResult("Payment Failed!", msg);
  }
}

void displayMainMenu() {
  inputAmount = "";
  paymentType = "";
  inputID = "";
  inputPIN = "";
  inputOTP = "";
  currentState = MAIN_MENU;
  displayInputScreen("Enter Amount:", "");
}

void displayMethodMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Choose Method:");
  lcd.setCursor(0, 1);
  lcd.print("A: Debit Card");
  lcd.setCursor(0, 2);
  lcd.print("B: Credit Card");
  lcd.setCursor(0, 3);
  lcd.print("C: UPI Payment");
  currentState = SELECT_TYPE;
}

void displayInputScreen(String title, String currentInput) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(title);
  lcd.setCursor(0, 1);
  lcd.print(currentInput);
}

void displayResult(String title, String msg) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(title);
  lcd.setCursor(0, 2);
  lcd.print(msg);
  lcd.setCursor(0, 3);
  lcd.print("Press Any Key...");
  currentState = SHOW_RESULT;
}

void sendPaymentRequest() {
  JsonDocument doc; // Dynamic memory management in V7
  lastReqID = String(millis());
  doc["req_id"] = lastReqID;
  doc["type"] = paymentType;
  doc["amount"] = inputAmount.toFloat();
  doc["id"] = inputID;
  doc["pin"] = inputPIN;

  char buffer[512];
  serializeJson(doc, buffer);
  client.publish(TOPIC_REQ, buffer);
  
  lcd.clear();
  lcd.print("Sent Request...");
  lcd.setCursor(0, 1);
  lcd.print("Waiting for Server");
  currentState = WAITING_RES;
}

void sendOTP() {
  JsonDocument doc; 
  doc["req_id"] = lastReqID;
  doc["otp"] = inputOTP;

  char buffer[256];
  serializeJson(doc, buffer);
  client.publish(TOPIC_OTP, buffer);

  lcd.clear();
  lcd.print("Verifying OTP...");
  currentState = WAITING_RES;
}

void handleRFID() {
  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  String tag = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    tag += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
    tag += String(rfid.uid.uidByte[i], HEX);
  }
  tag.toUpperCase();
  rfid.PICC_HaltA();

  Serial.print("Scanned UID: ");
  Serial.println(tag);

  if (currentState == INPUT_DETAILS && (paymentType == "debit" || paymentType == "credit")) {
    inputID = tag;
    currentState = INPUT_PIN;
    inputPIN = "";
    displayInputScreen("Enter PIN:", "");
  }
}

void reconnect() {
  while (!client.connected()) {
    if (client.connect("PaymentTerminal-01")) {
      client.subscribe(TOPIC_RES);
    } else {
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();
  
  SPI.begin();
  rfid.PCD_Init();
  
  setup_wifi();
  
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  
  displayMainMenu();
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  char key = keypad.getKey();
  handleRFID();

  if (key) {
    if (currentState == MAIN_MENU) {
      if (key == '#') {
        if (inputAmount.length() > 0) displayMethodMenu();
      } else if (key >= '0' && key <= '9') {
        inputAmount += key;
        displayInputScreen("Enter Amount:", inputAmount);
      } else if (key == '*') {
        displayMainMenu();
      }
    }
    else if (currentState == SELECT_TYPE) {
      if (key == 'A') { paymentType = "debit"; currentState = INPUT_DETAILS; displayInputScreen("ID: (Scan Card)", ""); }
      else if (key == 'B') { paymentType = "credit"; currentState = INPUT_DETAILS; displayInputScreen("ID: ", ""); }
      else if (key == 'C') { paymentType = "upi"; currentState = INPUT_DETAILS; displayInputScreen("Phone/ID: ", ""); }
      else if (key == '*') displayMainMenu();
    } 
    else if (currentState == INPUT_DETAILS) {
      if (key == '#') {
        if (inputID != "") { currentState = INPUT_PIN; inputPIN = ""; displayInputScreen("Enter PIN:", ""); }
      } else if (key == '*') {
        displayMethodMenu();
      } else if ((key >= '0' && key <= '9') || (key >= 'A' && key <= 'C')) {
        inputID += key;
        displayInputScreen("ID: " + inputID, "Press # to finish");
      }
    }
    else if (currentState == INPUT_PIN) {
      if (key == '#') {
        sendPaymentRequest();
      } else if (key == '*') {
        currentState = INPUT_DETAILS;
        inputPIN = "";
        displayInputScreen("ID: " + inputID, "Press # to finish");
      } else if (key >= '0' && key <= '9') {
        inputPIN += key;
        String stars = "";
        for(int i=0; i<inputPIN.length(); i++) stars += "*";
        displayInputScreen("Enter PIN:", stars);
      }
    }
    else if (currentState == INPUT_OTP) {
      if (key == '#') {
        sendOTP();
      } else if (key >= '0' && key <= '9') {
        inputOTP += key;
        displayInputScreen("OTP: ", inputOTP);
      }
    }
    else if (currentState == SHOW_RESULT) {
      displayMainMenu();
    }
  }
}
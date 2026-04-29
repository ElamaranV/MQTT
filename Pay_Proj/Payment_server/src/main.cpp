#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>
#include "config.h"

// Hardware setup
LiquidCrystal_I2C lcd(0x27, 16, 2);
WiFiClient espClient;
PubSubClient client(espClient);

// JSON Database (Simulated in memory)
// V7 uses JsonDocument for both static and dynamic needs
JsonDocument db;
char currentOTP[5] = "0000";
String lastRequestID = "";

// Pending transaction context
String pendingUserID = "";
String pendingType = "";
float pendingAmount = 0;

// Forward Declarations
void handlePaymentRequest(JsonDocument& req);
void handleOTPVerification(JsonDocument& req);
void updateBalanceAndFinish();
void generateOTP();
void sendResponse(String status, String msg);

// Initialize Database
void initDB() {
  db.clear();
  
  // V7 syntax for creating nested structures
  JsonArray users = db["users"].to<JsonArray>();

  JsonObject user1 = users.add<JsonObject>();
  user1["name"] = "Mr. Sugumar";
  user1["id"] = "101";
  user1["account_balance"] = 5000.00; // Shared balance
  
  JsonObject debit = user1["debit_card"].to<JsonObject>();
  debit["card_number"] = "1234123412341234";
  debit["pin"] = "1111";
  debit["rfid_tag"] = "01020304"; 

  JsonObject credit = user1["credit_card"].to<JsonObject>();
  credit["card_number"] = "4321";
  credit["pin"] = "2222";
  credit["limit"] = 10000.00;
  credit["used"] = 1000.00;

  JsonObject upi = user1["upi"].to<JsonObject>();
  upi["upi_id"] = "9876543210"; 
  upi["phone"] = "9876543210";
  upi["pin"] = "3333";
}

void setup_wifi() {
  delay(10);
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    lcd.print(".");
  }
  lcd.clear();
  lcd.print("WiFi Connected");
  Serial.println("WiFi connected");
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.println("]");

  JsonDocument doc; // Size is handled automatically
  DeserializationError error = deserializeJson(doc, payload, length);

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }

  if (String(topic) == TOPIC_REQ) {
    handlePaymentRequest(doc);
  } else if (String(topic) == TOPIC_OTP) {
    handleOTPVerification(doc);
  }
}

void handlePaymentRequest(JsonDocument& req) {
  String type = req["type"]; 
  float amount = req["amount"];
  String pin = req["pin"];
  String identifier = req["id"]; 
  
  lastRequestID = req["req_id"].as<String>();

  // Debugging info
  Serial.println("--- TRANSACTION ATTEMPT ---");
  Serial.printf("Type: %s | ID: %s | Amt: %.2f\n", type.c_str(), identifier.c_str(), amount);

  lcd.clear();
  lcd.print("New Req: ");
  lcd.print(type);
  lcd.setCursor(0, 1);
  lcd.print("Amt: ");
  lcd.print(amount);

  JsonArray users = db["users"];
  bool found = false;
  
  for (JsonObject user : users) {
    if (type == "debit") {
      String dbCard = user["debit_card"]["card_number"].as<String>();
      String dbRFID = user["debit_card"]["rfid_tag"].as<String>();
      String dbPIN = user["debit_card"]["pin"].as<String>();

      if ((dbCard == identifier || dbRFID == identifier) && dbPIN == pin) {
        if (user["account_balance"].as<float>() >= amount) {
          found = true;
          pendingUserID = user["id"].as<String>();
          pendingType = type;
          pendingAmount = amount;
          generateOTP();
          sendResponse("NEED_OTP", "Enter OTP on Terminal");
        } else {
          sendResponse("FAILED", "Insufficient Balance");
        }
        break;
      }
    } else if (type == "credit") {
      String dbCard = user["credit_card"]["card_number"].as<String>();
      String dbPIN = user["credit_card"]["pin"].as<String>();

      if (dbCard == identifier && dbPIN == pin) {
        float limit = user["credit_card"]["limit"];
        float used = user["credit_card"]["used"];
        if ((limit - used) >= amount) {
          found = true;
          pendingUserID = user["id"].as<String>();
          pendingType = type;
          pendingAmount = amount;
          generateOTP();
          sendResponse("NEED_OTP", "Enter OTP on Terminal");
        } else {
          sendResponse("FAILED", "Limit Exceeded");
        }
        break;
      }
    } else if (type == "upi") {
      String dbID = user["upi"]["upi_id"].as<String>();
      String dbPhone = user["upi"]["phone"].as<String>();
      String dbPIN = user["upi"]["pin"].as<String>();

      if ((dbID == identifier || dbPhone == identifier) && dbPIN == pin) {
        if (user["account_balance"].as<float>() >= amount) {
          found = true;
          pendingUserID = user["id"].as<String>();
          pendingType = type;
          pendingAmount = amount;
          generateOTP();
          sendResponse("NEED_OTP", "Enter OTP on Terminal");
        } else {
          sendResponse("FAILED", "Insufficient Balance");
        }
        break;
      }
    }
  }

  if (!found && type != "") {
      sendResponse("FAILED", "Invalid Auth");
  }
}

void handleOTPVerification(JsonDocument& req) {
  String enteredOTP = req["otp"];
  if (enteredOTP == currentOTP) {
    updateBalanceAndFinish();
  } else {
    sendResponse("FAILED", "Wrong OTP");
  }
}

void updateBalanceAndFinish() {
  JsonArray users = db["users"];
  float newBal = 0;
  bool updated = false;
  for (JsonObject user : users) {
    if (user["id"].as<String>() == pendingUserID) {
      if (pendingType == "debit" || pendingType == "upi") {
        float b = user["account_balance"];
        user["account_balance"] = b - pendingAmount;
        newBal = b - pendingAmount;
        updated = true;
      } else if (pendingType == "credit") {
        float u = user["credit_card"]["used"];
        user["credit_card"]["used"] = u + pendingAmount;
        newBal = user["credit_card"]["limit"].as<float>() - (u + pendingAmount);
        updated = true;
      }
      
      if (updated) {
        Serial.print("Database Updated. User: ");
        Serial.println(user["name"].as<String>());
      }
      break;
    }
  }

  lcd.clear();
  lcd.print("Success!");
  lcd.setCursor(0, 1);
  lcd.print("Bal Updated");
  
  // Send the success response with the balance
  JsonDocument res;
  res["status"] = "SUCCESS";
  res["message"] = "Payment Accepted";
  res["balance"] = newBal;
  res["req_id"] = lastRequestID;
  
  char buffer[256];
  serializeJson(res, buffer);
  client.publish(TOPIC_RES, buffer);
}

void generateOTP() {
  int r = random(1000, 9999);
  sprintf(currentOTP, "%04d", r);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Gen OTP:");
  lcd.setCursor(0, 1);
  lcd.print(currentOTP);
  Serial.print("Generated OTP: ");
  Serial.println(currentOTP);
}

void sendResponse(String status, String msg) {
  JsonDocument res;
  res["status"] = status;
  res["message"] = msg;
  res["req_id"] = lastRequestID;
  
  char buffer[256];
  serializeJson(res, buffer);
  client.publish(TOPIC_RES, buffer);
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("PaymentServer-01")) {
      Serial.println("connected");
      client.subscribe(TOPIC_REQ);
      client.subscribe(TOPIC_OTP);
    } else {
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(0));
  
  lcd.init();
  lcd.backlight();
  lcd.print("Server Starting");
  
  initDB();
  setup_wifi();
  
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
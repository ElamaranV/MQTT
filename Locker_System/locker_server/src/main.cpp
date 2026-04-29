#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const int BTN_PIN = 13; // Safe input pin for the master override
const int LED_PIN = 2;  

const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "172.18.168.57"; 

WiFiClient espClient;
PubSubClient client(espClient);
LiquidCrystal_I2C lcd(0x27, 20, 4);

const String MASTER_PIN = "1234";
const String PIN_L1 = "1111";
const String PIN_L2 = "2222";
const String PIN_L3 = "3333";

unsigned long lastButtonPress = 0;
bool systemLockdown = false;
String l1_status = "SECURED";
String l2_status = "SECURED";
String l3_status = "SECURED";

String expectedOTP = "";
int pendingLocker = 0; 
int failedAttempts = 0; 

void setup_wifi();
void updateDashboard();
void triggerOTP(int lockerNum);
void handleFailedAttempt();
void callback(char* topic, byte* payload, unsigned int length);
void reconnect();

void setup_wifi() {
  delay(10);
  lcd.setCursor(0, 0); lcd.print("Connecting WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }
  lcd.clear(); lcd.print("WiFi Connected!");
  delay(1000);
}

void updateDashboard() {
  lcd.clear();
  if (systemLockdown) {
    lcd.setCursor(0, 0); lcd.print("!!! SYSTEM LOCKED !!!");
  } else {
    lcd.setCursor(0, 0); lcd.print("SYSTEM: SECURE       ");
  }
  lcd.setCursor(0, 1); lcd.print("L1: " + l1_status);
  lcd.setCursor(0, 2); lcd.print("L2: " + l2_status);
  lcd.setCursor(0, 3); lcd.print("L3: " + l3_status);
}

void triggerOTP(int lockerNum) {
  expectedOTP = String(random(1000, 9999));
  pendingLocker = lockerNum;
  Serial.println("\n[SMS DISPATCHED] L" + String(lockerNum) + " OTP: " + expectedOTP + "\n");
  String topic = "bank/locker/" + String(lockerNum) + "/response";
  client.publish(topic.c_str(), "REQUIRE_OTP");
}

void handleFailedAttempt() {
  failedAttempts++;
  Serial.println("Failed attempts: " + String(failedAttempts));
  
  if (failedAttempts >= 3) {
    systemLockdown = true;
    digitalWrite(LED_PIN, HIGH);
    client.publish("bank/system/override", "MAX_FAILURES"); 
    Serial.println("CRITICAL: MAX FAILED ATTEMPTS REACHED. LOCKDOWN INITIATED.");
    updateDashboard();
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String topicStr = String(topic);
  String msg = "";
  for (int i = 0; i < length; i++) { msg += (char)payload[i]; }

  if (topicStr == "bank/main/request") {
    if (msg == MASTER_PIN && !systemLockdown) {
      client.publish("bank/main/response", "GRANT_MASTER");
    } else {
      client.publish("bank/main/response", "DENY_MASTER");
      handleFailedAttempt();
    }
  } 
  
  else if (topicStr == "bank/locker/1/request") {
    if (msg == PIN_L1 && !systemLockdown) { triggerOTP(1); }
    else { client.publish("bank/locker/1/response", "DENY_LOCKER"); handleFailedAttempt(); }
  }
  else if (topicStr == "bank/locker/2/request") {
    if (msg == PIN_L2 && !systemLockdown) { triggerOTP(2); }
    else { client.publish("bank/locker/2/response", "DENY_LOCKER"); handleFailedAttempt(); }
  }
  else if (topicStr == "bank/locker/3/request") {
    if (msg == PIN_L3 && !systemLockdown) { triggerOTP(3); }
    else { client.publish("bank/locker/3/response", "DENY_LOCKER"); handleFailedAttempt(); }
  }

  else if (topicStr.indexOf("/otp") > 0) {
    int lockerReq = topicStr.substring(12, 13).toInt(); 
    if (lockerReq == pendingLocker && msg == expectedOTP && !systemLockdown) {
      failedAttempts = 0; 
      String respTopic = "bank/locker/" + String(lockerReq) + "/response";
      client.publish(respTopic.c_str(), "GRANT_LOCKER");
    } else {
      String respTopic = "bank/locker/" + String(lockerReq) + "/response";
      client.publish(respTopic.c_str(), "DENY_LOCKER");
      handleFailedAttempt();
    }
    expectedOTP = "";
    pendingLocker = 0;
  }

  else if (topicStr == "bank/locker/1/status") { l1_status = msg; updateDashboard(); }
  else if (topicStr == "bank/locker/2/status") { l2_status = msg; updateDashboard(); }
  else if (topicStr == "bank/locker/3/status") { l3_status = msg; updateDashboard(); }

  else if (topicStr.endsWith("/alarm")) {
    digitalWrite(LED_PIN, HIGH);
    lcd.setCursor(0, 0); lcd.print("!!! TAMPER ALARM !!! ");
  }
  
  // Perimeter Motion Handling
  else if (topicStr == "bank/vault/alarm" && msg == "MOTION_DETECTED") {
    systemLockdown = true;
    digitalWrite(LED_PIN, HIGH);
    client.publish("bank/system/override", "EMERGENCY_LOCK"); 
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("!!! VAULT MOTION !!!");
    lcd.setCursor(0, 1); lcd.print("UNAUTHORIZED ENTRY");
    Serial.println("CRITICAL: UNAUTHORIZED MOTION DETECTED IN VAULT.");
  }
}

void reconnect() {
  while (!client.connected()) {
    String clientId = "BankServer-"; clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      client.subscribe("bank/main/request");
      client.subscribe("bank/locker/+/request");
      client.subscribe("bank/locker/+/otp"); 
      client.subscribe("bank/locker/+/status");
      client.subscribe("bank/locker/+/alarm");
      client.subscribe("bank/vault/alarm"); 
      updateDashboard();
    } else { delay(5000); }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n--- SERVER BOOTING ---"); 
  
  randomSeed(analogRead(0)); 
  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  lcd.init(); lcd.backlight();
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();

  int btnState = digitalRead(BTN_PIN);
  if (btnState == LOW && millis() - lastButtonPress > 1000) {
    lastButtonPress = millis();
    systemLockdown = !systemLockdown; 
    
    Serial.println("\n[MANAGER OVERRIDE BUTTON PRESSED]");
    
    if (systemLockdown) {
      digitalWrite(LED_PIN, HIGH);
      client.publish("bank/system/override", "EMERGENCY_LOCK");
      Serial.println("--> SYSTEM LOCKED");
    } else {
      digitalWrite(LED_PIN, LOW);
      failedAttempts = 0; 
      client.publish("bank/system/override", "SYSTEM_RESTORED");
      Serial.println("--> SYSTEM RESTORED (Failures Reset to 0)");
    }
    updateDashboard();
  }
}
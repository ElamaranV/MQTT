#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <ESP32Servo.h>

const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "172.18.168.57"; 

WiFiClient espClient;
PubSubClient client(espClient);
LiquidCrystal_I2C lcd(0x27, 20, 4);

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

const int BUZZER_PIN = 5;
const int PIR_PIN = 39; // VN Pin for Motion Sensor

struct LockerConfig {
  int servoPin;
  int swPin;
  int ledPin;
  Servo servo;
  bool isAuthorized;
  bool lastDoorState; 
  unsigned long unlockTime;
};

LockerConfig lockers[3] = {
  {15, 34, 18, Servo(), false, false, 0},
  {2, 35, 19, Servo(), false, false, 0},
  {4, 36, 23, Servo(), false, false, 0} 
};

enum SystemState {
  STATE_IDLE,        
  WAIT_MASTER_AUTH,
  SELECT_LOCKER,
  ENTER_LOCKER_PIN,
  WAIT_PIN_AUTH,
  ENTER_OTP,
  WAIT_OTP_AUTH,
  UNLOCKED,
  LOCKDOWN
};

SystemState currentState = STATE_IDLE; 
String inputBuffer = "";
int selectedLocker = -1; 
const unsigned long UNLOCK_TIMEOUT = 15000; 
unsigned long lastMotionAlarm = 0;

void updateDisplay(String line1, String line2);
void lockAll();
void triggerTamperAlarm(int lockerIndex);
void callback(char* topic, byte* payload, unsigned int length);
void reconnect();
void monitorDoors();

void setup_wifi() {
  updateDisplay("Connecting WiFi...", "");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }
  updateDisplay("WiFi Connected!", "");
  delay(1000);
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n--- CLIENT BOOTING ---");

  lcd.init();
  lcd.backlight();
  
  pinMode(BUZZER_PIN, OUTPUT);
  noTone(BUZZER_PIN);
  pinMode(PIR_PIN, INPUT);

  for(int i=0; i<3; i++) {
    lockers[i].servo.attach(lockers[i].servoPin);
    pinMode(lockers[i].swPin, INPUT);
    pinMode(lockers[i].ledPin, OUTPUT);
  }
  
  lockAll(); 
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback); 
  
  updateDisplay("ENTER MASTER PIN:", "");
}

void lockAll() {
  for(int i=0; i<3; i++) {
    lockers[i].servo.write(0);             
    digitalWrite(lockers[i].ledPin, LOW);  
    lockers[i].isAuthorized = false;
  }
}

void updateDisplay(String line1, String line2) {
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(line1);
  lcd.setCursor(0, 1); lcd.print(line2);
}

void callback(char* topic, byte* payload, unsigned int length) {
  String topicStr = String(topic);
  String msg = "";
  for (int i = 0; i < length; i++) { msg += (char)payload[i]; }
  
  Serial.println("Received: [" + topicStr + "] " + msg);

  if (topicStr == "bank/system/override") {
    if (msg == "EMERGENCY_LOCK" || msg == "MAX_FAILURES") {
      currentState = LOCKDOWN;
      lockAll();
      
      if (msg == "MAX_FAILURES") {
        updateDisplay("!!! MAX ATTEMPTS !!!", "SYSTEM FROZEN");
      } else {
        updateDisplay("!!! LOCKDOWN !!!", "SYSTEM FROZEN");
      }
      tone(BUZZER_PIN, 1000); 
      
    } else if (msg == "SYSTEM_RESTORED") {
      currentState = STATE_IDLE; 
      noTone(BUZZER_PIN);
      inputBuffer = "";
      updateDisplay("ENTER MASTER PIN:", "");
    }
    return;
  }

  if (currentState == LOCKDOWN) return; 

  if (topicStr == "bank/main/response" && currentState == WAIT_MASTER_AUTH) {
    if (msg == "GRANT_MASTER") {
      currentState = SELECT_LOCKER;
      updateDisplay("SELECT LOCKER", "Press 1, 2, or 3");
    } else {
      updateDisplay("ACCESS DENIED", "Master PIN Invalid");
      delay(2000);
      currentState = STATE_IDLE; 
      updateDisplay("ENTER MASTER PIN:", "");
    }
  }
  
  else if (topicStr.endsWith("/response") && selectedLocker != -1) {
    if (msg == "REQUIRE_OTP" && currentState == WAIT_PIN_AUTH) {
      currentState = ENTER_OTP;
      inputBuffer = "";
      updateDisplay("ENTER OTP:", "");
    }
    else if (msg == "GRANT_LOCKER") {
      currentState = UNLOCKED;
      lockers[selectedLocker].isAuthorized = true;
      lockers[selectedLocker].servo.write(90); 
      digitalWrite(lockers[selectedLocker].ledPin, HIGH); 
      lockers[selectedLocker].unlockTime = millis();
      
      String statusTopic = "bank/locker/" + String(selectedLocker + 1) + "/status";
      client.publish(statusTopic.c_str(), "UNLOCKED"); 
      
      updateDisplay("L" + String(selectedLocker + 1) + " UNLOCKED", "Open door now...");
    }
    else if (msg == "DENY_LOCKER") {
      updateDisplay("ACCESS DENIED", "Invalid PIN/OTP");
      delay(2000);
      currentState = STATE_IDLE; 
      updateDisplay("ENTER MASTER PIN:", "");
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    String clientId = "BankClient-"; clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      client.subscribe("bank/main/response");
      client.subscribe("bank/locker/+/response");
      client.subscribe("bank/system/override");
    } else { delay(5000); }
  }
}

void monitorDoors() {
  for(int i=0; i<3; i++) {
    bool currentDoorOpen = (digitalRead(lockers[i].swPin) == HIGH); 
    
    if (currentDoorOpen != lockers[i].lastDoorState) {
      String topic = "bank/locker/" + String(i + 1) + "/status";
      
      if (currentDoorOpen) {
        client.publish(topic.c_str(), "OPEN");
        if (!lockers[i].isAuthorized) { triggerTamperAlarm(i); }
      } else {
        client.publish(topic.c_str(), "CLOSED");
        if (lockers[i].isAuthorized) {
          lockers[i].servo.write(0);
          digitalWrite(lockers[i].ledPin, LOW); 
          lockers[i].isAuthorized = false;
          
          client.publish(topic.c_str(), "SECURED");
          
          if(currentState == UNLOCKED && selectedLocker == i) {
            currentState = STATE_IDLE; 
            inputBuffer = "";
            updateDisplay("SECURED.", "ENTER MASTER PIN:");
          }
        }
      }
      lockers[i].lastDoorState = currentDoorOpen;
    }

    if (lockers[i].isAuthorized && !currentDoorOpen) {
      if (millis() - lockers[i].unlockTime > UNLOCK_TIMEOUT) {
        lockers[i].servo.write(0);
        digitalWrite(lockers[i].ledPin, LOW); 
        lockers[i].isAuthorized = false;
        
        String alarmTopic = "bank/locker/" + String(i + 1) + "/alarm";
        client.publish(alarmTopic.c_str(), "TIMEOUT");
        
        String statusTopic = "bank/locker/" + String(i + 1) + "/status";
        client.publish(statusTopic.c_str(), "SECURED");
        
        if(currentState == UNLOCKED && selectedLocker == i) {
          currentState = STATE_IDLE; 
          inputBuffer = "";
          updateDisplay("TIMEOUT.", "ENTER MASTER PIN:");
        }
      }
    }
  }
}

void triggerTamperAlarm(int lockerIndex) {
  String topic = "bank/locker/" + String(lockerIndex + 1) + "/alarm";
  client.publish(topic.c_str(), "TAMPER");
  tone(BUZZER_PIN, 1000);
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop(); 
  
  monitorDoors(); 

  // --- Perimeter Motion Detection ---
  if (currentState == STATE_IDLE && digitalRead(PIR_PIN) == HIGH) {
    if (millis() - lastMotionAlarm > 5000) { 
      lastMotionAlarm = millis();
      tone(BUZZER_PIN, 1000);
      updateDisplay("!!! INTRUDER !!!", "MOTION DETECTED");
      currentState = LOCKDOWN;
      client.publish("bank/vault/alarm", "MOTION_DETECTED");
    }
  }

  char key = keypad.getKey();
  
  if (key && currentState != LOCKDOWN) {
    if (key == '*') {
      // Force Close Active Locker
      if (currentState == UNLOCKED && selectedLocker != -1) {
        lockers[selectedLocker].servo.write(0);             
        digitalWrite(lockers[selectedLocker].ledPin, LOW);  
        lockers[selectedLocker].isAuthorized = false;       
        
        String statusTopic = "bank/locker/" + String(selectedLocker + 1) + "/status";
        client.publish(statusTopic.c_str(), "SECURED");
      }
      inputBuffer = "";
      selectedLocker = -1;
      currentState = STATE_IDLE; 
      updateDisplay("ENTER MASTER PIN:", "");
      return;
    }

    switch (currentState) {
      case STATE_IDLE: 
        if (key != '#') {
          inputBuffer += key;
          updateDisplay("ENTER MASTER PIN:", inputBuffer);
        } else if (inputBuffer.length() > 0) {
          client.publish("bank/main/request", inputBuffer.c_str());
          currentState = WAIT_MASTER_AUTH;
          inputBuffer = "";
          updateDisplay("Verifying...", "");
        }
        break;

      case SELECT_LOCKER:
        if (key >= '1' && key <= '3') {
          selectedLocker = (key - '0') - 1; 
          currentState = ENTER_LOCKER_PIN;
          inputBuffer = "";
          updateDisplay("L" + String(selectedLocker + 1) + " PIN:", "");
        }
        break;

      case ENTER_LOCKER_PIN:
        if (key != '#') {
          inputBuffer += key;
          String masked = "";
          for(int i=0; i<inputBuffer.length(); i++) masked += "*";
          updateDisplay("L" + String(selectedLocker + 1) + " PIN:", masked);
        } else if (inputBuffer.length() > 0) {
          String topic = "bank/locker/" + String(selectedLocker + 1) + "/request";
          client.publish(topic.c_str(), inputBuffer.c_str());
          currentState = WAIT_PIN_AUTH;
          inputBuffer = "";
          updateDisplay("Verifying...", "");
        }
        break;

      case ENTER_OTP:
        if (key != '#') {
          inputBuffer += key;
          updateDisplay("ENTER OTP:", inputBuffer);
        } else if (inputBuffer.length() > 0) {
          String topic = "bank/locker/" + String(selectedLocker + 1) + "/otp";
          client.publish(topic.c_str(), inputBuffer.c_str());
          currentState = WAIT_OTP_AUTH;
          inputBuffer = "";
          updateDisplay("Verifying OTP...", "");
        }
        break;
        
      default:
        break;
    }
  }
}
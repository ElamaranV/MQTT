#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* mqtt_server = "172.18.168.57";

WiFiClient espClient;
PubSubClient client(espClient);

LiquidCrystal_I2C lcd(0x27,16,2);

int ledPin = 23;
int buzzerPin = 19;

float temperature = 0;
float humidity = 0;

// thresholds
float tempWarn = 28;
float tempCritical = 30;

float humWarn = 65;
float humCritical = 70;

String systemStatus = "NORMAL";

unsigned long lastMessageTime = 0;
const unsigned long sensorTimeout = 15000;

void setup_wifi() {

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void callback(char* topic, byte* payload, unsigned int length) {

  String message = "";

  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  if (String(topic) == "sensor/temp") {
    temperature = message.toFloat();
  }

  if (String(topic) == "sensor/humidity") {
    humidity = message.toFloat();
  }

  lastMessageTime = millis();
}

void evaluateEnvironment() {

  if (temperature > tempCritical && humidity > humCritical) {
    systemStatus = "HEAT STRESS";
  }
  else if (temperature > tempCritical) {
    systemStatus = "HOT";
  }
  else if (humidity > humCritical) {
    systemStatus = "HUMID";
  }
  else if (temperature > tempWarn || humidity > humWarn) {
    systemStatus = "WARNING";
  }
  else {
    systemStatus = "NORMAL";
  }
}

void updateDisplay() {

  lcd.setCursor(0,0);
  lcd.print("T:");
  lcd.print(temperature,1);
  lcd.print(" H:");
  lcd.print(humidity,1);
  lcd.print("   ");

  lcd.setCursor(0,1);

  if(systemStatus == "HEAT STRESS"){
    lcd.print("HEAT STRESS     ");
  }
  else if(systemStatus == "HOT"){
    lcd.print("HOT TEMP        ");
  }
  else if(systemStatus == "HUMID"){
    lcd.print("HIGH HUMIDITY   ");
  }
  else if(systemStatus == "WARNING"){
    lcd.print("WARNING         ");
  }
  else{
    lcd.print("NORMAL          ");
  }
}

void handleAlert() {

  if(systemStatus == "HEAT STRESS"){

    digitalWrite(ledPin, HIGH);
    tone(buzzerPin,1000);

  }
  else if(systemStatus == "HOT" || systemStatus == "HUMID" || systemStatus == "WARNING"){

    digitalWrite(ledPin, HIGH);
    noTone(buzzerPin);

  }
  else{

    digitalWrite(ledPin, LOW);
    noTone(buzzerPin);

  }
}

void reconnect() {

  while (!client.connected()) {

    if (client.connect("ESP32Server")) {

      client.subscribe("sensor/temp");
      client.subscribe("sensor/humidity");

    } else {

      delay(2000);

    }
  }
}

void setup() {

  Serial.begin(115200);

  pinMode(ledPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0,0);
  lcd.print("System Starting");
  delay(2000);
  lcd.clear();

  setup_wifi();

  client.setServer(mqtt_server,1883);
  client.setCallback(callback);
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }

  client.loop();

  if(millis() - lastMessageTime > sensorTimeout){

    lcd.setCursor(0,0);
    lcd.print("Sensor Offline  ");
    lcd.setCursor(0,1);
    lcd.print("Check Publisher ");

    digitalWrite(ledPin, LOW);
    noTone(buzzerPin);

    return;
  }

  evaluateEnvironment();
  updateDisplay();
  handleAlert();

  delay(1000);
}
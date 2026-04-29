#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// --- Configuration ---
// 1. Replace with your computer's Local IP (e.g., 192.168.1.15)
const char* mqtt_server = "host.wokwi.internal";

// 2. Wi-Fi Credentials
const char* ssid = "Wokwi-GUEST"; // Use your actual SSID if not in Wokwi
const char* password = "";

const char* topic_req = "math/request"; 
const char* topic_res = "math/result";
const int BUTTON_PIN = 4;

LiquidCrystal_I2C lcd(0x27, 16, 2);
WiFiClient espClient;
PubSubClient client(espClient);

// --- Math Parser Logic ---
const char* str;
char peek() { return *str; }
char get() { return *str++; }

double expression();

double number() {
    double result = 0;
    double sign = 1;
    if (peek() == '-') { get(); sign = -1; }
    while (peek() >= '0' && peek() <= '9') result = result * 10 + get() - '0';
    if (peek() == '.') {
        get();
        double weight = 0.1;
        while (peek() >= '0' && peek() <= '9') {
            result += (get() - '0') * weight;
            weight /= 10;
        }
    }
    return result * sign;
}

double factor() {
    if ((peek() >= '0' && peek() <= '9') || peek() == '-') return number();
    else if (peek() == '(') {
        get();
        double result = expression();
        get();
        return result;
    }
    return 0;
}

double term() {
    double result = factor();
    while (peek() == '*' || peek() == '/') {
        if (get() == '*') result *= factor();
        else {
            double den = factor();
            if (den != 0) result /= den;
        }
    }
    return result;
}

double expression() {
    double result = term();
    while (peek() == '+' || peek() == '-') {
        if (get() == '+') result += term();
        else result -= term();
    }
    return result;
}

// --- MQTT Callback ---
void callback(char* topic, byte* payload, unsigned int length) {
    char input[length + 1];
    for (int i = 0; i < length; i++) input[i] = (char)payload[i];
    input[length] = '\0';
    
    str = input; 
    double result = expression();

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(input); // Shows the math problem
    lcd.setCursor(0, 1);
    lcd.print("= ");
    lcd.print(result, 2);

    String resStr = String(result, 2);
    client.publish(topic_res, resStr.c_str());
}

// --- Connection Logic ---
void reconnect() {
    while (!client.connected()) {
        Serial.print("Attempting MQTT connection to ");
        Serial.println(mqtt_server);
        
        // Ensure unique Client ID for local broker
        if (client.connect("ESP32_Math_Solver")) {
            Serial.println("Connected to Local Mosquitto!");
            client.subscribe(topic_req);
            lcd.clear();
            lcd.print("Local MQTT: OK");
        } else {
            Serial.print("Failed, rc=");
            Serial.print(client.state());
            Serial.println(" - Retrying in 5s");
            delay(5000);
        }
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    
    lcd.init();
    lcd.backlight();
    lcd.print("WiFi Connecting");

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi Connected!");
        lcd.clear();
        lcd.print("WiFi: OK");
    } else {
        Serial.println("\nWiFi Connection Failed!");
        lcd.clear();
        lcd.print("WiFi: FAILED");
    }
    
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);
}

void loop() {
    if (!client.connected()) reconnect();
    client.loop();

    // Manual Reset via Button
    if (digitalRead(BUTTON_PIN) == LOW) {
        delay(50);
        lcd.clear();
        lcd.print("Ready for Math");
        while(digitalRead(BUTTON_PIN) == LOW);
    }
}
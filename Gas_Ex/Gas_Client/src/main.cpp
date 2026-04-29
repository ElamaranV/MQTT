

#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Keypad.h>

// ── WiFi & MQTT ──────────────────────────────────────────────────────────────
const char* ssid        = "Wokwi-GUEST";
const char* password    = "";
const char* mqtt_server = "172.18.168.57";

const char* topic_subscribe = "factorySafe99X/gas/status";
const char* topic_publish   = "factorySafe99X/valve/control";

WiFiClient   espClient;
PubSubClient client(espClient);

// ── OLED ──────────────────────────────────────────────────────────────────────
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
bool oledOK = false;

// ── RGB LED Pins (4 lanes) ────────────────────────────────────────────────────
// GREEN = valve OPEN (safe), RED = valve SHUT (alarm)
const int rgbR[4] = {13, 14, 26, 33};
const int rgbG[4] = {12, 27, 25, 32};

// ── Buzzer ────────────────────────────────────────────────────────────────────
const int BUZZER_PIN = 23;
// ── Keypad (2×3, matches original Wokwi JSON wiring) ─────────────────────────
const byte ROWS = 2, COLS = 3;
char keys[ROWS][COLS] = {
  {'1', '2', '3'},
  {'4', '5', '6'}
};
byte rowPins[ROWS] = {19, 18};

// ---> UPDATE THIS LINE <---
byte colPins[COLS] = {5, 4, 15}; // Replaced D17 and D16 with safe pins D4 and D15

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ── State ─────────────────────────────────────────────────────────────────────
int  gasPPM[4]         = {0, 0, 0, 0};   // PPM values received from server
bool valveOpen[4]      = {true, true, true, true};
bool manualOverride[4] = {false, false, false, false};

unsigned long lastReconnectAttempt = 0;

// ── updateLEDs ────────────────────────────────────────────────────────────────
void updateLEDs() {
  bool alarmActive = false;

  for (int i = 0; i < 4; i++) {
    if (valveOpen[i]) {
      digitalWrite(rgbR[i], LOW);    // RED off
      digitalWrite(rgbG[i], HIGH);   // GREEN on  → SAFE
    } else {
      digitalWrite(rgbR[i], HIGH);   // RED on    → ALARM
      digitalWrite(rgbG[i], LOW);    // GREEN off
      alarmActive = true;
    }
  }

  // FIX #1: Buzzer on during alarm, properly silenced when alarm clears.
  // noTone() was previously commented out — buzzer ran forever once triggered.
  if (alarmActive) {
    tone(BUZZER_PIN, 1000);
  } else {
    noTone(BUZZER_PIN);
  }

  // FIX #4: Serial debug for LED/buzzer state
  Serial.print("LEDs: ");
  for (int i = 0; i < 4; i++) {
    Serial.printf("L%d=%s ", i + 1, valveOpen[i] ? "GRN" : "RED");
  }
  Serial.println(alarmActive ? "| BUZZER ON" : "| buzzer off");
}

// ── updateOLED ────────────────────────────────────────────────────────────────
void updateOLED() {
  if (!oledOK) return;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Header
  display.setCursor(0, 0);
  display.println("--- CONTROL ROOM ---");

  // One row per lane: "L1: 1500ppm [SHUT]*"
  for (int i = 0; i < 4; i++) {
    display.setCursor(0, 12 + (i * 13));
    display.print("L");
    display.print(i + 1);
    display.print(":");
    // FIX #2: Show PPM value (server now sends PPM, not raw ADC)
    display.print(gasPPM[i]);
    display.print("p ");
    display.print(valveOpen[i] ? "[OPEN]" : "[SHUT]");
    if (manualOverride[i]) display.print("*");
  }

  display.display();
}

// ── MQTT callback ─────────────────────────────────────────────────────────────
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  Serial.println("MQTT RX: " + msg);

  // Parse 8 comma-separated values: ppm0,valve0,ppm1,valve1,...
  int values[8] = {0};
  int valIdx = 0, startIndex = 0;

  for (int i = 0; i < (int)msg.length(); i++) {
    if (msg.charAt(i) == ',' && valIdx < 7) {
      values[valIdx++] = msg.substring(startIndex, i).toInt();
      startIndex = i + 1;
    }
  }
  // Write last value unconditionally (no off-by-one guard)
  values[valIdx] = msg.substring(startIndex).toInt();

  // Unpack: even index = PPM, odd index = valve state
  for (int i = 0; i < 4; i++) {
    gasPPM[i]   = values[i * 2];
    valveOpen[i] = (values[(i * 2) + 1] == 1);
  }

  updateLEDs();
  updateOLED();
}

// ── reconnect ─────────────────────────────────────────────────────────────────
void reconnect() {
  if (!client.connected()) {
    String clientId = "ControlRoom-" + String(random(0xffff), HEX);
    Serial.print("Connecting to MQTT...");
    if (client.connect(clientId.c_str())) {
      client.subscribe(topic_subscribe);
      Serial.println(" connected. Subscribed to " + String(topic_subscribe));
    } else {
      Serial.printf(" failed (rc=%d), retry in 5s\n", client.state());
    }
  }
}

// ── setup ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== FactorySafe99X — Control Room Node ===");

  // LED output pins
  for (int i = 0; i < 4; i++) {
    pinMode(rgbR[i], OUTPUT);
    pinMode(rgbG[i], OUTPUT);
  }
  pinMode(BUZZER_PIN, OUTPUT);

  // All LEDs green (safe) at boot
  updateLEDs();

  // I2C on SDA=21, SCL=22
  Wire.begin(21, 22);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("WARNING: OLED init failed — continuing without display");
    oledOK = false;
  } else {
    Serial.println("OLED init OK");
    oledOK = true;
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    // Boot message while waiting for first MQTT packet
    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("--- CONTROL ROOM ---");
    display.setCursor(0, 20);
    display.println("Connecting...");
    display.display();
  }

  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected: " + WiFi.localIP().toString());

  client.setServer(mqtt_server, 1883);
  client.setCallback(mqtt_callback);
}

// ── loop ──────────────────────────────────────────────────────────────────────
void loop() {
  if (!client.connected()) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      reconnect();
    }
  } else {
    client.loop();
  }

  char key = keypad.getKey();
  if (key) {
    // Keys '1'–'4' → lanes 0–3
    // Keys '5','6' are ignored (no lane assigned)
    int laneIndex = key - '1';
    if (laneIndex >= 0 && laneIndex <= 3) {
      manualOverride[laneIndex] = !manualOverride[laneIndex];
      String cmd = String(laneIndex + 1) + ":" +
                   (manualOverride[laneIndex] ? "OFF" : "AUTO");
      client.publish(topic_publish, cmd.c_str());
      Serial.println("MQTT TX: " + cmd);
      updateOLED();
    }
  }
}
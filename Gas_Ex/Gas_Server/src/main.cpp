
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ── WiFi & MQTT ───────────────────────────────────────────────────────────────
const char* ssid        = "Wokwi-GUEST";
const char* password    = "";
const char* mqtt_server = "172.18.168.57";

const char* topic_publish   = "factorySafe99X/gas/status";
const char* topic_subscribe = "factorySafe99X/valve/control";

WiFiClient   espClient;
PubSubClient client(espClient);

// ── OLED ──────────────────────────────────────────────────────────────────────
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
bool oledOK = false;

// ── Pin definitions ───────────────────────────────────────────────────────────
const int gasSensorPin[4] = {32, 33, 34, 35};   // gas1–gas4 AOUT
const int relayPin[4]     = {13, 12, 14, 27};   // relay1–relay4 IN
const int RGB_R = 25, RGB_G = 15, RGB_B = 2;    // RGB LED

// ── Threshold ─────────────────────────────────────────────────────────────────
// Wokwi gas sensor AOUT at 0 ppm (slider minimum) → ADC ≈ 800–1100 (idle floor)
// Dangerous gas level                              → ADC ≈ 2500–4095
// 1200 sits safely above idle noise and below danger zone.
// If you still get false alarms at safe levels, raise this to 1400 or 1600.
const int GAS_THRESHOLD_ADC = 1200;

// ── Timing ────────────────────────────────────────────────────────────────────
const unsigned long SENSOR_INTERVAL_MS  = 500;    // read + update every 500ms
const unsigned long PUBLISH_INTERVAL_MS = 2000;   // MQTT publish every 2s
const unsigned long WIFI_RETRY_MS       = 5000;   // WiFi retry every 5s
const unsigned long MQTT_RETRY_MS       = 5000;   // MQTT retry every 5s

// ── State ─────────────────────────────────────────────────────────────────────
int  gasADC[4]         = {0, 0, 0, 0};
bool relayOpen[4]      = {true, true, true, true};
bool manualOverride[4] = {false, false, false, false};

unsigned long lastSensor    = 0;
unsigned long lastPublish   = 0;
unsigned long lastWifiRetry = 0;
unsigned long lastMqttRetry = 0;

// ── RGB helper ────────────────────────────────────────────────────────────────
void setRGB(bool r, bool g, bool b) {
  digitalWrite(RGB_R, r ? HIGH : LOW);
  digitalWrite(RGB_G, g ? HIGH : LOW);
  digitalWrite(RGB_B, b ? HIGH : LOW);
}

// ── updateOLED ────────────────────────────────────────────────────────────────
void updateOLED() {
  if (!oledOK) return;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("--- FIELD SERVER  ---");
  for (int i = 0; i < 4; i++) {
    display.setCursor(0, 12 + (i * 13));
    display.print("G"); display.print(i + 1); display.print(":");
    display.print(gasADC[i]);
    display.print(relayOpen[i] ? " [OPEN]" : " [SHUT]");
    if (manualOverride[i]) display.print("*");
  }
  display.display();
}

// ── readSensors ───────────────────────────────────────────────────────────────
void readSensors() {
  for (int i = 0; i < 4; i++) {
    gasADC[i] = analogRead(gasSensorPin[i]);
  }
}

// ── applyRelays ───────────────────────────────────────────────────────────────
void applyRelays() {
  bool anyAlarm    = false;
  bool anyOverride = false;

  for (int i = 0; i < 4; i++) {
    if (manualOverride[i]) {
      relayOpen[i] = false;
    } else {
      // OPEN (safe) when ADC below threshold, SHUT (alarm) when above
      relayOpen[i] = (gasADC[i] < GAS_THRESHOLD_ADC);
    }

    // ACTIVE LOW relay: HIGH = relay OFF = valve OPEN
    //                   LOW  = relay ON  = valve SHUT
    digitalWrite(relayPin[i], relayOpen[i] ? HIGH : LOW);

    if (!relayOpen[i])     anyAlarm    = true;
    if (manualOverride[i]) anyOverride = true;
  }

  // RGB: BLUE=override, RED=alarm, GREEN=all clear
  if (anyOverride)     setRGB(false, false, true);
  else if (anyAlarm)   setRGB(true,  false, false);
  else                 setRGB(false, true,  false);
}

// ── printSerial ───────────────────────────────────────────────────────────────
void printSerial() {
  Serial.println("─────────────────────────────────────");
  for (int i = 0; i < 4; i++) {
    Serial.printf("G%d | ADC:%4d | thr:%4d | %s | ovr:%s\n",
      i + 1, gasADC[i], GAS_THRESHOLD_ADC,
      relayOpen[i] ? "OPEN" : "SHUT",
      manualOverride[i] ? "Y" : "N"
    );
  }
  Serial.printf("WiFi:%s  MQTT:%s\n",
    WiFi.status() == WL_CONNECTED ? "OK" : "...",
    client.connected() ? "OK" : "..."
  );
}

// ── publishStatus ─────────────────────────────────────────────────────────────
void publishStatus() {
  if (!client.connected()) return;   // skip silently if not connected yet
  String payload = "";
  for (int i = 0; i < 4; i++) {
    payload += String(gasADC[i]);
    payload += ",";
    payload += String(relayOpen[i] ? 1 : 0);
    if (i < 3) payload += ",";
  }
  client.publish(topic_publish, payload.c_str());
  Serial.println("MQTT TX: " + payload);
}

// ── MQTT callback ─────────────────────────────────────────────────────────────
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  Serial.println("MQTT RX: " + msg);

  int colonIdx = msg.indexOf(':');
  if (colonIdx < 1) return;

  int    laneNum = msg.substring(0, colonIdx).toInt();
  String cmd     = msg.substring(colonIdx + 1);
  cmd.trim();

  if (laneNum < 1 || laneNum > 4) return;
  int idx = laneNum - 1;

  if      (cmd == "OFF")  manualOverride[idx] = true;
  else if (cmd == "AUTO") manualOverride[idx] = false;
  else return;

  Serial.printf("Lane %d override → %s\n", laneNum, cmd.c_str());
  applyRelays();
  updateOLED();
  publishStatus();
}

// ── tryConnectWiFi (non-blocking) ─────────────────────────────────────────────
void tryConnectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  unsigned long now = millis();
  if (now - lastWifiRetry < WIFI_RETRY_MS) return;
  lastWifiRetry = now;
  Serial.print("WiFi connecting...");
  WiFi.begin(ssid, password);
}

// ── tryConnectMQTT (non-blocking) ─────────────────────────────────────────────
void tryConnectMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;   // need WiFi first
  if (client.connected()) return;
  unsigned long now = millis();
  if (now - lastMqttRetry < MQTT_RETRY_MS) return;
  lastMqttRetry = now;

  String clientId = "FieldNode-" + String(random(0xffff), HEX);
  Serial.print("MQTT connecting...");
  if (client.connect(clientId.c_str())) {
    client.subscribe(topic_subscribe);
    Serial.println(" OK");
    // Immediately publish current state so client gets updated on connect
    publishStatus();
  } else {
    Serial.printf(" FAILED rc=%d\n", client.state());
  }
}

// ── setup ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(200);   // let serial settle
  Serial.println("\n=== FactorySafe99X — Field Server ===");
  Serial.printf("Threshold: ADC > %d\n", GAS_THRESHOLD_ADC);

  // Relays — all OPEN (HIGH) at boot before anything else
  for (int i = 0; i < 4; i++) {
    pinMode(relayPin[i], OUTPUT);
    digitalWrite(relayPin[i], HIGH);   // HIGH = relay OFF = valve OPEN
  }
  Serial.println("Relays: all OPEN");

  // RGB LED
  pinMode(RGB_R, OUTPUT); pinMode(RGB_G, OUTPUT); pinMode(RGB_B, OUTPUT);
  setRGB(false, true, false);   // GREEN = safe at boot

  // ADC input pins
  for (int i = 0; i < 4; i++) pinMode(gasSensorPin[i], INPUT);

  // OLED
  Wire.begin(21, 22);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED: FAILED — continuing without display");
    oledOK = false;
  } else {
    oledOK = true;
    Serial.println("OLED: OK");
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
  }

  // FIX 1: Do NOT block on WiFi here.
  // Kick off WiFi in background — loop() handles retries.
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  lastWifiRetry = millis();

  client.setServer(mqtt_server, 1883);
  client.setCallback(mqtt_callback);

  // Read sensors and update display immediately at boot
  // so OLED shows real values even before WiFi connects.
  readSensors();
  applyRelays();
  updateOLED();
  printSerial();

  Serial.println("Boot complete — sensor loop running.");
}

// ── loop ──────────────────────────────────────────────────────────────────────
void loop() {
  // Keep WiFi and MQTT alive (non-blocking)
  tryConnectWiFi();
  tryConnectMQTT();
  if (client.connected()) client.loop();

  unsigned long now = millis();

  // Read sensors and update relays + OLED every 500ms
  // This runs regardless of WiFi/MQTT state
  if (now - lastSensor >= SENSOR_INTERVAL_MS) {
    lastSensor = now;
    readSensors();
    applyRelays();
    updateOLED();
    printSerial();
  }

  // Publish to MQTT every 2s (only if connected)
  if (now - lastPublish >= PUBLISH_INTERVAL_MS) {
    lastPublish = now;
    publishStatus();
  }
}
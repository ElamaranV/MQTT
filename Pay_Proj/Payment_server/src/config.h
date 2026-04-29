#ifndef CONFIG_H
#define CONFIG_H

// WiFi Configuration
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// MQTT Broker Configuration
const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;

// MQTT Topics
#define TOPIC_REQ "elamaran/pay/req"      // Terminal -> Server (JSON request)
#define TOPIC_RES "elamaran/pay/res"      // Server -> Terminal (Status/Response)
#define TOPIC_OTP "elamaran/pay/otp"      // Terminal -> Server (OTP validation)
#define TOPIC_LOG "elamaran/pay/log"      // Server -> Terminal/Logs

#endif

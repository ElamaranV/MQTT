# MQTT IoT Projects Collection

A comprehensive collection of IoT projects built with **ESP32** microcontrollers using **MQTT** protocol for real-time communication and control. These projects demonstrate various IoT applications ranging from simple LED control to complex systems like payment processing, toll gates, and smart lockers.

## 📋 Table of Contents

- [Overview](#overview)
- [Project Structure](#project-structure)
- [Featured Projects](#featured-projects)
- [Prerequisites](#prerequisites)
- [Getting Started](#getting-started)
- [Technologies Used](#technologies-used)
- [Project Descriptions](#project-descriptions)
- [Building & Running](#building--running)
- [Database Integration](#database-integration)
- [Contributing](#contributing)

## Overview

This repository contains multiple standalone IoT applications that communicate via **MQTT** (Message Queuing Telemetry Transport), a lightweight publish-subscribe protocol ideal for IoT devices. All projects use **ESP32** as the microcontroller platform and are simulated using **Wokwi** virtual hardware.

### Key Features
- ✅ Real-time device communication via MQTT
- ✅ WiFi connectivity for IoT devices
- ✅ Multiple application scenarios (Smart Home, Payment Systems, Toll Gates, etc.)
- ✅ JSON data handling and storage
- ✅ MongoDB integration for data persistence
- ✅ Wokwi simulator support for prototyping
- ✅ Client-Server architecture patterns

## Project Structure

```
MQTT/
├── Basic IoT Examples
│   ├── led_toggle_mqtt/           # Toggle LEDs via MQTT
│   ├── lcd_Mqtt/                  # LCD display control via MQTT
│   ├── mqttx_test/                # MQTT client testing
│   └── arithmetic_operation/      # Basic arithmetic operations
│
├── Advanced Systems
│   ├── Temperature/               # Temperature sensor system
│   │   ├── Temp_Client/
│   │   └── Temp_Server/
│   ├── Gas_Ex/                    # Gas monitoring system
│   │   ├── Gas_Client/
│   │   └── Gas_Server/
│   ├── Locker_System/             # Smart locker control
│   │   ├── locker_client/
│   │   └── locker_server/
│   ├── Toll_Gate/                 # Toll collection system
│   │   ├── Toll_Client/
│   │   └── Toll_Server/
│   └── Pay_Proj/                  # Payment processing
│       ├── Payment_Client/
│       └── Payment_server/
│
├── Additional Examples
│   ├── esp32-http-server/         # HTTP server implementation
│   ├── JsonTest/                  # JSON data handling & MongoDB
│   ├── Payment_Interface/         # Payment UI with ESP32
│   ├── TicTacToe/                 # Game application
│   │   ├── WebApp/
│   │   ├── MobileApp/
│   │   └── Wokwi_ESP32/
│   ├── EX_10_Node1/               # Exercise examples
│   └── EX10_Node2/
```

## Featured Projects

### 1. **LED Toggle MQTT** - Basic IoT Control
Control LEDs from anywhere using MQTT messaging. Perfect for beginners.

### 2. **Temperature Monitoring System** - IoT Sensor Network
Temperature sensors send data to a central server via MQTT for monitoring and analysis.

### 3. **Smart Locker System** - Access Control
Client-server architecture for secure locker management with MQTT communication.

### 4. **Payment System** - Financial Transaction Processing
Complete payment processing system with client and server components.

### 5. **Toll Gate Management** - Real-world Application
Automated toll collection system using MQTT for vehicle identification and payment.

### 6. **Gas Monitoring System** - Safety & IoT
Monitor gas sensors across multiple locations with central server aggregation.

### 7. **LCD MQTT Control** - Display Integration
Control liquid crystal displays on ESP32 devices via MQTT commands.

### 8. **JSON Test with MongoDB** - Data Persistence
Store and retrieve IoT sensor data using JSON format and MongoDB database.

## Prerequisites

Before you begin, ensure you have installed:

- **PlatformIO** - Embedded development environment
  ```bash
  pip install platformio
  ```
  
- **Visual Studio Code** - Code editor
  
- **Wokwi for VS Code** - Hardware simulator extension
  - Install from VS Code Extensions Marketplace: `wokwi.wokwi-vscode`
  
- **Python 3.7+** - For utility scripts
  
- **Node.js** - For web-based applications (TicTacToe, Payment Interface)
  
- **MongoDB** (Optional) - For data persistence
  - Local: [MongoDB Community](https://www.mongodb.com/try/download/community)
  - Cloud: [MongoDB Atlas](https://www.mongodb.com/cloud/atlas)

## Getting Started

### 1. Clone the Repository
```bash
git clone <repository-url>
cd MQTT
```

### 2. Select a Project
```bash
cd led_toggle_mqtt
```

### 3. Build the Project
```bash
pio run
```

### 4. Simulate with Wokwi
- Open the project in VS Code
- Press `F1`
- Select "Wokwi: Start Simulator"
- Interact with the simulated hardware

### 5. View Output
Open the Serial Monitor in VS Code to see debug messages and MQTT activity.

## Technologies Used

| Technology | Purpose |
|-----------|---------|
| **ESP32** | Microcontroller with WiFi & BLE |
| **MQTT** | Lightweight IoT messaging protocol |
| **ArduinoJson** | JSON parsing and generation |
| **PubSubClient** | MQTT client library |
| **WiFi** | Network connectivity |
| **MongoDB** | NoSQL database for data storage |
| **Python** | Utility scripts and data migration |
| **React/Node.js** | Web and mobile applications |
| **Wokwi** | Virtual hardware simulation |

## Project Descriptions

### Basic IoT Examples

#### **led_toggle_mqtt**
- Demonstrates basic MQTT publish-subscribe pattern
- Toggle LED via MQTT messages
- Uses WiFi to connect to MQTT broker
- Perfect learning project for MQTT basics

#### **lcd_Mqtt**
- Connects I2C LCD display to ESP32
- Displays messages received via MQTT
- Shows real-time data on LCD screen
- Integrates multiple components (WiFi + MQTT + I2C)

#### **mqttx_test**
- MQTT broker testing and debugging
- Message publishing and subscription examples
- Useful for understanding MQTT message flow

### Advanced Systems

#### **Temperature System**
- **Temp_Client**: Reads temperature sensors and publishes data
- **Temp_Server**: Receives and processes temperature data
- Client-Server architecture for sensor networks

#### **Gas Monitoring**
- **Gas_Client**: Gas sensor data collection
- **Gas_Server**: Central monitoring and alerts
- Industrial IoT application example

#### **Smart Locker System**
- **locker_client**: User access interface
- **locker_server**: Lock control and management
- Access control and security implementation

#### **Toll Gate System**
- **Toll_Client**: Vehicle identification and communication
- **Toll_Server**: Payment processing and toll collection
- Real-world IoT deployment scenario

#### **Payment System**
- **Payment_Client**: User payment interface
- **Payment_server**: Transaction processing
- Financial transaction handling via IoT

### Data & Web Integration

#### **JsonTest with MongoDB**
Store sensor data in JSON format with MongoDB persistence:
1. ESP32 sends JSON sensor data via MQTT
2. Python script captures and stores in MongoDB
3. Query and analyze historical data

**Setup:**
```bash
cd JsonTest
pip install -r requirements.txt
# Configure MongoDB connection
python subscriber.py
```

#### **Payment Interface**
Web-based payment UI connecting to ESP32:
- Real-time transaction updates
- WebSocket communication
- Responsive design

#### **TicTacToe Game**
Multi-platform game application:
- **WebApp**: Browser-based version
- **MobileApp**: Mobile application
- **Wokwi_ESP32**: Embedded game on microcontroller

## Building & Running

### Build All Projects
```bash
for dir in */; do
  cd "$dir"
  pio run
  cd ..
done
```

### Build Specific Project
```bash
cd <project-name>
pio run
```

### Run Simulation
1. Open project in VS Code
2. Press `F1` and select "Wokwi: Start Simulator"
3. Or use: `pio run -e wokwi && pio device monitor`

### View Serial Output
```bash
pio device monitor
```

### Upload to Real Device
```bash
pio run --target upload -e esp32dev
```

## Database Integration

### MongoDB Setup

#### Local Installation
1. Download [MongoDB Community](https://www.mongodb.com/try/download/community)
2. Install and start the MongoDB service
3. Default: `mongodb://localhost:27017/`

#### MongoDB Atlas (Cloud)
1. Create account on [Atlas](https://www.mongodb.com/cloud/atlas)
2. Create free cluster
3. Get connection string: `mongodb+srv://user:pass@cluster.mongodb.net/`

### Python MongoDB Integration
```bash
pip install pymongo
```

Configure connection in Python scripts:
```python
from pymongo import MongoClient

client = MongoClient('mongodb+srv://user:password@cluster.mongodb.net/')
db = client['iot_database']
collection = db['sensor_data']
```

## MQTT Broker Options

### Public Brokers (for testing)
- `broker.emqx.io` - EMQ X Public Broker
- `test.mosquitto.org` - Mosquitto Test Broker
- `host.wokwi.internal` - Wokwi Simulator Broker (for Wokwi projects)

### Self-hosted Broker
- **Mosquitto**: Open-source MQTT broker
  ```bash
  docker run -it -p 1883:1883 eclipse-mosquitto
  ```

## Configuration Files

### `platformio.ini`
Project build configuration. Specifies:
- Framework (Arduino)
- Board type (ESP32)
- Serial monitor settings
- Libraries and dependencies

### `wokwi.toml`
Simulator configuration including:
- Virtual hardware components
- Pin mappings
- Simulation parameters

### `diagram.json`
Wokwi circuit diagram in JSON format showing:
- Component connections
- Pin assignments
- Breadboard layout

## Common MQTT Topics Used

| Topic | Purpose |
|-------|---------|
| `elamaran/device/data` | General device data publishing |
| `wokwi/test/in` | Test messages input |
| `test/jsondb/data` | JSON sensor data |
| `test/jsondb/status` | Connection status |
| `temperature/sensor/#` | Temperature readings |
| `gas/sensor/#` | Gas sensor data |
| `payment/transaction` | Payment transactions |
| `locker/command` | Locker control |
| `toll/vehicle` | Toll vehicle identification |

## File Structure in Projects

Each project typically contains:
```
project_name/
├── src/
│   └── main.cpp           # Main source code
├── include/               # Header files
├── lib/                   # Libraries
├── test/                  # Test files
├── platformio.ini         # PlatformIO config
├── wokwi.toml            # Wokwi simulator config
├── diagram.json          # Circuit diagram
└── README.md             # Project documentation
```

## Troubleshooting

### WiFi Connection Issues
- Verify SSID and password are correct
- Check WiFi credentials in `main.cpp`
- Ensure WiFi network is 2.4GHz (ESP32 limitation)

### MQTT Connection Failed
- Verify MQTT broker address and port
- Check firewall settings
- Confirm broker is running (for local brokers)

### Library Not Found
```bash
pio run --target clean
pio run --target upload
```

### Serial Monitor Not Working
```bash
pio device monitor --baud 115200
```

### Wokwi Simulation Issues
- Update Wokwi VS Code extension
- Verify `wokwi.toml` configuration
- Check `diagram.json` for circuit issues

## Performance Tips

- Use topic filtering to reduce message overhead
- Implement QoS levels appropriately (0, 1, or 2)
- Keep payload size minimal for bandwidth efficiency
- Use JSON compression for large data

## Security Considerations

- ⚠️ Change default credentials in all projects
- Use TLS/SSL for production MQTT connections
- Store sensitive data in secure storage
- Implement proper authentication mechanisms
- Validate all MQTT messages before processing

## Future Enhancements

- [ ] Add cloud integration (AWS IoT, Azure IoT Hub)
- [ ] Implement OTA (Over-The-Air) updates
- [ ] Add energy monitoring and optimization
- [ ] Extend mobile app support
- [ ] Implement advanced analytics dashboard
- [ ] Add machine learning for predictive features

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

## Resources

- [MQTT Protocol Guide](https://mqtt.org/)
- [ESP32 Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
- [PlatformIO Docs](https://docs.platformio.org/)
- [Wokwi Simulator](https://wokwi.com/)
- [ArduinoJson Guide](https://arduinojson.org/)
- [MongoDB Documentation](https://docs.mongodb.com/)

## License

Check individual projects for license information.

---

## Quick Reference

### Common Commands
```bash
# Build current project
pio run

# Clean build
pio run --target clean

# Start simulator (VS Code)
F1 → Wokwi: Start Simulator

# Monitor serial output
pio device monitor

# Install libraries
pio lib install <library_name>

# List available boards
pio boards
```

### Useful Links
- MQTT Broker: `broker.emqx.io:1883`
- Wokwi Broker (Simulator): `host.wokwi.internal:1883`
- MongoDB Atlas: https://www.mongodb.com/cloud/atlas

---

**Happy IoT Development! 🚀**

For questions or issues, please check the individual project documentation or open an issue in the repository.

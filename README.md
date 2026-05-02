# Septic Tank IoT Monitoring System

An IoT-based monitoring system for septic tanks that provides real-time monitoring of internal conditions to prevent overflows, detect dangerous gas accumulation, and optimize pumping schedules.

## Features

- **Level Monitoring**: Ultrasonic sensor measures waste level (cm & % capacity)
- **Gas Detection**: Monitors H2S (>10 ppm = danger), CH4 (>1000 ppm), NH3 (>25 ppm)
- **Environmental Sensing**: Temperature and humidity via DHT22
- **Chemical Parameters**: pH (0-14) and turbidity (0-1000 NTU)
- **Status Indicators**: 3 LEDs (Green = Safe, Yellow = Warning, Red = Critical/Gas)
- **Alert System**: Anomaly confirmation requires >3 consecutive threshold violations
- **Cloud Connectivity**: WiFi + MQTT to HiveMQ broker
- **JSON Data Format**: Structured payload for backend integration

## Hardware Requirements

| Component | Description |
|-----------|-------------|
| ESP32 | esp32doit-devkit-v1 |
| HC-SR04 / JSN-SR04T | Ultrasonic sensor for level measurement (20-450 cm) |
| DHT22 | Temperature & humidity sensor |
| MQ-136 | Hydrogen Sulfide (H2S) gas sensor |
| MQ-4 | Methane/CH4 gas sensor |
| MQ-137 | Ammonia/NH3 gas sensor |
| pH-4502C | pH level sensor (0-14) |
| Turbidity Sensor | Water clarity (0-1000 NTU) |
| LEDs | Green, Yellow, Red status indicators |

## Software Dependencies

Install via PlatformIO:

- `Adafruit Unified Sensor`
- `DHT sensor library`
- `NewPing` (ultrasonic)
- `PubSubClient` (MQTT)
- `ArduinoJson`

## Quick Start

1. Clone this repository
2. Open with PlatformIO IDE or VSCode + PlatformIO extension
3. Configure your WiFi credentials in `src/main.cpp`:
   ```cpp
   const char* ssid = "YOUR_WIFI_SSID";
   const char* password = "YOUR_WIFI_PASSWORD";
   ```
4. Build and upload to ESP32

## MQTT Configuration

- **Broker**: broker.hivemq.com
- **Port**: 1883
- **Topics**:
  - `septic-iot/node-001/data` - sensor readings
  - `septic-iot/node-001/alert` - alert notifications

## Simulation

This project includes Wokwi simulation support:

1. Open [Wokwi](https://wokwi.com)
2. Upload `diagram.json` and `wokwi.toml`
3. Run the simulation with poteniometers simulating gas/pH/turbidity sensors

## Project Structure

```
septic-tank-monitoring/
├── src/
│   └── main.cpp          # Main application code
├── include/              # Header files
├── lib/                  # Local libraries
├── diagram.json          # Wokwi simulation diagram
├── wokwi.toml            # Wokwi configuration
├── platformio.ini        # PlatformIO configuration
└── PRD.md                # Product Requirements Document
```

## Alert Thresholds

| Parameter | Warning | Danger |
|-----------|---------|--------|
| H2S | > 5 ppm | > 10 ppm |
| CH4 | > 500 ppm | > 1000 ppm |
| NH3 | > 15 ppm | > 25 ppm |
| pH | < 5.5 or > 8.5 | < 4.5 or > 9.5 |
| Level | > 70% | > 90% |

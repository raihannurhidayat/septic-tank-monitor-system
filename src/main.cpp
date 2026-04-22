#include <Arduino.h>

/**
 * ============================================================
 *  SEPTIC TANK IoT MONITORING SYSTEM — Wokwi Prototype
 *  PRD: Sistem Monitoring Kondisi Septic Tank Berbasis IoT
 * ============================================================
 *
 * Sensor yang disimulasikan:
 *   - HC-SR04        → Level kotoran (pengganti JSN-SR04T)
 *   - DHT22          → Suhu & kelembaban
 *   - Potensiometer  → MQ-136 (H2S), MQ-4 (CH4), MQ-137 (NH3)
 *   - Potensiometer  → pH-4502C (pH)
 *   - Potensiometer  → Turbidity sensor
 *
 * Output:
 *   - Serial Monitor  → Data real-time + status alert
 *   - LED Indicator   → Hijau (Aman), Kuning (Waspada), Merah (Kritis/Gas)
 *   - MQTT            → broker.hivemq.com (topic: septic-tank/node-001)
 *
 * Payload JSON sesuai PRD Section 4.5
 * ============================================================
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <NewPing.h>
#include <ArduinoJson.h>

// ─────────────────────────────────────────────
//  PIN DEFINITIONS
// ─────────────────────────────────────────────
#define PIN_DHT22         4
#define PIN_TRIG          5
#define PIN_ECHO          18
#define PIN_MQ136_H2S     34    // ADC1 - H2S
#define PIN_MQ4_CH4       35    // ADC1 - CH4
#define PIN_MQ137_NH3     32    // ADC1 - NH3
#define PIN_PH            33    // ADC1 - pH
#define PIN_TURBIDITY     25    // ADC2 - Turbidity

#define PIN_LED_GREEN     26    // Status: AMAN
#define PIN_LED_YELLOW    27    // Status: WASPADA
#define PIN_LED_RED       14    // Status: KRITIS / GAS BERBAHAYA

// ─────────────────────────────────────────────
//  SEPTIC TANK CONFIGURATION
// ─────────────────────────────────────────────
#define TANK_DEPTH_CM       200   // Kedalaman total tangki (cm)
#define TANK_CAPACITY_PCT   100   // 100% kapasitas

// Threshold level kotoran (% kapasitas)
#define LEVEL_WASPADA_PCT   60.0
#define LEVEL_KRITIS_PCT    80.0
#define LEVEL_PENUH_PCT     95.0

// Threshold gas (sesuai OSHA/WHO - PRD Section 2.2 Story 2)
#define THRESHOLD_H2S_PPM   10.0    // H2S > 10 ppm → bahaya
#define THRESHOLD_CH4_PPM   1000.0  // CH4 > 1000 ppm → bahaya
#define THRESHOLD_NH3_PPM   25.0    // NH3 > 25 ppm → bahaya

// Threshold pH
#define PH_MIN_NORMAL       6.0
#define PH_MAX_NORMAL       8.5

// ─────────────────────────────────────────────
//  WiFi & MQTT CONFIGURATION
// ─────────────────────────────────────────────
const char* WIFI_SSID     = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";

const char* MQTT_BROKER   = "broker.hivemq.com";
const int   MQTT_PORT     = 1883;
const char* MQTT_CLIENT   = "septic-tank-esp32-001";
const char* MQTT_TOPIC    = "septic-iot/node-001/data";
const char* MQTT_ALERT    = "septic-iot/node-001/alert";
const char* NODE_ID       = "septic-tank-001";

// ─────────────────────────────────────────────
//  TIMING
// ─────────────────────────────────────────────
#define SAMPLING_INTERVAL_MS  5000    // 5 detik per sampling (Wokwi demo)
#define SERIAL_BAUD           115200

// ─────────────────────────────────────────────
//  SENSOR CALIBRATION CONSTANTS (MQ Series)
//  Formula: ppm = A * (Rs/Ro)^B  [PRD Section 4.3]
//  Di sini kita mapping ADC 0-4095 → ppm range via linear interpolation
// ─────────────────────────────────────────────
// H2S (MQ-136): ADC range → 0-50 ppm
#define H2S_PPM_MAX     50.0
// CH4 (MQ-4):   ADC range → 0-5000 ppm
#define CH4_PPM_MAX     5000.0
// NH3 (MQ-137): ADC range → 0-100 ppm
#define NH3_PPM_MAX     100.0
// pH (4502C):   ADC range → 0-14 pH
#define PH_MIN_VAL      0.0
#define PH_MAX_VAL      14.0
// Turbidity:    ADC range → 0-1000 NTU
#define TURB_NTU_MAX    1000.0

// ─────────────────────────────────────────────
//  OBJECTS
// ─────────────────────────────────────────────
DHT       dht(PIN_DHT22, DHT22);
NewPing   sonar(PIN_TRIG, PIN_ECHO, 300);

WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);

// ─────────────────────────────────────────────
//  STATE VARIABLES
// ─────────────────────────────────────────────
unsigned long lastSampleTime = 0;
bool          gasAlertActive  = false;
int           alertConsecutiveCount = 0;  // Anomaly confirmation (PRD 3.2: >3 sampling)

// ─────────────────────────────────────────────
//  SENSOR DATA STRUCT
// ─────────────────────────────────────────────
struct SensorData {
  float level_cm;
  float level_pct;
  float h2s_ppm;
  float ch4_ppm;
  float nh3_ppm;
  float temp_c;
  float humidity_pct;
  float ph;
  float turbidity_ntu;
  String status;
  bool  gas_alert;
  bool  level_alert;
};

// ─────────────────────────────────────────────
//  FUNCTION DECLARATIONS
// ─────────────────────────────────────────────
void     setupWiFi();
void     setupMQTT();
void     reconnectMQTT();
float    readUltrasonic();
float    adcToPpm(int pin, float maxPpm);
float    adcToPh(int pin);
float    adcToNtu(int pin);
String   determineLevelStatus(float pct);
void     updateLEDs(SensorData &data);
void     publishMQTT(SensorData &data);
void     publishAlert(SensorData &data);
void     printSerial(SensorData &data);
String   buildJsonPayload(SensorData &data);
String   getCurrentTimestamp();

// ─────────────────────────────────────────────
//  SETUP
// ─────────────────────────────────────────────
void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(500);

  Serial.println(F("============================================================"));
  Serial.println(F("  SEPTIC TANK IoT MONITORING SYSTEM — Wokwi Prototype"));
  Serial.println(F("  PRD: Sistem Monitoring Kondisi Septic Tank Berbasis IoT"));
  Serial.println(F("============================================================"));
  Serial.println();

  // Init sensor pins
  pinMode(PIN_LED_GREEN,  OUTPUT);
  pinMode(PIN_LED_YELLOW, OUTPUT);
  pinMode(PIN_LED_RED,    OUTPUT);

  // LED startup test
  Serial.println(F("[INIT] LED self-test..."));
  digitalWrite(PIN_LED_GREEN,  HIGH); delay(300);
  digitalWrite(PIN_LED_YELLOW, HIGH); delay(300);
  digitalWrite(PIN_LED_RED,    HIGH); delay(300);
  digitalWrite(PIN_LED_GREEN,  LOW);
  digitalWrite(PIN_LED_YELLOW, LOW);
  digitalWrite(PIN_LED_RED,    LOW);

  // Init DHT22
  dht.begin();
  Serial.println(F("[INIT] DHT22 initialized."));

  // ADC resolution
  analogReadResolution(12);  // 12-bit ADC → 0-4095
  Serial.println(F("[INIT] ADC set to 12-bit resolution."));

  // WiFi
  setupWiFi();

  // MQTT
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setBufferSize(512);
  Serial.println(F("[INIT] MQTT client configured."));

  Serial.println();
  Serial.println(F("============================"));
  Serial.println(F("  Sistem siap. Mulai sampling..."));
  Serial.println(F("============================"));
  Serial.println();

  lastSampleTime = millis() - SAMPLING_INTERVAL_MS; // Trigger immediate first read
}

// ─────────────────────────────────────────────
//  MAIN LOOP
// ─────────────────────────────────────────────
void loop() {
  // Maintain MQTT connection
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqtt.connected()) {
      reconnectMQTT();
    }
    mqtt.loop();
  }

  // Sampling interval
  unsigned long now = millis();
  if (now - lastSampleTime >= SAMPLING_INTERVAL_MS) {
    lastSampleTime = now;

    SensorData data;

    // ── 1. Level Kotoran (HC-SR04) ──────────────
    float rawDistance = readUltrasonic();
    // Jarak sensor ke permukaan = rawDistance
    // Level kotoran = kedalaman tangki - jarak ke permukaan
    if (rawDistance <= 0 || rawDistance > TANK_DEPTH_CM) {
      data.level_cm  = 0;
    } else {
      data.level_cm = TANK_DEPTH_CM - rawDistance;
    }
    data.level_cm  = constrain(data.level_cm, 0, TANK_DEPTH_CM);
    data.level_pct = (data.level_cm / TANK_DEPTH_CM) * 100.0;

    // ── 2. Gas Sensors (Potensiometer simulasi MQ) ─
    data.h2s_ppm = adcToPpm(PIN_MQ136_H2S, H2S_PPM_MAX);
    data.ch4_ppm = adcToPpm(PIN_MQ4_CH4,   CH4_PPM_MAX);
    data.nh3_ppm = adcToPpm(PIN_MQ137_NH3, NH3_PPM_MAX);

    // ── 3. DHT22 ────────────────────────────────
    data.temp_c       = dht.readTemperature();
    data.humidity_pct = dht.readHumidity();
    if (isnan(data.temp_c))       data.temp_c       = 0.0;
    if (isnan(data.humidity_pct)) data.humidity_pct = 0.0;

    // ── 4. pH Sensor ─────────────────────────────
    data.ph = adcToPh(PIN_PH);

    // ── 5. Turbidity ─────────────────────────────
    data.turbidity_ntu = adcToNtu(PIN_TURBIDITY);

    // ── 6. Status & Alert Logic ──────────────────
    data.status = determineLevelStatus(data.level_pct);

    // Gas alert — konfirmasi >3 sampling berurutan (PRD 3.2)
    bool currentGasOver = (data.h2s_ppm > THRESHOLD_H2S_PPM) ||
                          (data.ch4_ppm > THRESHOLD_CH4_PPM) ||
                          (data.nh3_ppm > THRESHOLD_NH3_PPM);

    if (currentGasOver) {
      alertConsecutiveCount++;
    } else {
      alertConsecutiveCount = 0;
    }
    data.gas_alert   = (alertConsecutiveCount >= 3);
    data.level_alert = (data.level_pct >= LEVEL_KRITIS_PCT);

    // ── 7. Output ─────────────────────────────────
    printSerial(data);
    updateLEDs(data);

    if (WiFi.status() == WL_CONNECTED && mqtt.connected()) {
      publishMQTT(data);
      if (data.gas_alert || data.level_alert) {
        publishAlert(data);
      }
    }
  }
}

// ─────────────────────────────────────────────
//  WIFI SETUP
// ─────────────────────────────────────────────
void setupWiFi() {
  Serial.print(F("[WiFi] Connecting to: "));
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print(F("[WiFi] Connected! IP: "));
    Serial.println(WiFi.localIP());
  } else {
    Serial.println();
    Serial.println(F("[WiFi] GAGAL terhubung. Mode offline - data hanya tampil di Serial."));
  }
}

// ─────────────────────────────────────────────
//  MQTT RECONNECT
// ─────────────────────────────────────────────
void reconnectMQTT() {
  int attempts = 0;
  while (!mqtt.connected() && attempts < 3) {
    Serial.print(F("[MQTT] Connecting to broker.hivemq.com... "));
    if (mqtt.connect(MQTT_CLIENT)) {
      Serial.println(F("CONNECTED!"));
    } else {
      Serial.print(F("GAGAL, rc="));
      Serial.print(mqtt.state());
      Serial.println(F(" | retry in 2s"));
      delay(2000);
      attempts++;
    }
  }
}

// ─────────────────────────────────────────────
//  READ ULTRASONIC (HC-SR04)
// ─────────────────────────────────────────────
float readUltrasonic() {
  unsigned int uS = sonar.ping_median(5);  // 5x median untuk akurasi
  float distance  = sonar.convert_cm(uS);
  return distance;
}

// ─────────────────────────────────────────────
//  ADC → PPM CONVERSION (Linear mapping)
//  Simulasi: Potensiometer 0→full = 0→maxPpm
// ─────────────────────────────────────────────
float adcToPpm(int pin, float maxPpm) {
  int raw = analogRead(pin);
  return (raw / 4095.0) * maxPpm;
}

// ─────────────────────────────────────────────
//  ADC → pH CONVERSION
//  pH 4502C: 0V = pH 0, 3.3V = pH 14
//  (Pada hardware nyata: Vo = 2.5 - 0.1776*(pH-7))
// ─────────────────────────────────────────────
float adcToPh(int pin) {
  int raw = analogRead(pin);
  // Map ADC 0-4095 → pH 0.0-14.0
  float ph = (raw / 4095.0) * 14.0;
  return ph;
}

// ─────────────────────────────────────────────
//  ADC → NTU CONVERSION (Turbidity)
//  Analog turbidity: 0→max ADC = 0→1000 NTU
// ─────────────────────────────────────────────
float adcToNtu(int pin) {
  int raw = analogRead(pin);
  return (raw / 4095.0) * TURB_NTU_MAX;
}

// ─────────────────────────────────────────────
//  DETERMINE LEVEL STATUS
// ─────────────────────────────────────────────
String determineLevelStatus(float pct) {
  if (pct >= LEVEL_PENUH_PCT)   return "PENUH";
  if (pct >= LEVEL_KRITIS_PCT)  return "KRITIS";
  if (pct >= LEVEL_WASPADA_PCT) return "WASPADA";
  return "AMAN";
}

// ─────────────────────────────────────────────
//  UPDATE LED INDICATORS
// ─────────────────────────────────────────────
void updateLEDs(SensorData &data) {
  // Reset all
  digitalWrite(PIN_LED_GREEN,  LOW);
  digitalWrite(PIN_LED_YELLOW, LOW);
  digitalWrite(PIN_LED_RED,    LOW);

  if (data.gas_alert || data.status == "PENUH" || data.status == "KRITIS") {
    digitalWrite(PIN_LED_RED, HIGH);
  } else if (data.status == "WASPADA") {
    digitalWrite(PIN_LED_YELLOW, HIGH);
  } else {
    digitalWrite(PIN_LED_GREEN, HIGH);
  }
}

// ─────────────────────────────────────────────
//  BUILD JSON PAYLOAD (sesuai PRD Section 4.5)
// ─────────────────────────────────────────────
String buildJsonPayload(SensorData &data) {
  StaticJsonDocument<512> doc;

  doc["node_id"]       = NODE_ID;
  doc["timestamp"]     = getCurrentTimestamp();
  doc["level_cm"]      = serialized(String(data.level_cm, 1));
  doc["level_pct"]     = serialized(String(data.level_pct, 1));
  doc["level_status"]  = data.status;
  doc["h2s_ppm"]       = serialized(String(data.h2s_ppm, 2));
  doc["ch4_ppm"]       = serialized(String(data.ch4_ppm, 1));
  doc["nh3_ppm"]       = serialized(String(data.nh3_ppm, 2));
  doc["temp_c"]        = serialized(String(data.temp_c, 1));
  doc["humidity_pct"]  = serialized(String(data.humidity_pct, 1));
  doc["ph"]            = serialized(String(data.ph, 2));
  doc["turbidity_ntu"] = serialized(String(data.turbidity_ntu, 1));
  doc["gas_alert"]     = data.gas_alert;
  doc["level_alert"]   = data.level_alert;

  String output;
  serializeJson(doc, output);
  return output;
}

// ─────────────────────────────────────────────
//  PUBLISH SENSOR DATA VIA MQTT
// ─────────────────────────────────────────────
void publishMQTT(SensorData &data) {
  String payload = buildJsonPayload(data);
  bool ok = mqtt.publish(MQTT_TOPIC, payload.c_str(), false);

  Serial.print(F("[MQTT] Publish → "));
  Serial.print(MQTT_TOPIC);
  Serial.print(ok ? F(" ✓") : F(" ✗ GAGAL"));
  Serial.println();
}

// ─────────────────────────────────────────────
//  PUBLISH ALERT VIA MQTT
// ─────────────────────────────────────────────
void publishAlert(SensorData &data) {
  StaticJsonDocument<256> doc;
  doc["node_id"]     = NODE_ID;
  doc["timestamp"]   = getCurrentTimestamp();
  doc["level_alert"] = data.level_alert;
  doc["gas_alert"]   = data.gas_alert;
  doc["level_status"]= data.status;
  doc["level_pct"]   = serialized(String(data.level_pct, 1));

  if (data.h2s_ppm > THRESHOLD_H2S_PPM) {
    doc["h2s_over"]  = serialized(String(data.h2s_ppm, 2));
  }
  if (data.ch4_ppm > THRESHOLD_CH4_PPM) {
    doc["ch4_over"]  = serialized(String(data.ch4_ppm, 1));
  }
  if (data.nh3_ppm > THRESHOLD_NH3_PPM) {
    doc["nh3_over"]  = serialized(String(data.nh3_ppm, 2));
  }

  String payload;
  serializeJson(doc, payload);

  bool ok = mqtt.publish(MQTT_ALERT, payload.c_str(), false);
  Serial.print(F("[MQTT] ALERT → "));
  Serial.print(MQTT_ALERT);
  Serial.println(ok ? F(" ✓ TERKIRIM") : F(" ✗ GAGAL"));
}

// ─────────────────────────────────────────────
//  SERIAL MONITOR OUTPUT
// ─────────────────────────────────────────────
void printSerial(SensorData &data) {
  Serial.println(F("────────────────────────────────────────────────────────────"));
  Serial.print(F("  NODE: ")); Serial.print(NODE_ID);
  Serial.print(F("  |  Uptime: ")); Serial.print(millis() / 1000); Serial.println(F("s"));
  Serial.println(F("────────────────────────────────────────────────────────────"));

  // Level Kotoran
  Serial.print(F("  📊 LEVEL KOTORAN  : "));
  Serial.print(data.level_cm, 1);
  Serial.print(F(" cm  |  "));
  Serial.print(data.level_pct, 1);
  Serial.print(F("%  →  STATUS: "));
  Serial.println(data.status);

  // Progress bar
  Serial.print(F("  ["));
  int bars = (int)(data.level_pct / 5.0);  // 20 bar = 100%
  for (int i = 0; i < 20; i++) {
    Serial.print(i < bars ? "█" : "░");
  }
  Serial.print(F("] "));
  Serial.print(data.level_pct, 0);
  Serial.println(F("%"));

  Serial.println();

  // Gas Sensors
  Serial.println(F("  🔥 GAS BERBAHAYA:"));

  Serial.print(F("    H2S  (MQ-136) : "));
  Serial.print(data.h2s_ppm, 2);
  Serial.print(F(" ppm"));
  if (data.h2s_ppm > THRESHOLD_H2S_PPM) Serial.print(F("  ⚠️  MELEBIHI BATAS OSHA (>10 ppm)!"));
  Serial.println();

  Serial.print(F("    CH4  (MQ-4)   : "));
  Serial.print(data.ch4_ppm, 1);
  Serial.print(F(" ppm"));
  if (data.ch4_ppm > THRESHOLD_CH4_PPM) Serial.print(F("  ⚠️  MELEBIHI BATAS OSHA (>1000 ppm)!"));
  Serial.println();

  Serial.print(F("    NH3  (MQ-137) : "));
  Serial.print(data.nh3_ppm, 2);
  Serial.print(F(" ppm"));
  if (data.nh3_ppm > THRESHOLD_NH3_PPM) Serial.print(F("  ⚠️  MELEBIHI BATAS OSHA (>25 ppm)!"));
  Serial.println();

  if (data.gas_alert) {
    Serial.println(F("    ⛔  GAS ALERT AKTIF! (>3 sampling berturut-turut melebihi batas)"));
  }

  Serial.println();

  // Environment
  Serial.println(F("  🌡️  LINGKUNGAN:"));
  Serial.print(F("    Suhu       : ")); Serial.print(data.temp_c, 1);     Serial.println(F(" °C"));
  Serial.print(F("    Kelembaban : ")); Serial.print(data.humidity_pct, 1); Serial.println(F(" %RH"));

  Serial.println();

  // Chemical
  Serial.println(F("  🧪 PARAMETER KIMIAWI:"));
  Serial.print(F("    pH         : "));
  Serial.print(data.ph, 2);
  if (data.ph < PH_MIN_NORMAL || data.ph > PH_MAX_NORMAL) {
    Serial.print(F("  ⚠️  Di luar range normal (6.0-8.5)"));
  } else {
    Serial.print(F("  ✓ Normal"));
  }
  Serial.println();

  Serial.print(F("    Turbidity  : ")); Serial.print(data.turbidity_ntu, 1); Serial.println(F(" NTU"));

  Serial.println();

  // Alert Summary
  if (data.gas_alert || data.level_alert) {
    Serial.println(F("  🚨 ═══════════════════════════════════ 🚨"));
    Serial.println(F("     ALERT AKTIF — Notifikasi dikirim via MQTT!"));
    if (data.level_alert) {
      Serial.print(F("     Level kotoran KRITIS: "));
      Serial.print(data.level_pct, 1);
      Serial.println(F("% — Segera lakukan pengurasan!"));
    }
    if (data.gas_alert) {
      Serial.println(F("     Konsentrasi GAS BERBAHAYA melebihi threshold OSHA!"));
      Serial.println(F("     Tindakan: Ventilasi area, hindari zona berbahaya!"));
    }
    Serial.println(F("  🚨 ═══════════════════════════════════ 🚨"));
  }

  Serial.println();
}

// ─────────────────────────────────────────────
//  SIMPLE TIMESTAMP (ms-based untuk simulasi)
// ─────────────────────────────────────────────
String getCurrentTimestamp() {
  // Pada hardware nyata: gunakan NTP + RTC
  // Di Wokwi: gunakan ms counter sebagai pseudo-timestamp
  unsigned long ms = millis();
  unsigned long s  = ms / 1000;
  unsigned long m  = s  / 60;
  unsigned long h  = m  / 60;
  s %= 60; m %= 60;

  char buf[30];
  snprintf(buf, sizeof(buf), "2026-04-22T%02lu:%02lu:%02luZ", h, m, s);
  return String(buf);
}


# Product Requirements Document (PRD)
# Sistem Monitoring Kondisi Septic Tank Berbasis Internet of Things (IoT)

---

## 1. Executive Summary

### 1.1 Problem Statement
Sistem septic tank konvensional tidak memiliki mekanisme pemantauan real-time terhadap kondisi internal, sehingga pengurasan dilakukan secara periodik tanpa memperhatikan kondisi aktual. Hal ini menyebabkan risiko limpahan ketika tank penuh lebih cepat dari jadwal, atau pemborosan biaya pengurasan ketika tank masih memiliki kapasitas. Selain itu, gas berbahaya (H2S, CH4, NH3) yang terakumulasi tanpa deteksi dapat membahayakan keselamatan penghuni.

### 1.2 Proposed Solution
Membangun sistem monitoring IoT end-to-end yang secara real-time memantau level kotoran, parameter gas berbahaya, suhu, kelembaban, dan kondisi kimiawi septic tank. Data dikirim ke cloud backend untuk divisualisasikan pada dashboard web/mobile dengan fitur alerting otomatis dan prediksi jadwal pengurasan berbasis machine learning.

### 1.3 Success Criteria
1. **Akurasi Sensor**: Pembacaan level kotoran akurat dengan toleransi ±2 cm; deteksi gas berbahaya responsif pada threshold batas aman WHO/OSHA.
2. **Latency Data**: Data sensor tersedia pada dashboard dalam waktu < 5 detik dari waktu sampling.
3. **Uptime Sistem**: Ketersediaan sistem monitoring ≥ 95% dalam kondisi operasi normal (24/7).
4. **Prediksi Pengurasan**: Model prediksi mencapai Mean Absolute Percentage Error (MAPE) < 15% dibandingkan kondisi aktual.
5. **Alert Delivery**: Notifikasi alert threshold tersampaikan ke user dalam waktu < 30 detik.

---

## 2. User Experience & Functionality

### 2.1 User Personas

| Persona | Deskripsi | Kebutuhan Utama |
|---------|-----------|-----------------|
| **Pemilik Rumah** | Pengguna residensial yang memiliki 1–2 septic tank | Melihat status penuh, menerima notifikasi sebelum limpah, jadwal pengurasan optimal |
| **Pengelola Perumahan** | Admin komplek perumahan/cluster | Monitoring multi-node, manajemen alert untuk puluhan unit, laporan periodik |
| **Teknisi/Operator** | Tim teknis penguras/pengelola limbah | Akses lokasi dan kondisi sebelum kedatangan, data historis untuk troubleshooting |

### 2.2 User Stories & Acceptance Criteria

#### Story 1: Monitoring Level Kotoran Real-Time
**Sebagai** Pemilik Rumah, **saya ingin** melihat level kotoran septic tank secara real-time **sehingga** saya tahu kapan harus melakukan pengurasan.

**Acceptance Criteria**:
- Dashboard menampilkan level kotoran dalam satuan cm, % kapasitas, dan status (Aman / Waspada / Kritis / Penuh).
- Data diperbarui secara otomatis setiap 30 detik tanpa refresh manual.
- Visualisasi level ditampilkan dalam bentuk gauge meter atau grafik trend historis 7 hari.

#### Story 2: Deteksi dan Alert Gas Berbahaya
**Sebagai** Pemilik Rumah, **saya ingin** menerima notifikasi jika konsentrasi gas berbahaya (H2S, CH4, NH3) melebihi ambang batas **sehingga** saya dapat mengambil tindakan keselamatan.

**Acceptance Criteria**:
- Sistem mendeteksi H2S > 10 ppm, CH4 > 1000 ppm, dan NH3 > 25 ppm sesuai standar OSHA.
- Notifikasi otomatis dikirim melalui Telegram Bot / WhatsApp API / Email dalam waktu < 30 detik setelah threshold terlampaui.
- Log alarm tersimpan di database dengan timestamp dan nilai sensor saat kejadian.

#### Story 3: Prediksi Jadwal Pengurasan
**Sebagai** Pemilik Rumah, **saya ingin** sistem memprediksi tanggal optimal pengurasan berikutnya **sehingga** saya dapat menjadwalkan layanan pengurasan lebih efisien.

**Acceptance Criteria**:
- Model ML memproses data historis level kotoran dan pola penggunaan air untuk prediksi.
- Prediksi menampilkan estimasi tanggal pengurasan dan tingkat confidence (%).
- Prediksi diperbarui minimal sekali per hari.

#### Story 4: Multi-Node Monitoring (Cluster)
**Sebagai** Pengelola Perumahan, **saya ingin** memantau puluhan septic tank dari satu dashboard terpusat **sehingga** saya dapat mengelola seluruh cluster secara efisien.

**Acceptance Criteria**:
- Dashboard mendukung view daftar semua node dengan status ringkasan (Aman / Waspada / Kritis).
- Setiap node memiliki identifikasi unik (Node ID + Lokasi Geografis).
- Data dari masing-masing node dapat di-drill-down untuk melihat detail dan histori.

### 2.3 Non-Goals
- **Tidak** membangun sistem pengurasan otomatis (actuator pompa); monitoring dan alerting saja.
- **Tidak** melakukan analisis kualitas air limbah untuk parameter mikrobiologi (E. coli, bakteri); fokus pada fisik-kimia dan gas.
- **Tidak** mendesain custom PCB dari nol pada tahap ini; menggunakan modul development board (ESP32).
- **Tidak** mengimplementasikan billing/payment gateway untuk layanan pengurasan.

---

## 3. AI System Requirements

### 3.1 Tool Requirements
- **Machine Learning Framework**: Python + scikit-learn / TensorFlow Lite (untuk edge inference) untuk prediksi kapan tank penuh.
- **Time-Series Database**: InfluxDB atau TimescaleDB untuk menyimpan data sensor temporal.
- **Data Processing**: Node-RED atau Python (FastAPI/Flask) untuk pipeline data ingestion dan preprocessing.
- **Visualization**: Grafana atau Next.js dashboard dengan Chart.js/Recharts.
- **Messaging/Alerting**: Telegram Bot API / n8n / MQTT broker (Mosquitto/EMQX).

### 3.2 Evaluation Strategy
- **Prediksi Model**: Evaluasi menggunakan MAPE (Mean Absolute Percentage Error) dengan target < 15%.
- **Anomaly Detection**: Deteksi outlier pada data sensor menggunakan Z-score atau Isolation Forest; alert dibangkitkan hanya jika anomali terkonfirmasi > 3 sampling berurutan.
- **Gas Sensor Calibration**: Sensor gas MQ-series dikalibrasi terhadap gas referensi sebelum deployment; evaluasi drift dilakukan setiap 3 bulan.

---

## 4. Technical Specifications

### 4.1 Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                              EDGE LAYER                                          │
│  ┌──────────────────────────────────────────────────────────────────────────┐   │
│  │  ESP32-WROOM-32 (Microcontroller)                                         │   │
│  │  ├── ADC Multiplexer (ADS1115, 16-bit, 4-channel)                       │   │
│  │  ├── JSN-SR04T (Ultrasonic Level, Waterproof)                             │   │
│  │  ├── MQ-136 (H2S Sensor)                                                  │   │
│  │  ├── MQ-4   (CH4 / Methane Sensor)                                        │   │
│  │  ├── MQ-137 (NH3 / Ammonia Sensor)                                      │   │
│  │  ├── DHT22  (Suhu & Kelembaban, -40~80°C, 0~100% RH)                    │   │
│  │  ├── pH-4502C (pH Probe + Interface Module, 0~14 pH)                    │   │
│  │  └── TSS/Turbidity Sensor (Analog, 0~1000 NTU)                         │   │
│  └──────────────────────────────────────────────────────────────────────────┘   │
│                                    │                                             │
│                         WiFi 802.11 b/g/n / LoRaWAN                            │
│                                    │                                             │
└────────────────────────────────────┼─────────────────────────────────────────────┘
                                     │
                           ┌─────────┴─────────┐
                           │   MQTT Broker     │
                           │  (Mosquitto/EMQX) │
                           └─────────┬─────────┘
                                     │
┌────────────────────────────────────┼─────────────────────────────────────────────┐
│                              CLOUD / BACKEND LAYER                               │
│                                    │                                             │
│  ┌─────────────────────────────────┴──────────────────────────────────────────┐   │
│  │  Backend Service (Python Flask / FastAPI / Node.js)                       │   │
│  │  ├── REST API (Dashboard & Mobile App)                                    │   │
│  │  ├── ML Inference Service (Prediksi Pengurasan)                           │   │
│  │  ├── Alerting Engine (Threshold Rules + Notification Dispatcher)          │   │
│  │  └── Data Retention & Aggregation Jobs                                    │   │
│  └──────────────────────────────────────────────────────────────────────────┘   │
│                                    │                                             │
│  ┌─────────────────────────────────┴──────────────────────────────────────────┐   │
│  │  Time-Series Database (InfluxDB / TimescaleDB / MongoDB Time-Series)      │   │
│  │  ├── Bucket: raw_sensor_data (retention: 90 hari)                         │   │
│  │  ├── Bucket: aggregated_hourly (retention: 1 tahun)                       │   │
│  │  └── Bucket: alert_logs (retention: 2 tahun)                              │   │
│  └──────────────────────────────────────────────────────────────────────────┘   │
│                                    │                                             │
│  ┌─────────────────────────────────┴──────────────────────────────────────────┐   │
│  │  Dashboard & Visualization                                                │   │
│  │  ├── Grafana (Ops Dashboard)                                              │   │
│  │  └── Web Dashboard (React/Next.js atau Vue.js)                            │   │
│  │       ├── Real-time Charts (WebSocket / SSE)                              │   │
│  │       ├── Alert History & Acknowledgment                                  │   │
│  │       └── Prediction Calendar                                             │   │
│  └──────────────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────────────┘
```

### 4.2 Parameter Sensor & Spesifikasi Teknis

| Parameter | Sensor | Principle | Range | Resolusi | Sampling Rate |
|-----------|--------|-----------|-------|----------|---------------|
| Level Kotoran | JSN-SR04T | Ultrasonic Time-of-Flight | 20–450 cm | 0.3 cm | 30 detik |
| Hidrogen Sulfida (H2S) | MQ-136 | Semiconductor MOS | 1–200 ppm | 1 ppm | 60 detik |
| Methane (CH4) | MQ-4 | Semiconductor MOS | 300–10000 ppm | 100 ppm | 60 detik |
| Ammonia (NH3) | MQ-137 | Semiconductor MOS | 5–500 ppm | 5 ppm | 60 detik |
| Suhu | DHT22 | Capacitive / Thermistor | -40°C – 80°C | 0.1°C | 30 detik |
| Kelembaban | DHT22 | Capacitive | 0% – 100% RH | 0.1% RH | 30 detik |
| pH | pH-4502C + Probe BNC | Glass Electrode | 0 – 14 pH | 0.01 pH | 5 menit |
| Turbidity/TSS | Turbidity Sensor Module | Optical Scattering | 0 – 1000 NTU | 10 NTU | 5 menit |

### 4.3 Edge Device & Wiring Diagram (Konseptual)

**Microcontroller**: ESP32-WROOM-32 (Dual-core 240 MHz, WiFi + Bluetooth, 4 MB Flash, 520 KB SRAM).

**Multiplexer ADC**: ADS1115 (I2C, 16-bit, 4 differential/8 single-ended channel) diperlukan karena ESP32 memiliki 1 ADC internal dengan resolusi 12-bit yang tidak cukup untuk akurasi pH dan turbidity, serta GPIO terbatas.

**Pin Mapping (Konseptual)**:
- JSN-SR04T: Trigger → GPIO 5, Echo → GPIO 18 (5V logic, voltage divider pada echo jika diperlukan).
- DHT22: Data → GPIO 19 (dengan pull-up 10kΩ).
- MQ-136: Analog → ADS1115 Channel 0 (A0). Heater → 5V kontinu (pre-heat 24 jam sebelum kalibrasi).
- MQ-4: Analog → ADS1115 Channel 1 (A1). Heater → 5V kontinu.
- MQ-137: Analog → ADS1115 Channel 2 (A2). Heater → 5V kontinu.
- pH-4502C: Analog pH → ADS1115 Channel 3 (A3). Temperature compensation → DHT22 suhu (software).
- Turbidity: Analog → ADC internal ESP32 (GPIO 34) atau ADS1115 dengan multiplexer tambahan (CD74HC4067).
- I2C Bus (ADS1115): SDA → GPIO 21, SCL → GPIO 22.

**Catatan Gas Sensor (MQ Series)**:
- Sensor MQ memerlukan **pre-heating 24–48 jam** sebelum penggunaan agar heater stabil.
- Kalibrasi dilakukan di udara bersih (Ro = Rs di udara bersih) dan dikoreksi dengan suhu/kelembaban (DHT22).
- Formula konsentrasi: `ppm = A × (Rs/Ro)^B` di mana A dan B diperoleh dari datasheet kurva sensitivitas.

### 4.4 Power Management (Outdoor Deployment)

Septic tank monitoring berada di outdoor dengan ketersediaan listrik AC yang tidak stabil atau tidak tersedia. Sistem dirancang untuk **autonomous power**:

| Komponen | Spesifikasi | Estimasi Daya |
|----------|-------------|---------------|
| Solar Panel | 12V / 20W Monocrystalline | Sumber utama |
| Battery | 12V / 7Ah AGM Deep Cycle | Backup & night |
| Charge Controller | MPPT 5A, 12V | Efisiensi > 95% |
| Buck Converter | LM2596, 12V → 5V @ 3A | Supply ESP32 & sensor |
| Daya Total | ESP32 aktif + sensor + WiFi TX | ~1.5W idle, ~3W peak |
| Estimasi Autonomy | Tanpa sinar matahari | > 48 jam dengan baterai penuh |

**Power Strategy**:
- ESP32 menggunakan **Deep Sleep** antara sampling interval (30–60 detik) untuk menghemat daya.
- WiFi PHY dimatikan (WiFi.disconnect()) selama deep sleep; bangkit, connect, publish, lalu sleep lagi.
- Heater MQ-series **TIDAK** dimatikan selama operasi karena memerlukan suhu kerja stabil (perlu redesign ke solid-state MOS alternatif untuk versi battery-only, atau gunakan panel solar cukup besar).

### 4.5 Connectivity & Protocol

- **Lokal**: ESP32 terhubung ke Access Point WiFi rumah/komplek.
- **Protokol**: MQTT (Message Queuing Telemetry Transport) dengan QoS 1 (at least once delivery).
- **Payload**: JSON dengan struktur:
  ```json
  {
    "node_id": " septic-tank-001",
    "timestamp": "2026-04-22T15:14:15Z",
    "level_cm": 145.5,
    "level_pct": 72.5,
    "h2s_ppm": 3.2,
    "ch4_ppm": 420,
    "nh3_ppm": 8.1,
    "temp_c": 31.2,
    "humidity_pct": 78.0,
    "ph": 7.45,
    "turbidity_ntu": 45
  }
  ```
- **Fallback**: Jika WiFi tidak tersedia, simpan ke microSD module (SPI) dan upload batch ketika koneksi pulih.
- **Cluster Scale**: Untuk komplek perumahan, pertimbangkan **LoRaWAN + Gateway** agar tidak bergantung pada WiFi per unit.

### 4.6 Backend & Database

- **MQTT Broker**: Mosquitto (open-source, lightweight) atau EMQX (skalabilitas tinggi).
- **Telegraf / Node-RED**: MQTT subscriber yang memparse JSON dan menulis ke InfluxDB.
- **InfluxDB**:
  - Bucket `raw_data`: retention 90 hari, precision detik.
  - Bucket `hourly_avg`: retention 1 tahun, downsampling continuous query.
- **API Backend**: FastAPI (async Python) menyediakan endpoint REST untuk dashboard.
- **ML Service**: Python script (scikit-learn / Prophet) yang berjalan sebagai cron job harian untuk memprediksi level penuh.

### 4.7 Dashboard & Mobile

- **Web Dashboard** (React / Next.js):
  - Halaman Overview: peta lokasi node, status summary.
  - Halaman Detail Node: real-time gauge (level, gas, suhu), grafik historis (1 jam / 24 jam / 7 hari / 30 hari).
  - Halaman Prediksi: kalender estimasi pengurasan.
  - Halaman Alert: log alarm, ack/mute, threshold settings.
- **Mobile**: Responsive PWA (Progressive Web App) atau Telegram Bot minimal (command `/status`, `/history`, `/predict`).

### 4.8 Security & Privacy

- **MQTT**: Aktifkan TLS/SSL (port 8883) dan autentikasi username/password atau certificate-based auth (mTLS).
- **API**: JWT token untuk autentikasi dashboard; rate limiting pada endpoint publik.
- **Firmware**: Secure Boot + Flash Encryption pada ESP32; OTA update yang ditandatangani (signed firmware image).
- **Data**: Data sensor pribadi (level kotoran) dianggap sebagai data non-PII secara regulasi, namun tetap amankan dengan enkripsi transit dan at-rest.

---

## 5. Risks & Roadmap

### 5.1 Phased Rollout

#### Phase MVP (0–6 Minggu)
- 1 unit prototype hardware (ESP32 + JSN-SR04T + MQ-136 + DHT22).
- Backend: Mosquitto MQTT + InfluxDB + Grafana dasar.
- Dashboard: Grafana panel saja; alerting via Telegram Bot.
- Testing: 2 minggu deployment di 1 septic tank aktual.

#### Phase v1.1 (7–12 Minggu)
- Tambah sensor CH4, NH3, pH, Turbidity.
- Pindah dashboard ke custom web app (Next.js) dengan UI lebih baik.
- Implementasi prediksi ML dasar (linear regression / moving average) untuk level penuh.
- Multi-node support (3–5 unit).

#### Phase v2.0 (13–24 Minggu)
- Skalasi ke LoRaWAN untuk cluster > 20 node.
- Advanced ML: Prophet / LSTM untuk prediksi dengan musim dan pola penggunaan air.
- Mobile App PWA dengan push notification.
- Kalibrasi otomatis gas sensor menggunakan baseline tracking.

### 5.2 Technical Risks

| Risk | Likelihood | Impact | Mitigasi |
|------|------------|--------|----------|
| **Sensor Gas Drift** | Tinggi | Tinggi | Kalibrasi berkala tiap 3 bulan; logging baseline Ro; pertimbangkan electrochemical sensor (ME3-H2S) sebagai upgrade |
| **Kondorisi Korosif & Lembab** | Tinggi | Tinggi | IP67 enclosure; conformal coating pada PCB; venting membran hydrophobic untuk gas sensor |
| **WiFi Range Outdoor** | Sedang | Sedang | Gunakan WiFi extender / outdoor AP; fallback batch logging ke SD card |
| **Dayauta Autonomous Power** | Sedang | Sedang | Oversize solar panel 30W+; battery monitor BMS; low-power firmware deep sleep |
| **Latency MQTT / Jaringan** | Rendah | Sedang | QoS 1; local buffering; message queue pada edge |
| **Akurasi ML Prediksi Awal** | Tinggi | Sedang | Mulai dengan rule-based heuristic (kecepatan naik level rata-rata); kumpulkan data 3 bulan sebelum train model kompleks |

---

## 6. Appendix: Bill of Materials (BOM) Prototype

| Item | Qty | Est. Harga (IDR) | Keterangan |
|------|-----|------------------|------------|
| ESP32-WROOM-32 DevKit | 1 | 50.000 | Microcontroller utama |
| JSN-SR04T Waterproof | 1 | 75.000 | Level sensor, tahan air |
| MQ-136 (H2S) | 1 | 85.000 | Gas sensor |
| MQ-4 (CH4) | 1 | 45.000 | Methane sensor |
| MQ-137 (NH3) | 1 | 65.000 | Ammonia sensor |
| DHT22 | 1 | 35.000 | Suhu & kelembaban |
| pH-4502C + Probe BNC | 1 | 180.000 | pH module + probe |
| Turbidity Sensor Module | 1 | 95.000 | Analog turbidity |
| ADS1115 ADC I2C | 1 | 35.000 | 16-bit multiplexer |
| Solar Panel 20W 12V | 1 | 250.000 | Power source |
| Battery AGM 12V 7Ah | 1 | 180.000 | Backup power |
| MPPT Controller 5A | 1 | 120.000 | Charge controller |
| LM2596 Buck Converter | 2 | 25.000 | 12V → 5V |
| IP67 Enclosure | 1 | 95.000 | Outdoor housing |
| PCB Breadboard / Proto | 1 | 25.000 | Wiring |
| **Total Estimasi** | | **~1.370.000** | Per unit prototype |

---

*Dokumen ini disusun sebagai Product Requirements Document untuk proyek akademik/riset Sistem Monitoring Kondisi Septic Tank Berbasis IoT. Spesifikasi teknis dapat disesuaikan dengan ketersediaan komponen, constraint biaya, dan kebutuhan akhir dari pemangku kepentingan.*

**Co-Authored-By: Oz <oz-agent@warp.dev>**

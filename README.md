# IoT Acoustic Sensing & Telemetry System (ESP32 + Docker + SQLite + Dashboard)

## Project Overview
This project implements an end-to-end IoT acoustic telemetry solution:
- **ESP32 Microcontroller** reading analog sound pressure from a **KY-038 sensor module**.
- **Edge Actuation**: Automatic trip detection activating an indicator LED on GPIO 18 when sound exceeds threshold.
- **Data Communication**: Wi-Fi enabled HTTP POST transmission with structured JSON telemetry payloads.
- **Containerized Backend**: Dockerized Python backend persisting data to SQLite and exposing REST endpoints.
- **Real-Time Web Dashboard**: Interactive UI featuring live sound level gauge, dynamic Chart.js waveform, alert badges, and database audit logs.

---

## Hardware Pin Connections

| KY-038 Pin | ESP32 Pin | Description |
|---|---|---|
| **AO** (Analog Out) | **GPIO 34** | ADC1 Channel 6 (12-bit, 0-4095 range) |
| **VCC** | **3.3V** | Power Supply |
| **GND** | **GND** | Common Ground |
| **LED (+ Anode)** | **GPIO 18** | Driven via 220Ω resistor |
| **LED (- Cathode)** | **GND** | Ground |

---

## Quick Start Guide

### 1. Run Backend with Docker
```bash
docker compose up --build -d
```
The backend and web dashboard will be available at:
`http://localhost:8000` (or `http://<YOUR_COMPUTER_IP>:8000`)

### 2. Run Backend Directly (Without Docker)
If Docker is not running on your host machine, you can run the server directly:
```bash
python backend/server.py
```

### 3. Flash the ESP32
1. Open [code.ino](file:///c:/Users/sneha/Downloads/Iot-project/code.ino) in Arduino IDE.
2. Select your ESP32 board (`DOIT ESP32 DEVKIT V1` or generic `ESP32 Dev Module`).
3. Update `ssid` and `password` with your Wi-Fi credentials:
   ```cpp
   const char* ssid = "YOUR_WIFI_NAME";
   const char* password = "YOUR_WIFI_PASSWORD";
   ```
4. Set `serverUrl` to your computer's local IP address (e.g. `http://10.141.184.43:8000/api/sensor-data`).
5. Click **Upload** and open the Serial Monitor at **115200 baud**.

### 4. Test with Sensor Simulator (Optional)
To test and demonstrate the pipeline without physical ESP32 flashing:
```bash
python simulate_sensor.py
```
Open `http://localhost:8000` in any browser to watch live telemetry and database ingestion in real time.


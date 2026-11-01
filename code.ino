#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ======================== WiFi & Backend Config ========================
const char* ssid = "Moto";          // Replace with your WiFi network name
const char* password = "Sneha@033";  // Replace with your WiFi password

// Backend server URL (Your computer/server's local IP address and port 8000)
// Example: "http://192.168.1.100:8000/api/sensor-data" or "http://10.141.184.43:8000/api/sensor-data"
const char* serverUrl = "http://10.141.184.43:8000/api/sensor-data";

// ======================== Pin Configuration ============================
const int SOUND_SENSOR_PIN = 34;  // KY-038 AO (Analog Output)
const int LED_PIN = 18;           // Green LED via current limiting resistor

// ======================== Operational Thresholds =======================
// Adjust threshold after checking ambient room readings (KY-038 ADC: 0 to 4095)
const int SOUND_THRESHOLD = 970;

// LED hold time after the last detected sound (in milliseconds)
const unsigned long HOLD_TIME_MS = 600;

// Telemetry interval to backend (e.g. transmit reading every 500ms or 1000ms)
const unsigned long TELEMETRY_INTERVAL_MS = 500;

// ======================== Variables ====================================
bool ledState = false;
bool soundWasDetected = false;

unsigned long lastSoundTime = 0;
unsigned long lastPrintTime = 0;
unsigned long lastTelemetryTime = 0;

void connectToWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected successfully!");
    Serial.print("ESP32 IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi connection failed! Running in offline mode; will retry periodically.");
  }
}

void sendSensorDataToBackend(int soundVal, int thresholdVal, bool currentLedState) {
  if (WiFi.status() != WL_CONNECTED) {
    // Attempt non-blocking reconnect if disconnected
    WiFi.reconnect();
    return;
  }

  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/json");

  // Construct JSON payload: {"sound_level": 850, "threshold": 960, "led_state": 0, "device_id": "ESP32_Sound_01"}
  String payload = "{";
  payload += "\"sound_level\":" + String(soundVal) + ",";
  payload += "\"threshold\":" + String(thresholdVal) + ",";
  payload += "\"led_state\":" + String(currentLedState ? 1 : 0) + ",";
  payload += "\"device_id\":\"ESP32_KY038_Node\"";
  payload += "}";

  int httpResponseCode = http.POST(payload);

  if (httpResponseCode > 0) {
    // 200 OK
    // Serial.printf("HTTP POST Success: %d\n", httpResponseCode);
  } else {
    Serial.printf("HTTP POST Error: %s (code %d)\n", http.errorToString(httpResponseCode).c_str(), httpResponseCode);
  }

  http.end();
}

void setup() {
  Serial.begin(115200);

  pinMode(SOUND_SENSOR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);

  digitalWrite(LED_PIN, LOW);

  delay(1000);

  Serial.println();
  Serial.println("================================================");
  Serial.println("  IoT Sound Monitoring System (KY-038 + ESP32)  ");
  Serial.println("================================================");

  // Initialize WiFi connection
  connectToWiFi();

  Serial.println("Reading KY-038 analog output & transmitting telemetry...");
}

void loop() {
  unsigned long now = millis();

  // 1. Read analog sound level (ESP32 ADC 12-bit: 0 - 4095)
  int soundValue = analogRead(SOUND_SENSOR_PIN);

  // 2. Local threshold detection & LED actuation
  if (soundValue > SOUND_THRESHOLD) {
    lastSoundTime = now;
    soundWasDetected = true;

    if (!ledState) {
      ledState = true;
      digitalWrite(LED_PIN, HIGH);
      Serial.println(">>> Sound detected - LED ON <<<");
    }
  }

  // Turn off after the sound level stays below threshold for HOLD_TIME_MS
  if (ledState && soundWasDetected && (now - lastSoundTime >= HOLD_TIME_MS)) {
    ledState = false;
    digitalWrite(LED_PIN, LOW);
    soundWasDetected = false;
    Serial.println("--- Quiet detected - LED OFF ---");
  }

  // 3. Periodic local Serial print
  if (now - lastPrintTime >= 200) {
    lastPrintTime = now;
    Serial.print("Sound level: ");
    Serial.print(soundValue);
    Serial.print(" | Threshold: ");
    Serial.print(SOUND_THRESHOLD);
    Serial.print(" | LED: ");
    Serial.println(ledState ? "ON" : "OFF");
  }

  // 4. Send telemetry data to Containerized Backend over HTTP/JSON
  if (now - lastTelemetryTime >= TELEMETRY_INTERVAL_MS) {
    lastTelemetryTime = now;
    sendSensorDataToBackend(soundValue, SOUND_THRESHOLD, ledState);
  }

  delay(5);
}

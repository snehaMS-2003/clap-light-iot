#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ======================== WiFi & Backend Config ========================
const char* ssid = "Moto";
const char* password = "Sneha@033";

// Public Render Backend URL
const char* serverUrl = "https://clap-light-iot.onrender.com/api/sensor-data";



// ======================== Pin Configuration ============================
const int SOUND_SENSOR_PIN = 34;  // KY-038 AO
const int LED_PIN = 18;           // Green LED

const int SOUND_THRESHOLD = 960;
const unsigned long HOLD_TIME_MS = 600;
const unsigned long TELEMETRY_INTERVAL_MS = 500;

bool ledState = false;
bool soundWasDetected = false;

unsigned long lastSoundTime = 0;
unsigned long lastPrintTime = 0;
unsigned long lastTelemetryTime = 0;

void connectToWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);

  WiFi.disconnect(true);
  delay(500);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected successfully!");
    Serial.print("ESP32 IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi Connection attempt timed out. Continuing in offline mode...");
  }
}

void sendSensorDataToBackend(int soundVal, int thresholdVal, bool currentLedState) {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  HTTPClient http;
  http.setTimeout(2000);
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/json");

  String payload = "{";
  payload += "\"sound_level\":" + String(soundVal) + ",";
  payload += "\"threshold\":" + String(thresholdVal) + ",";
  payload += "\"led_state\":" + String(currentLedState ? 1 : 0) + ",";
  payload += "\"device_id\":\"ESP32_Hardware_COM11\"";
  payload += "}";

  int httpResponseCode = http.POST(payload);

  if (httpResponseCode > 0) {
    Serial.printf("[HTTP] POST OK (%d)\n", httpResponseCode);
  } else {
    Serial.printf("[HTTP] POST failed: %s (%d)\n", http.errorToString(httpResponseCode).c_str(), httpResponseCode);
  }

  http.end();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(SOUND_SENSOR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.println("\n--- ESP32 KY-038 Sound Sensor Node Starting ---");
  connectToWiFi();
}

void loop() {
  unsigned long now = millis();

  // Read analog sound level
  int soundValue = analogRead(SOUND_SENSOR_PIN);

  // Threshold detection & LED actuation
  if (soundValue > SOUND_THRESHOLD) {
    lastSoundTime = now;
    soundWasDetected = true;

    if (!ledState) {
      ledState = true;
      digitalWrite(LED_PIN, HIGH);
      Serial.println(">>> Sound detected - LED ON <<<");
    }
  }

  if (ledState && soundWasDetected && (now - lastSoundTime >= HOLD_TIME_MS)) {
    ledState = false;
    digitalWrite(LED_PIN, LOW);
    soundWasDetected = false;
    Serial.println("--- Quiet detected - LED OFF ---");
  }

  // Serial debug print every 1 second
  if (now - lastPrintTime >= 1000) {
    lastPrintTime = now;
    Serial.printf("Sound: %d | LED: %s | WiFi: %s\n", 
      soundValue, 
      ledState ? "ON" : "OFF", 
      WiFi.status() == WL_CONNECTED ? "CONNECTED" : "DISCONNECTED"
    );
  }

  // Send telemetry
  if (now - lastTelemetryTime >= TELEMETRY_INTERVAL_MS) {
    lastTelemetryTime = now;
    sendSensorDataToBackend(soundValue, SOUND_THRESHOLD, ledState);
  }

  delay(5);
}

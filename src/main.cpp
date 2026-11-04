#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// ======================== WiFi & Backend Config ========================
// Current active open Wi-Fi network (no password)
const char* ssid = "Moto 2";
const char* password = "";

// Public Render Backend endpoint
const char* serverUrl = "https://clap-light-iot.onrender.com/api/sensor-data";

// Fallback local IP backend endpoint (in case hotspot isolates WAN/LAN)
const char* localServerUrl = "http://10.141.184.43:8000/api/sensor-data";

// ======================== Pin Configuration ============================
const int SOUND_SENSOR_PIN = 34;  // KY-038 AO (Analog Out)
const int LED_PIN = 18;           // Indicator Green LED

const int SOUND_THRESHOLD = 960;
const unsigned long HOLD_TIME_MS = 600;
const unsigned long TELEMETRY_INTERVAL_MS = 300; // Fast 300ms stream for responsive dashboard

bool ledState = false;
bool soundWasDetected = false;

unsigned long lastSoundTime = 0;
unsigned long lastPrintTime = 0;
unsigned long lastTelemetryTime = 0;

void connectToWiFi() {
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(ssid);

  WiFi.disconnect(true);
  delay(500);

  WiFi.mode(WIFI_STA);
  // Open network connection
  WiFi.begin(ssid, NULL);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(400);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n>>> Wi-Fi Connected Successfully! <<<");
    Serial.print("ESP32 IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal Strength (RSSI): ");
    Serial.println(WiFi.RSSI());
  } else {
    Serial.println("\n[!] Wi-Fi connection attempt timed out; will keep retrying in loop.");
  }
}

void sendSensorDataToBackend(int soundVal, int thresholdVal, bool currentLedState) {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    return;
  }

  // Use WiFiClientSecure with setInsecure to allow rapid SSL posting without root certificate bloat
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(3);

  HTTPClient http;
  if (!http.begin(client, serverUrl)) {
    Serial.println("[HTTP] begin failed on Render URL");
    return;
  }

  http.addHeader("Content-Type", "application/json");
  http.setUserAgent("ESP32-KY038-Device");

  // Construct JSON Payload
  String payload = "{";
  payload += "\"sound_level\":" + String(soundVal) + ",";
  payload += "\"threshold\":" + String(thresholdVal) + ",";
  payload += "\"led_state\":" + String(currentLedState ? 1 : 0) + ",";
  payload += "\"device_id\":\"ESP32_Hardware_COM11\"";
  payload += "}";

  int httpCode = http.POST(payload);

  if (httpCode > 0) {
    // 200 OK
  } else {
    Serial.printf("[HTTP] POST error on Render: %s (code %d)\n", http.errorToString(httpCode).c_str(), httpCode);
    
    // Also try local backend endpoint if available
    HTTPClient localHttp;
    if (localHttp.begin(localServerUrl)) {
      localHttp.addHeader("Content-Type", "application/json");
      localHttp.POST(payload);
      localHttp.end();
    }
  }

  http.end();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(SOUND_SENSOR_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.println("\n=======================================================");
  Serial.println("  ESP32 KY-038 Sound Telemetry System Initializing...  ");
  Serial.println("=======================================================");

  connectToWiFi();
}

void loop() {
  unsigned long now = millis();

  // 1. Read analog sound level
  int soundValue = analogRead(SOUND_SENSOR_PIN);

  // 2. Threshold check & LED actuation
  if (soundValue > SOUND_THRESHOLD) {
    lastSoundTime = now;
    soundWasDetected = true;

    if (!ledState) {
      ledState = true;
      digitalWrite(LED_PIN, HIGH);
      Serial.println(">>> SOUND TRIGGERED - LED ON <<<");
    }
  }

  if (ledState && soundWasDetected && (now - lastSoundTime >= HOLD_TIME_MS)) {
    ledState = false;
    digitalWrite(LED_PIN, LOW);
    soundWasDetected = false;
    Serial.println("--- Quiet - LED OFF ---");
  }

  // 3. Periodic local serial print every 1000ms
  if (now - lastPrintTime >= 1000) {
    lastPrintTime = now;
    Serial.printf("Sound: %d | LED: %s | WiFi: %s (%s)\n",
      soundValue,
      ledState ? "ON" : "OFF",
      WiFi.status() == WL_CONNECTED ? "CONNECTED" : "CONNECTING...",
      ssid
    );
  }

  // 4. Send telemetry packet every 300ms
  if (now - lastTelemetryTime >= TELEMETRY_INTERVAL_MS) {
    lastTelemetryTime = now;
    sendSensorDataToBackend(soundValue, SOUND_THRESHOLD, ledState);
  }

  delay(5);
}

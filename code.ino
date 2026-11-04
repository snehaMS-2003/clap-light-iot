#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// ======================== WiFi & Backend Config ========================
const char* ssid = "Moto 2";
const char* password = "";

const char* serverUrl = "https://clap-light-iot.onrender.com/api/sensor-data";
const char* localServerUrl = "http://10.141.184.43:8000/api/sensor-data";

// ======================== Pin Configuration ============================
const int SOUND_SENSOR_PIN = 34;  // KY-038 AO
const int LED_PIN = 18;           // Green LED

const int SOUND_THRESHOLD = 960;
const unsigned long HOLD_TIME_MS = 600;
const unsigned long TELEMETRY_INTERVAL_MS = 300;

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
  } else {
    Serial.println("\n[!] Wi-Fi connection timed out.");
  }
}

void sendSensorDataToBackend(int soundVal, int thresholdVal, bool currentLedState) {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(3);

  HTTPClient http;
  if (!http.begin(client, serverUrl)) {
    return;
  }

  http.addHeader("Content-Type", "application/json");
  http.setUserAgent("ESP32-KY038-Device");

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

  connectToWiFi();
}

void loop() {
  unsigned long now = millis();

  int soundValue = analogRead(SOUND_SENSOR_PIN);

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

  if (now - lastPrintTime >= 1000) {
    lastPrintTime = now;
    Serial.printf("Sound: %d | LED: %s | WiFi: %s\n",
      soundValue,
      ledState ? "ON" : "OFF",
      WiFi.status() == WL_CONNECTED ? "CONNECTED" : "DISCONNECTED"
    );
  }

  if (now - lastTelemetryTime >= TELEMETRY_INTERVAL_MS) {
    lastTelemetryTime = now;
    sendSensorDataToBackend(soundValue, SOUND_THRESHOLD, ledState);
  }

  delay(5);
}

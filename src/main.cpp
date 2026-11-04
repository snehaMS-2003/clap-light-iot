#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// ======================== WiFi & Backend Config ========================
const char* ssid = "Moto 2";
const int targetChannel = 1;

const char* serverUrl = "https://clap-light-iot.onrender.com/api/sensor-data";
const char* localServerUrl = "http://10.141.184.43:8000/api/sensor-data";

// ======================== Pin Configuration ============================
const int SOUND_SENSOR_PIN = 34;  // KY-038 AO
const int LED_PIN = 18;           // Green LED

const int SOUND_THRESHOLD = 960;
const unsigned long HOLD_TIME_MS = 600;
const unsigned long TELEMETRY_INTERVAL_MS = 350;

bool ledState = false;
bool soundWasDetected = false;

unsigned long lastSoundTime = 0;
unsigned long lastPrintTime = 0;
unsigned long lastTelemetryTime = 0;

void connectOpenWiFi() {
  Serial.print("Connecting to Open Wi-Fi [");
  Serial.print(ssid);
  Serial.println("] on Channel 1...");

  WiFi.persistent(false);
  WiFi.disconnect(true, true);
  delay(300);

  WiFi.mode(WIFI_STA);
  // Specifically connect with channel and open authentication
  WiFi.begin(ssid, "", targetChannel);

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 25) {
    delay(400);
    Serial.print(".");
    tries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n>>> SUCCESS: Wi-Fi Connected! <<<");
    Serial.print("ESP32 IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.printf("\n[!] Status code: %d. Retrying in background...\n", WiFi.status());
  }
}

void sendTelemetry(int soundVal, int thresholdVal, bool currentLedState) {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(2);

  HTTPClient http;
  if (!http.begin(client, serverUrl)) {
    return;
  }

  http.addHeader("Content-Type", "application/json");
  http.setUserAgent("ESP32-KY038-Telemetry");

  String payload = "{";
  payload += "\"sound_level\":" + String(soundVal) + ",";
  payload += "\"threshold\":" + String(thresholdVal) + ",";
  payload += "\"led_state\":" + String(currentLedState ? 1 : 0) + ",";
  payload += "\"device_id\":\"ESP32_Hardware_COM11\"";
  payload += "}";

  int httpCode = http.POST(payload);

  if (httpCode > 0) {
    // Serial.printf("[HTTP] POST OK (%d)\n", httpCode);
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

  Serial.println("\n================================================");
  Serial.println("  ESP32 KY-038 Sound Monitoring & Alert System  ");
  Serial.println("================================================");

  connectOpenWiFi();
}

void loop() {
  unsigned long now = millis();

  // 1. Read sound sensor
  int soundValue = analogRead(SOUND_SENSOR_PIN);

  // 2. Threshold Detection & LED Actuation
  if (soundValue > SOUND_THRESHOLD) {
    lastSoundTime = now;
    soundWasDetected = true;

    if (!ledState) {
      ledState = true;
      digitalWrite(LED_PIN, HIGH);
      Serial.println(">>> SOUND ALERT: LED ON <<<");
    }
  }

  if (ledState && soundWasDetected && (now - lastSoundTime >= HOLD_TIME_MS)) {
    ledState = false;
    digitalWrite(LED_PIN, LOW);
    soundWasDetected = false;
    Serial.println("--- Sound Normal: LED OFF ---");
  }

  // 3. Periodic debug logging
  if (now - lastPrintTime >= 1500) {
    lastPrintTime = now;
    Serial.printf("[Telemetry] Sound: %4d | LED: %-3s | Wi-Fi: %s\n",
      soundValue,
      ledState ? "ON" : "OFF",
      WiFi.status() == WL_CONNECTED ? "CONNECTED" : "RECONNECTING..."
    );
  }

  // 4. Auto-reconnect check
  if (WiFi.status() != WL_CONNECTED && (now % 10000 < 50)) {
    WiFi.begin(ssid, "", targetChannel);
  }

  // 5. Send sensor telemetry
  if (now - lastTelemetryTime >= TELEMETRY_INTERVAL_MS) {
    lastTelemetryTime = now;
    sendTelemetry(soundValue, SOUND_THRESHOLD, ledState);
  }

  delay(5);
}

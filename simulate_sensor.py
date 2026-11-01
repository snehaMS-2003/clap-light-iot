"""
Simulator Script: Generates realistic acoustic sensor telemetry for testing the IoT pipeline
without requiring immediate physical ESP32 flashing.
"""
import time
import random
import urllib.request
import json

BACKEND_URL = "http://localhost:8000/api/sensor-data"
THRESHOLD = 960

print(f"[*] Starting ESP32 KY-038 Telemetry Simulator...")
print(f"[*] Target Endpoint: {BACKEND_URL}")
print(f"[*] Press Ctrl+C to stop.\n")

counter = 0
led_state = 0
spike_cooldown = 0

while True:
    counter += 1
    
    # Simulate ambient background sound noise around 700-900
    base_sound = int(random.gauss(820, 45))
    
    # Occasional sound impulse / spike (clap, voice, knock)
    if random.random() < 0.12 and spike_cooldown == 0:
        sound_val = random.randint(970, 1450)
        spike_cooldown = 3
    else:
        sound_val = max(100, min(base_sound, 4095))
        if spike_cooldown > 0:
            spike_cooldown -= 1

    # Threshold evaluation
    if sound_val > THRESHOLD:
        led_state = 1
    elif spike_cooldown == 0:
        led_state = 0

    payload = {
        "sound_level": sound_val,
        "threshold": THRESHOLD,
        "led_state": led_state,
        "device_id": "ESP32_KY038_Node_Simulated"
    }

    try:
        req = urllib.request.Request(
            BACKEND_URL,
            data=json.dumps(payload).encode('utf-8'),
            headers={'Content-Type': 'application/json'}
        )
        with urllib.request.urlopen(req, timeout=2) as resp:
            status = resp.status
            print(f"[Telemetry #{counter:04d}] Sound: {sound_val:4d} | Alert LED: {('ON' if led_state else 'OFF'):3s} | HTTP: {status}")
    except Exception as e:
        print(f"[Telemetry #{counter:04d}] Error sending to backend: {e}")

    time.sleep(0.5)


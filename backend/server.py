import os
import json
import sqlite3
import threading
from datetime import datetime
from http.server import HTTPServer, SimpleHTTPRequestHandler
import urllib.parse

PORT = 8000
DB_PATH = os.getenv("DB_PATH", os.path.join(os.path.dirname(__file__), "data", "iot_sensor.db"))
STATIC_DIR = os.path.join(os.path.dirname(__file__), "static")

os.makedirs(os.path.dirname(DB_PATH), exist_ok=True)

# Database Initialization
def init_db():
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS sensor_readings (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            sound_level INTEGER NOT NULL,
            threshold INTEGER NOT NULL,
            led_state INTEGER NOT NULL,
            device_id TEXT DEFAULT 'ESP32_KY038_Node',
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    """)
    conn.commit()
    conn.close()

init_db()

# In-memory latest record for lightweight polling/updates
latest_cache = {
    "sound_level": 0,
    "threshold": 960,
    "led_state": False,
    "device_id": "ESP32_KY038_Node",
    "timestamp": "--"
}

db_lock = threading.Lock()

class IoTRequestHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=STATIC_DIR, **kwargs)

    def _send_cors_headers(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")

    def do_OPTIONS(self):
        self.send_response(200)
        self._send_cors_headers()
        self.end_headers()

    def do_POST(self):
        if self.path == "/api/sensor-data":
            content_length = int(self.headers.get("Content-Length", 0))
            post_body = self.rfile.read(content_length)
            
            try:
                data = json.loads(post_body.decode("utf-8"))
                sound_level = int(data.get("sound_level", 0))
                threshold = int(data.get("threshold", 960))
                led_state = int(data.get("led_state", 0))
                device_id = str(data.get("device_id", "ESP32_KY038_Node"))
                timestamp = datetime.utcnow().strftime("%Y-%m-%d %H:%M:%S")

                with db_lock:
                    conn = sqlite3.connect(DB_PATH)
                    cursor = conn.cursor()
                    cursor.execute(
                        """
                        INSERT INTO sensor_readings (sound_level, threshold, led_state, device_id, timestamp)
                        VALUES (?, ?, ?, ?, ?)
                        """,
                        (sound_level, threshold, led_state, device_id, timestamp)
                    )
                    conn.commit()
                    reading_id = cursor.lastrowid
                    conn.close()

                # Update cache
                latest_cache.update({
                    "id": reading_id,
                    "sound_level": sound_level,
                    "threshold": threshold,
                    "led_state": bool(led_state),
                    "device_id": device_id,
                    "timestamp": timestamp
                })

                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self._send_cors_headers()
                self.end_headers()
                response = {"status": "success", "id": reading_id}
                self.wfile.write(json.dumps(response).encode("utf-8"))
            except Exception as e:
                self.send_response(400)
                self._send_cors_headers()
                self.end_headers()
                self.wfile.write(json.dumps({"error": str(e)}).encode("utf-8"))
        else:
            self.send_response(404)
            self.end_headers()

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)

        if parsed.path == "/api/sensor-data/live":
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self._send_cors_headers()
            self.end_headers()
            self.wfile.write(json.dumps(latest_cache).encode("utf-8"))

        elif parsed.path == "/api/sensor-data/history":
            query = urllib.parse.parse_qs(parsed.query)
            limit = int(query.get("limit", [30])[0])

            with db_lock:
                conn = sqlite3.connect(DB_PATH)
                conn.row_factory = sqlite3.Row
                cursor = conn.cursor()
                cursor.execute(
                    """
                    SELECT id, sound_level, threshold, led_state, device_id, timestamp
                    FROM sensor_readings
                    ORDER BY id DESC
                    LIMIT ?
                    """,
                    (limit,)
                )
                rows = cursor.fetchall()
                conn.close()

            history = [
                {
                    "id": r["id"],
                    "sound_level": r["sound_level"],
                    "threshold": r["threshold"],
                    "led_state": bool(r["led_state"]),
                    "device_id": r["device_id"],
                    "timestamp": r["timestamp"]
                }
                for r in reversed(rows)
            ]

            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self._send_cors_headers()
            self.end_headers()
            self.wfile.write(json.dumps(history).encode("utf-8"))

        elif parsed.path == "/api/sensor-data/stats":
            with db_lock:
                conn = sqlite3.connect(DB_PATH)
                cursor = conn.cursor()
                cursor.execute("""
                    SELECT 
                        COUNT(*), 
                        AVG(sound_level), 
                        MAX(sound_level), 
                        MIN(sound_level),
                        SUM(CASE WHEN led_state = 1 THEN 1 ELSE 0 END)
                    FROM sensor_readings
                """)
                total, avg_val, max_val, min_val, alert_count = cursor.fetchone()
                conn.close()

            stats = {
                "total_readings": total or 0,
                "average_sound_level": round(avg_val, 1) if avg_val is not None else 0,
                "max_sound_level": max_val or 0,
                "min_sound_level": min_val or 0,
                "alert_count": alert_count or 0
            }

            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self._send_cors_headers()
            self.end_headers()
            self.wfile.write(json.dumps(stats).encode("utf-8"))

        elif parsed.path == "/" or parsed.path == "/index.html":
            index_path = os.path.join(STATIC_DIR, "index.html")
            if os.path.exists(index_path):
                self.send_response(200)
                self.send_header("Content-Type", "text/html")
                self.end_headers()
                with open(index_path, "rb") as f:
                    self.wfile.write(f.read())
            else:
                self.send_response(404)
                self.end_headers()
        else:
            # Fall back to static files
            super().do_GET()

def run_server():
    server = HTTPServer(("0.0.0.0", PORT), IoTRequestHandler)
    print(f"[*] IoT Backend Server running on http://0.0.0.0:{PORT}")
    server.serve_forever()

if __name__ == "__main__":
    run_server()


import os
import json
import sqlite3
from datetime import datetime
from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel
from typing import List, Optional

# App Initialization
app = FastAPI(title="IoT Sound Monitoring Backend")

# Allow CORS for dashboard access
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

DB_PATH = os.getenv("DB_PATH", "/app/data/iot_sensor.db")

# Ensure database directory exists
os.makedirs(os.path.dirname(DB_PATH), exist_ok=True)

def init_db():
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS sensor_readings (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            sound_level INTEGER NOT NULL,
            threshold INTEGER NOT NULL,
            led_state INTEGER NOT NULL,
            device_id TEXT DEFAULT 'ESP32_Sound_01',
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    """)
    conn.commit()
    conn.close()

init_db()

# Pydantic schema for ingestion
class SensorData(BaseModel):
    sound_level: int
    threshold: int
    led_state: int  # 0 or 1
    device_id: Optional[str] = "ESP32_Sound_01"

# In-memory connection manager for WebSocket clients
class ConnectionManager:
    def __init__(self):
        self.active_connections: List[WebSocket] = []

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        self.active_connections.append(websocket)

    def disconnect(self, websocket: WebSocket):
        if websocket in self.active_connections:
            self.active_connections.remove(websocket)

    async def broadcast(self, message: dict):
        for connection in list(self.active_connections):
            try:
                await connection.send_text(json.dumps(message))
            except Exception:
                self.disconnect(connection)

manager = ConnectionManager()

@app.post("/api/sensor-data")
async def receive_sensor_data(data: SensorData):
    timestamp = datetime.utcnow().strftime("%Y-%m-%d %H:%M:%S")
    
    # Save to SQLite database
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    cursor.execute(
        """
        INSERT INTO sensor_readings (sound_level, threshold, led_state, device_id, timestamp)
        VALUES (?, ?, ?, ?, ?)
        """,
        (data.sound_level, data.threshold, data.led_state, data.device_id, timestamp)
    )
    conn.commit()
    reading_id = cursor.lastrowid
    conn.close()

    payload = {
        "id": reading_id,
        "sound_level": data.sound_level,
        "threshold": data.threshold,
        "led_state": bool(data.led_state),
        "device_id": data.device_id,
        "timestamp": timestamp
    }

    # Broadcast real-time update to connected dashboard WebSocket clients
    await manager.broadcast(payload)
    return {"status": "success", "id": reading_id}

@app.get("/api/sensor-data/history")
async def get_history(limit: int = 50):
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
        for r in rows
    ]
    # Return chronologically ascending for charts
    return list(reversed(history))

@app.get("/api/sensor-data/stats")
async def get_stats():
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

    return {
        "total_readings": total or 0,
        "average_sound_level": round(avg_val, 1) if avg_val is not None else 0,
        "max_sound_level": max_val or 0,
        "min_sound_level": min_val or 0,
        "alert_count": alert_count or 0
    }

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    await manager.connect(websocket)
    try:
        while True:
            # Keep connection open / listen for heartbeat
            await websocket.receive_text()
    except WebSocketDisconnect:
        manager.disconnect(websocket)
    except Exception:
        manager.disconnect(websocket)

# Mount frontend static directory
static_dir = os.path.join(os.path.dirname(__file__), "static")
if os.path.exists(static_dir):
    app.mount("/", StaticFiles(directory=static_dir, html=True), name="static")


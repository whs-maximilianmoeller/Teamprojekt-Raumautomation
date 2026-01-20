
import threading
import time
import json
import sqlite3
import serial
import serial.tools.list_ports
from flask import Flask, jsonify, request, send_from_directory
from datetime import datetime

# --- CONFIG ---
DB_NAME = "measurements.db"
SERIAL_BAUDRATE = 9600
PORT = 8000

app = Flask(__name__)

# --- STATE ---
# Global state to hold latest data for fast API response
latest_data = {
    "temp": 0.0,
    "hum": 0.0,
    "pwm": 0,
    "mode": "Unknown",
    "sub": "",
    "target": 0,
    "timestamp": None,
    "connected": False
}

serial_connection = None
stop_event = threading.Event()

# --- DATABASE ---
def init_db():
    conn = sqlite3.connect(DB_NAME)
    c = conn.cursor()
    c.execute('''CREATE TABLE IF NOT EXISTS data
                 (id INTEGER PRIMARY KEY AUTOINCREMENT,
                  timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
                  temperature REAL,
                  humidity REAL,
                  pwm INTEGER,
                  mode TEXT)''')
    conn.commit()
    conn.close()

def log_data(temp, hum, pwm, mode):
    try:
        conn = sqlite3.connect(DB_NAME)
        c = conn.cursor()
        c.execute("INSERT INTO data (temperature, humidity, pwm, mode) VALUES (?, ?, ?, ?)",
                  (temp, hum, pwm, mode))
        conn.commit()
        conn.close()
    except Exception as e:
        print(f"DB Error: {e}")

# --- SERIAL WORKER ---
def serial_worker():
    global serial_connection, latest_data
    print("Background worker: Starting...")

    while not stop_event.is_set():
        if serial_connection is None or not serial_connection.is_open:
            # Try to reconnect
            ports = [p.device for p in serial.tools.list_ports.comports() if 'usbmodem' in p.device or 'usbserial' in p.device or 'COM' in p.device]
            if ports:
                try:
                    print(f"Connecting to {ports[0]}...")
                    serial_connection = serial.Serial(ports[0], SERIAL_BAUDRATE, timeout=1)
                    time.sleep(2) # Wait for reset
                    latest_data["connected"] = True
                    print("Connected!")
                except Exception as e:
                    print(f"Connection failed: {e}")
                    time.sleep(2)
            else:
                # print("No ports found, waiting...")
                latest_data["connected"] = False
                time.sleep(2)
            continue
        
        try:
            if serial_connection.in_waiting:
                line = serial_connection.readline().decode('utf-8').strip()
                if not line:
                    continue
                
                try:
                    data = json.loads(line)
                    # Update global state
                    latest_data.update(data)
                    latest_data["timestamp"] = datetime.now().isoformat()
                    latest_data["connected"] = True
                    
                    # Log to DB (only if values are valid)
                    if "temp" in data and "hum" in data:
                        log_data(data.get("temp", 0), data.get("hum", 0), data.get("pwm", 0), data.get("mode", "Unknown"))
                        
                except json.JSONDecodeError:
                    print(f"Invalid JSON: {line}")
        except Exception as e:
            print(f"Serial Error: {e}")
            serial_connection.close()
            serial_connection = None
            latest_data["connected"] = False
            
    if serial_connection:
        serial_connection.close()

# --- ROUTES ---

@app.route('/')
def serve_index():
    return send_from_directory('.', 'index.html')

@app.route('/api/live', methods=['GET'])
def get_live():
    return jsonify(latest_data)

@app.route('/api/history', methods=['GET'])
def get_history():
    # Get last 300 entries (approx 5 minutes if logged every second)
    conn = sqlite3.connect(DB_NAME)
    conn.row_factory = sqlite3.Row
    c = conn.cursor()
    c.execute("SELECT * FROM data ORDER BY id DESC LIMIT 300")
    rows = c.fetchall()
    conn.close()
    
    # Reverse to have oldest first for chart
    data = [dict(row) for row in reversed(rows)]
    return jsonify(data)

@app.route('/api/control', methods=['POST'])
def send_control():
    global serial_connection
    if serial_connection and serial_connection.is_open:
        cmd = request.json
        cmd_str = json.dumps(cmd) + "\n"
        try:
            serial_connection.write(cmd_str.encode('utf-8'))
            return jsonify({"status": "sent", "cmd": cmd})
        except Exception as e:
            return jsonify({"error": str(e)}), 500
    else:
        return jsonify({"error": "Not connected to Arduino"}), 503

if __name__ == '__main__':
    init_db()
    
    # Start Serial Thread
    t = threading.Thread(target=serial_worker, daemon=True)
    t.start()
    
    print(f"Server starting at http://localhost:{PORT}")
    try:
        app.run(port=PORT, debug=True, use_reloader=False) # Helper script, disable reloader to avoid double threads
    except KeyboardInterrupt:
        stop_event.set()

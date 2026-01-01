from flask import Flask, jsonify, render_template_string, request
from models import db, ClimateReading
from serial_manager import SerialManager
from flask_cors import CORS
import os

app = Flask(__name__, static_folder='../pi_frontend', static_url_path='')
CORS(app) # CORS für alle Domains aktivieren

app.config['SQLALCHEMY_DATABASE_URI'] = 'sqlite:///climate_data.db'
app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False

db.init_app(app)

# DB Tabellen erstellen
with app.app_context():
    db.create_all()

# Initialisiere Serial Manager
# Wir übergeben 'app', damit dieser Kontexte für den Datenbankzugriff erstellen kann
serial_mgr = SerialManager(app)
serial_mgr.start()

@app.route('/')
def index():
    return app.send_static_file('index.html')

@app.route('/api/live', methods=['GET'])
def get_live_data():
    """Gibt den aktuellen Live-Status der Serial-Schleife zurück."""
    return jsonify({
        'temperature': serial_mgr.current_temp,
        'humidity': serial_mgr.current_hum,
        'fan_speed': serial_mgr.current_fan_speed,
        'target_temperature': serial_mgr.target_temp,
        'target_humidity': serial_mgr.target_hum,
        'control_mode': serial_mgr.control_mode,
        'active_sensor_port': serial_mgr.sensor_port,
        'active_motor_port': serial_mgr.motor_port
    })

@app.route('/api/history', methods=['GET'])
def get_history():
    """Gibt die letzten N Datensätze zurück."""
    limit = request.args.get('limit', 100, type=int)
    readings = ClimateReading.query.order_by(ClimateReading.timestamp.desc()).limit(limit).all()
    
    # Umkehren für chronologische Reihenfolge in Diagrammen
    data = [r.to_dict() for r in readings][::-1]
    return jsonify(data)

@app.route('/api/settings', methods=['POST'])
def update_settings():
    """Aktualisiert die Einstellungen."""
    data = request.json
    if 'target_temp' in data:
        serial_mgr.target_temp = float(data['target_temp'])
    if 'target_hum' in data:
        serial_mgr.target_hum = float(data['target_hum'])
    if 'control_mode' in data:
        serial_mgr.control_mode = data['control_mode']
        
    return jsonify({
        "status": "success", 
        "target_temp": serial_mgr.target_temp,
        "target_hum": serial_mgr.target_hum,
        "control_mode": serial_mgr.control_mode
    })

if __name__ == '__main__':
    # Server starten
    # Hinweis: debug=True verträgt sich manchmal schlecht mit Serial-Threads, 
    # aber ist okay für die Entwicklung.
    app.run(host='0.0.0.0', port=5001, debug=False)

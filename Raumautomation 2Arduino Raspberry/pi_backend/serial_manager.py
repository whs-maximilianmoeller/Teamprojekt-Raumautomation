import threading
import time
import json
import serial
import serial.tools.list_ports
from simple_pid import PID
from models import db, ClimateReading
from flask import Flask

class SerialManager:
    def __init__(self, app: Flask):
        self.app = app
        self.running = True
        self.sensor_port = None
        self.motor_port = None
        self.lock = threading.Lock()
        
        # Aktueller Status
        self.current_temp = None
        self.current_hum = None
        self.current_fan_speed = 0
        
        # Einstellungen
        self.control_mode = "TEMP" # TEMP, HUM, AUTO
        self.target_temp = 22.0
        self.target_hum = 50.0
        
        # PID Regler
        # 1. Temperatur PID (Kühlen: Negative Regelparameter)
        self.pid_temp = PID(-10, -0.1, -0.05, setpoint=self.target_temp)
        self.pid_temp.output_limits = (0, 255) 
        
        # 2. Feuchtigkeits PID (Entfeuchten: Feuchte > Ziel -> Lüfter AN)
        # Ähnlich wie Kühlen: Eingang > Sollwert -> Fehler negativ -> Ausgang positiv?
        # Standard: Fehler = Soll - Ist. 
        # Wenn Ist(60) > Soll(50) -> Fehler(-10). Wir wollen Lüfter AN.
        # Also brauchen wir wieder ein negatives Kp.
        self.pid_hum = PID(-5, -0.05, -0.01, setpoint=self.target_hum)
        self.pid_hum.output_limits = (0, 255)

        self.thread = threading.Thread(target=self._worker, daemon=True)

    def start(self):
        self.thread.start()

    def _find_ports(self):
        """Scannt nach angeschlossenen Arduinos."""
        ports = list(serial.tools.list_ports.comports())
        found_sensor = None
        found_motor = None
        
        print(f"Scanning {len(ports)} ports...")
        for p in ports:
            if "ACM" in p.device or "USB" in p.device or "COM" in p.device:
                try:
                    s = serial.Serial(p.device, 115200, timeout=2)
                    time.sleep(2) 
                    
                    start = time.time()
                    is_sensor = False
                    is_motor = False
                    
                    while time.time() - start < 3:
                        if s.in_waiting:
                            line = s.readline().decode('utf-8', errors='ignore')
                            if "temp" in line and "hum" in line:
                                is_sensor = True
                                break
                            if "Motor" in line or "Ready" in line:
                                is_motor = True
                                break
                    
                    s.close()
                    
                    if is_sensor:
                        found_sensor = p.device
                        print(f"Found Sensor on {p.device}")
                    elif is_motor:
                        found_motor = p.device
                        print(f"Found Motor (candidate) on {p.device}")
                    
                except Exception as e:
                    print(f"Error scanning {p.device}: {e}")
        
        return found_sensor, found_motor

    def _worker(self):
        print("Serial Manager started.")
        
        sensor_ser = None
        motor_ser = None
        
        last_log_time = time.time()

        while self.running:
            # 1. Verbindungswiederherstellung (Recovery)
            if not sensor_ser or not motor_ser:
                s, m = self._find_ports()
                if s: self.sensor_port = s
                if m: self.motor_port = m
                
                if self.sensor_port and not sensor_ser:
                    try:
                        sensor_ser = serial.Serial(self.sensor_port, 115200, timeout=1)
                        print("Connected to Sensor")
                    except: sensor_ser = None
                
                if self.motor_port and not motor_ser:
                    try:
                        motor_ser = serial.Serial(self.motor_port, 115200, timeout=1)
                        print("Connected to Motor")
                    except: motor_ser = None
                
                if not sensor_ser or not motor_ser:
                    print("Waiting for devices...")
                    time.sleep(5)
                    continue

            # 2. Hauptschleife
            try:
                # --- SENSOR LESEN ---
                if sensor_ser.in_waiting:
                    line = sensor_ser.readline().decode('utf-8', errors='ignore').strip()
                    if line.startswith('{') and line.endswith('}'):
                        try:
                            data = json.loads(line)
                            with self.lock:
                                self.current_temp = data.get('temp')
                                self.current_hum = data.get('hum')
                            
                            # --- REGELUNGS-LOGIK ---
                            speed_temp = 0
                            speed_hum = 0
                            
                            if self.current_temp is not None:
                                # PID Sollwerte aktualisieren (falls geändert)
                                self.pid_temp.setpoint = self.target_temp
                                self.pid_hum.setpoint = self.target_hum
                                
                                # Anforderungen berechnen
                                speed_temp = int(self.pid_temp(self.current_temp))
                                speed_hum = int(self.pid_hum(self.current_hum))
                                
                                final_speed = 0
                                
                                if self.control_mode == "TEMP":
                                    final_speed = speed_temp
                                elif self.control_mode == "HUM":
                                    final_speed = speed_hum
                                elif self.control_mode == "AUTO":
                                    final_speed = max(speed_temp, speed_hum)
                                
                                self.current_fan_speed = final_speed
                                
                                # Sende an Motor
                                cmd = {"fan_speed": self.current_fan_speed}
                                msg = json.dumps(cmd) + "\n"
                                motor_ser.write(msg.encode('utf-8'))
                                
                        except json.JSONDecodeError:
                            pass
                
                # --- LOGGING (Alle 30s) ---
                if time.time() - last_log_time > 30:
                    last_log_time = time.time()
                    if self.current_temp is not None:
                        with self.app.app_context():
                            reading = ClimateReading(
                                temperature=self.current_temp,
                                humidity=self.current_hum,
                                fan_speed=self.current_fan_speed
                            )
                            db.session.add(reading)
                            db.session.commit()
                        
                time.sleep(0.1)
                
            except Exception as e:
                print(f"Serial Loop Error: {e}")
                try:
                    if sensor_ser: sensor_ser.close()
                    if motor_ser: motor_ser.close()
                except: pass
                sensor_ser = None
                motor_ser = None
                time.sleep(2)

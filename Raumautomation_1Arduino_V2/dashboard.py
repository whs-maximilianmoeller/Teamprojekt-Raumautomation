import streamlit as st
import pandas as pd
import serial
import serial.tools.list_ports
import time
import threading
import json
import collections
import random

# --- CONFIGURATION ---
st.set_page_config(
    page_title="Raumautomation Dashboard",
    page_icon="🌡️",
    layout="wide",
    initial_sidebar_state="expanded"
)

# Constants
MAX_HISTORY = 120 # Save last 120 seconds (approx) for charts
BAUD_RATE = 115200

# --- SERIAL HANDLER (SINGLETON) ---
class SerialHandler:
    def __init__(self):
        self.ser = None
        self.running = False
        self.thread = None
        self.lock = threading.Lock()
        
        # Data Storage
        self.data_history = collections.deque(maxlen=MAX_HISTORY)
        self.current_state = {
            "temp": 0.0,
            "hum": 0.0,
            "target_temp": 25.0,
            "target_hum": 50.0,
            "fan_speed": 0,
            "mode": "UNKNOWN",
            "last_update": None
        }
        
        # Simulation
        self.simulation_mode = False

    def connect(self, port):
        if port == "SIMULATION":
            self.simulation_mode = True
            self.running = True
            self.thread = threading.Thread(target=self._worker_simulation, daemon=True)
            self.thread.start()
            return True, "Simulation started"
            
        try:
            self.ser = serial.Serial(port, BAUD_RATE, timeout=1)
            self.simulation_mode = False
            self.running = True
            self.thread = threading.Thread(target=self._worker, daemon=True)
            self.thread.start()
            return True, f"Connected to {port}"
        except Exception as e:
            return False, str(e)

    def disconnect(self):
        self.running = False
        if self.thread:
            self.thread.join(timeout=1.0)
        
        if self.ser:
            self.ser.close()
            self.ser = None
        
        # Clear Data
        with self.lock:
            self.data_history.clear()

    def send_command(self, cmd_string):
        """Sends a raw string command to the Arduino."""
        if self.simulation_mode:
            print(f"[SIM] Parsing Command: {cmd_string}")
            # Simulate immediate state update for responsiveness
            if "MODE:" in cmd_string:
                mode = cmd_string.split(":")[1].strip()
                with self.lock: self.current_state["mode"] = mode
            if "SET_SPEED:" in cmd_string:
                try:
                    speed = int(cmd_string.split(":")[1].strip())
                    with self.lock: self.current_state["fan_speed"] = speed
                except: pass
            if "SET_TEMP:" in cmd_string:
                try:
                    val = float(cmd_string.split(":")[1].strip())
                    with self.lock: self.current_state["target_temp"] = val
                except: pass
            if "SET_HUM:" in cmd_string:
                try:
                    val = float(cmd_string.split(":")[1].strip())
                    with self.lock: self.current_state["target_hum"] = val
                except: pass
            return

        if self.ser and self.ser.is_open:
            try:
                full_cmd = cmd_string + "\n"
                self.ser.write(full_cmd.encode('utf-8'))
            except Exception as e:
                print(f"Send Error: {e}")

    def _worker(self):
        """Reads JSON lines from Serial."""
        while self.running and self.ser and self.ser.is_open:
            try:
                if self.ser.in_waiting:
                    line = self.ser.readline().decode('utf-8', errors='ignore').strip()
                    if line.startswith('{') and line.endswith('}'):
                        try:
                            data = json.loads(line)
                            self._update_state(data)
                        except json.JSONDecodeError:
                            pass
                time.sleep(0.01)
            except Exception:
                break

    def _worker_simulation(self):
        """Generates fake data with System Physics."""
        temp = 22.0
        hum = 50.0
        
        while self.running:
            with self.lock:
                # Get current settings
                target_temp = self.current_state.get("target_temp", 25.0)
                mode = self.current_state.get("mode", "AUTO")
                
                # --- 1. PHYSICS SIMULATION ---
                # Natural heating (room gets warmer)
                temp += random.uniform(0.05, 0.15)
                
                # Active Cooling (if fan is running)
                fan_cooling_factor = (self.current_state.get("fan_speed", 0) / 255.0) * 0.3
                temp -= fan_cooling_factor
                
                # Humidity random walk
                hum += random.uniform(-0.5, 0.5)
                
                # Boundaries
                temp = max(18.0, min(35.0, temp))
                hum = max(30.0, min(80.0, hum))
                
                # --- 2. CONTROLLER LOGIC (Mimic Arduino) ---
                if mode == "AUTO":
                    # P-Regler Logic from Arduino
                    # Range 10.0 (e.g. 25-35)
                    temp_range = 10.0
                    
                    if temp > target_temp:
                        delta = temp - target_temp
                        if delta > temp_range: delta = temp_range
                        
                        factor = delta / temp_range
                        # Map to [60...255]
                        speed = int(60 + (factor * (255 - 60)))
                        self.current_state["fan_speed"] = speed
                    else:
                        self.current_state["fan_speed"] = 0

                # Update State
                self.current_state["temp"] = round(temp, 1)
                self.current_state["hum"] = round(hum, 1)
                self.current_state["last_update"] = time.time()
                
                # Store History
                self.data_history.append({
                    "timestamp": pd.Timestamp.now(),
                    "temp": self.current_state["temp"],
                    "hum": self.current_state["hum"]
                })
            
            time.sleep(0.5)

    def _update_state(self, new_data):
        with self.lock:
            # Update Current State
            self.current_state.update(new_data)
            self.current_state["last_update"] = time.time()
            
            # Append to History
            self.data_history.append({
                "timestamp": pd.Timestamp.now(),
                "temp": self.current_state.get("temp", 0),
                "hum": self.current_state.get("hum", 0)
            })

    def get_data(self):
        with self.lock:
            return self.current_state.copy(), list(self.data_history)

# --- CACHING ---
@st.cache_resource
def get_serial_handler():
    return SerialHandler()

handler = get_serial_handler()

# --- UI LAYOUT ---

# Sidebar: Connection
with st.sidebar:
    st.header("🔌 Verbindung")
    
    ports = [p.device for p in serial.tools.list_ports.comports()]
    ports.insert(0, "SIMULATION")
    
    selected_port = st.selectbox("COM Port", ports)
    
    c1, c2 = st.columns(2)
    if c1.button("Verbinden", type="primary", use_container_width=True):
        if not handler.running:
            success, msg = handler.connect(selected_port)
            if success: st.success(msg)
            else: st.error(msg)
            
    if c2.button("Trennen", use_container_width=True):
        handler.disconnect()
        st.info("Getrennt")

    st.markdown("---")
    
    # Status Indicator
    if handler.running:
        st.success("● ONLINE")
        if handler.simulation_mode:
            st.warning("⚠️ Simulations-Modus")
    else:
        st.error("● OFFLINE")

# Main Area
st.title("Raumautomation Dashboard 🎛️")

@st.fragment(run_every=1)
def show_live_data():
    # Fetch latest data
    state, history = handler.get_data()

    # Metrics Row
    m1, m2, m3, m4 = st.columns(4)
    m1.metric("Temperatur", f"{state.get('temp', '--')} °C", border=True)
    m2.metric("Luftfeuchtigkeit", f"{state.get('hum', '--')} %", border=True)
    m3.metric("Lüfter", f"{state.get('fan_speed', 0)}", border=True)
    m4.metric("Modus", state.get('mode', '--'), border=True)

    # Visual Fan Speed
    fan_val = int(state.get('fan_speed', 0))
    fan_pct = min(1.0, max(0.0, fan_val / 255.0))
    st.caption(f"Lüfter-Leistung: {int(fan_pct * 100)}%")
    st.progress(fan_pct)

    # Charts
    if len(history) > 0:
        df = pd.DataFrame(history).set_index("timestamp")
        st.line_chart(df[["temp", "hum"]], height=350)
    else:
        st.info("Warte auf Daten...")

# Show the live data fragment
if handler.running:
    show_live_data()
else:
    st.info("Bitte verbinden Starten, um Daten zu sehen.")

# Controls (Outside fragment to avoid constant reset issues, though usually fine in fragment too if managed well.
# But keeping controls stable is better. However, they need state access.)
st.subheader("⚙️ Steuerung")

# We need to get state again for controls if they are outside fragment, 
# but state changes inside fragment might not propagate out instantly without rerun.
# Actually, st.fragment is best for the *output*. Controls should probably trigger updates.
# Let's keep controls simple.

state, _ = handler.get_data() # snapshot for controls

# Mode Toggle
is_auto = (state.get("mode") == "AUTO")
mode_toggle = st.toggle("Automatik-Modus", value=is_auto, key="mode_toggle")

if mode_toggle != is_auto:
    new_mode = "AUTO" if mode_toggle else "MANUAL"
    handler.send_command(f"MODE:{new_mode}")
    st.toast(f"Modus geändert auf: {new_mode}")
    # We don't need to force rerun, the fragment will pick it up on next tick or we can ensure responsiveness.

# Target Settings (For Auto Mode)
st.markdown("### 🎯 Zielwerte")
c1, c2 = st.columns(2)

with c1:
    st.markdown("**Soll-Temperatur (°C)**")
    # Sync Slider and Number Input via session state hacks or just simple logic
    # To keep it simple: Slider is master, but good UX needs both.
    
    # We use a key for the slider. If we want 2-way sync it's complex using st.fragment.
    # User asked for "Slider AND Number".
    
    current_target_temp = state.get("target_temp", 25.0)
    
    # Callback to send command immediately
    def update_temp():
        val = st.session_state.target_temp_slider
        handler.send_command(f"SET_TEMP:{val}")

    target_temp = st.slider(
        "Temperatur", 
        15.0, 35.0, 
        value=float(current_target_temp), 
        step=0.5,
        key="target_temp_slider",
        on_change=update_temp
    )
    # Just display number for now, full 2-way sync between 2 inputs is boilerplate heavy in Streamlit
    st.info(f"Aktuell: {target_temp} °C")


with c2:
    st.markdown("**Soll-Feuchtigkeit (%)**")
    current_target_hum = state.get("target_hum", 50.0)

    def update_hum():
        val = st.session_state.target_hum_slider
        handler.send_command(f"SET_HUM:{val}")

    target_hum = st.slider(
        "Feuchtigkeit", 
        30.0, 80.0, 
        value=float(current_target_hum), 
        step=1.0,
        key="target_hum_slider",
        on_change=update_hum
    )
    st.info(f"Aktuell: {target_hum} %")

st.divider()

# Fan Slider (Only active in Manual)
st.markdown("### 💨 Lüftergeschwindigkeit (Manuell)")
fan_speed = st.slider(
    "PWM Wert (0-255)", 
    0, 255, 
    value=int(state.get("fan_speed", 0)), 
    disabled=is_auto,
    key="fan_slider"
)

if not is_auto:
    if st.button("Geschwindigkeit Setzen", use_container_width=True):
        handler.send_command(f"SET_SPEED:{fan_speed}")
        st.toast(f"Geschwindigkeit gesetzt: {fan_speed}")

# Footer
st.markdown("---")
st.caption("Raumautomation Teamprojekt V2.0 | Streamlit + Arduino")

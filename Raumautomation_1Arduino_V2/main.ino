/*
 * Projekt: Raumautomation Single-Controller
 * Autor: Senior Embedded Systems Engineer (Agentic AI)
 * Plattform: Arduino Uno + L293D Shield v1 + DHT22
 *
 * Beschreibung:
 * Dieser Sketch implementiert eine nicht-blockierende Echtzeit-Steuerung für
 * einen Raumlüfter. Er verwendet ein kooperatives Scheduler-Pattern anstelle
 * von delay(), um Sensor-Akquise, Motor-Regelung und Serielle Telemetrie
 * quasi-parallel auszuführen.
 *
 * Architektur:
 * - Task 1: Sensor-Polling (2000ms Interval)
 * - Task 2: Telemetrie-Broadcast (500ms Interval)
 * - Task 3: Kommando-Parsing (Ereignisbasiert / Jeder Zyklus)
 */

#include <AFMotor.h>
#include <DHT.h>

// --- KONFIGURATION & UNVERÄNDERLICHE KONSTANTEN ---

// Pin-Definitionen (const für Compiler-Optimierung)
const int PIN_DHT = 2; // Daten-Pin für DHT22 Sensor

// Hardware-Objekte
// L293D Motor Port 1, 1KHz PWM Frequenz
AF_DCMotor motor(1, MOTOR12_1KHZ);
DHT dht(PIN_DHT, DHT22);

// Scheduler-Intervalle (ms)
const unsigned long INTERVAL_SENSOR = 2000;
const unsigned long INTERVAL_TELEMETRY = 500;

// Regelungsparameter (Auto-Modus)
float targetTemp = 25.0;       // Zieltemperatur (Startwert für Regelung)
float targetHum = 50.0;        // Zielfeuchtigkeit (Aktuell nur Anzeige/Dummy)
const float TEMP_RANGE = 10.0; // Proportionalbereich (z.B. 25-35 Grad)
const int SPEED_MIN = 60;      // Minimale PWM
const int SPEED_MAX = 255;     // Maximale PWM

// --- GLOBALE STATUS-VARIABLEN ---

// Systemzustand
enum ControlMode { MODE_MANUAL, MODE_AUTO };

ControlMode currentMode = MODE_AUTO; // Standard: Automatik

// Sensor-Werte
float currentTemp = 0.0;
float currentHum = 0.0;
bool sensorError = false;

// Aktuator-Zustand
int targetFanSpeed = 0; // Ziel-Geschwindigkeit (0-255)
int actualFanSpeed = 0; // Tatsächlich gesetzte Geschwindigkeit

// Scheduler-Timer
unsigned long lastSensorTime = 0;
unsigned long lastTelemetryTime = 0;

// --- FUNKTIONS-PROTOTYPEN ---
void runSensorTask();
void runControlTask();
void runTelemetryTask();
void processSerialCommands();
void setMotorSpeed(int speed);

void setup() {
  // 1. Initialisierung der Seriellen Schnittstelle (Schnell für JSON)
  Serial.begin(115200);
  while (!Serial) {
    ;
  } // Warten auf Leonardo/Micro (optional bei Uno)

  // 2. Hardware-Init
  dht.begin();

  motor.setSpeed(0);
  motor.run(RELEASE);

  Serial.println(
      F("{\"status\": \"BOOT_COMPLETE\", \"msg\": \"Raumautomation Ready\"}"));
}

void loop() {
  unsigned long currentMillis = millis();

  // --- TASK 1: SENSOR ACQUISITION (2Hz / 2000ms) ---
  // DHT22 ist langsam, nicht öfter als alle 2 Sek abfragen
  if (currentMillis - lastSensorTime >= INTERVAL_SENSOR) {
    lastSensorTime = currentMillis;
    runSensorTask();
  }

  // --- TASK 2: CONTROL LOGIC (Jeder Zyklus / Event Driven nach Sensor) ---
  // Die Regelung wird kontinuierlich geprüft, reagiert aber auf Änderungen
  runControlTask();

  // --- TASK 3: TELEMETRY (2Hz / 500ms) ---
  // Sendet aktuellen Status an PC/Pi
  if (currentMillis - lastTelemetryTime >= INTERVAL_TELEMETRY) {
    lastTelemetryTime = currentMillis;
    runTelemetryTask();
  }

  // --- TASK 4: COMMAND HANDLING (Jeder Zyklus) ---
  // Prüft auf Eingaben vom Nutzer
  processSerialCommands();
}

/**
 * Task 1: Liest Temperatur und Feuchtigkeit vom DHT22.
 * Behandelt NaN-Fehler robust.
 */
void runSensorTask() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  // Fehlerprüfung: Wenn Sensor NaN (Not a Number) liefert
  if (isnan(t) || isnan(h)) {
    sensorError = true;
    // Im Fehlerfall behalten wir die alten Werte oder setzen Flags (hier: Error
    // Flag) Debug-Ausgabe nur sparsam, um JSON parsing nicht zu brechen
  } else {
    sensorError = false;
    currentTemp = t;
    currentHum = h;
  }
}

/**
 * Task 2: Führt die Regelungslogik aus.
 * Entscheidet basierend auf Modus (AUTO/MANUAL) über die Lüfterdrehzahl.
 */
void runControlTask() {
  int newSpeed = 0;

  if (currentMode == MODE_AUTO) {
    // --- AUTOMATIK-LOGIK (P-Regler) ---
    // --- AUTOMATIK-LOGIK (P-Regler) ---
    if (!sensorError && currentTemp > targetTemp) {
      // Berechne proportionalen Anteil
      // Mapping: [targetTemp ... targetTemp+Range] -> [MinSpeed ... MaxSpeed]

      float tempDelta = currentTemp - targetTemp;
      float tempRange = TEMP_RANGE;

      // Begrenzung (Clamping)
      if (tempDelta > tempRange)
        tempDelta = tempRange;

      float factor = tempDelta / tempRange; // 0.0 bis 1.0

      // Lineare Interpolation
      newSpeed = SPEED_MIN + (int)(factor * (SPEED_MAX - SPEED_MIN));

    } else {
      // Unter Schwellwert oder Sensorfehler -> Lüfter aus (oder Sicherer
      // Zustand)
      newSpeed = 0;
    }
  } else {
    // --- MANUAL-LOGIK ---
    // Geschwindigkeit wird direkt durch processSerialCommands gesetzt
    newSpeed = targetFanSpeed;
  }

  // Hardware nur updaten, wenn sich was ändert (Effizienz)
  if (newSpeed != actualFanSpeed) {
    setMotorSpeed(newSpeed);
  }
}

/**
 * Task 3: Sendet Systemzustand als JSON.
 * Format: {"temp": 24.5, "hum": 60.2, "fan_speed": 150, "mode": "AUTO"}
 */
void runTelemetryTask() {
  Serial.print(F("{"));

  Serial.print(F("\"temp\": "));
  if (sensorError) Serial.print(F"null"));
  else
    Serial.print(currentTemp, 1);

  Serial.print(F(", \"hum\": "));
  if (sensorError) Serial.print(F"null"));
  else
    Serial.print(currentHum, 1);

  Serial.print(F(", \"fan_speed\": "));
  Serial.print(actualFanSpeed);

  Serial.print(F(", \"mode\": \""));
  Serial.print(currentMode == MODE_AUTO ? "AUTO" : "MANUAL");
  Serial.print(F("\""));

  // Optional: Target Werte mitsenden für Sync
  Serial.print(F(", \"target_temp\": "));
  Serial.print(targetTemp, 1);
  Serial.print(F(", \"target_hum\": "));
  Serial.print(targetHum, 1);

  Serial.println(F("}"));
}

/**
 * Task 4: Verarbeitet Serielle Befehle.
 * Erwartet Format: "KEY:VALUE" (z.B. "SET_SPEED:200", "MODE:AUTO")
 * Nicht-blockierendes Einlesen.
 */
void processSerialCommands() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    // Einfacher Parser (Split by ':')
    int separatorIndex = command.indexOf(':');

    if (separatorIndex != -1) {
      String key = command.substring(0, separatorIndex);
      String value = command.substring(separatorIndex + 1);

      key.toUpperCase(); // Case-Insensitive keys

      if (key == "MODE") {
        value.toUpperCase();
        if (value == "AUTO") {
          currentMode = MODE_AUTO;
        } else if (value == "MANUAL") {
          currentMode = MODE_MANUAL;
        }
      } else if (key == "SET_SPEED") {
        int speedVal = value.toInt();
        // Sicherheit: Clamp 0-255
        if (speedVal < 0)
          speedVal = 0;
        if (speedVal > 255)
          speedVal = 255;

        targetFanSpeed = speedVal;

        // Wenn wir im Manual Mode sind, wird das im nächsten Control-Loop
        // angewendet. Falls Nutzer Speed setzt, schalten wir meist intuitiv auf
        // Manual? Anforderung sagt: "Im 'MANUAL'-Modus: Lüftergeschwindigkeit
        // wird ... gesetzt". Wir schalten hier NICHT automatisch um, Nutzer
        // muss explizit MODE:MANUAL senden oder wir interpretieren SET_SPEED
        // als impliziten Wechsel. Hier: Explizit bleiben (User muss MODE:MANUAL
        // senden).
      } else if (key == "SET_TEMP") {
        targetTemp = value.toFloat();
      } else if (key == "SET_HUM") {
        targetHum = value.toFloat();
      }
    }
  }
}
}

/**
 * Low-Level Motor Treiber Abstraktion
 */
void setMotorSpeed(int speed) {
  actualFanSpeed = speed;

  if (speed <= 0) {
    motor.run(RELEASE); // Motor komplett freigeben
  } else {
    motor.run(FORWARD); // Oder BACKWARD, je nach Verkabelung
    motor.setSpeed(speed);
  }
}

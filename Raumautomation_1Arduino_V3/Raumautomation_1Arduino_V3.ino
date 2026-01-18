/**
 * Raumautomation_1Arduino_V3
 *
 * Lüftersteuerung basierend auf AM2315 Temperatur/Feuchtigkeitssensor.
 * Kommunikation über Serial mittels JSON.
 *
 * Hardware:
 * - Arduino Uno
 * - AM2315 Sensor an I2C (SDA, SCL)
 * - 12V PWM Lüfter an Pin 9
 *
 * Bibliotheken:
 * - Adafruit AM2315
 * - ArduinoJson (Version 6+)
 */

#include <Adafruit_AM2315.h>
#include <ArduinoJson.h>
#include <Wire.h>

// --- Konfiguration ---
const int FAN_PIN = 9;
const long INTERVAL = 2000; // 2 Sekunden Intervall

// --- Globale Variablen ---
Adafruit_AM2315 am2315;
unsigned long previousMillis = 0;

// Status Variablen
String currentMode = "Auto"; // "Auto" oder "Manual"
String manualSubMode =
    "PWM"; // "PWM", "TEMP", "HUM" (nur relevant wenn Mode "Manual")

int currentPwm = 0; // Aktueller PWM Output (0-255)
float temp = 0.0;
float hum = 0.0;

// Zielwerte für Regelung
float targetTemp = 25.0; // Default Ziel-Temperatur
float targetHum = 60.0;  // Default Ziel-Feuchtigkeit
int manualTargetPwm = 0; // Manuell gesetzter PWM Wert

// JSON Buffer
StaticJsonDocument<300> docOut;
StaticJsonDocument<300> docIn;

void setup() {
  Serial.begin(9600);
  pinMode(FAN_PIN, OUTPUT);
  analogWrite(FAN_PIN, 0);

  delay(1000);
  if (!am2315.begin()) {
    Serial.println("{\"error\": \"Sensor AM2315 nicht gefunden!\"}");
  }
}

void loop() {
  unsigned long currentMillis = millis();

  // 1. Befehle verarbeiten
  checkSerialInput();

  // 2. Regelung & Messung
  if (currentMillis - previousMillis >= INTERVAL) {
    previousMillis = currentMillis;

    // Sensor lesen
    if (am2315.readTemperatureAndHumidity(temp, hum)) {
      controlFan();
    }

    // Output setzen
    analogWrite(FAN_PIN, currentPwm);

    // Status senden
    sendStatus();
  }
}

/**
 * Kern-Logik für die Lüftersteuerung
 */
void controlFan() {
  if (currentMode == "Auto") {
    // Legacy Auto Mode: 25°C -> 0%, 30°C -> 100%
    if (temp < 25.0)
      currentPwm = 0;
    else if (temp >= 30.0)
      currentPwm = 255;
    else {
      float ratio = (temp - 25.0) / (30.0 - 25.0);
      currentPwm = (int)(ratio * 255);
    }
  } else if (currentMode == "Manual") {
    if (manualSubMode == "PWM") {
      // Direkte Steuerung
      currentPwm = manualTargetPwm;
    } else if (manualSubMode == "TEMP") {
      // Einfache P-Regelung: Je wärmer über Ziel, desto schneller
      // Beispiel: Ziel 24°C.
      // Ist 24°C -> 0 PWM
      // Ist 29°C -> 255 PWM (Delta 5 K)
      float diff = temp - targetTemp;
      if (diff <= 0)
        currentPwm = 0;
      else if (diff >= 5.0)
        currentPwm = 255;
      else
        currentPwm = (int)((diff / 5.0) * 255);
    } else if (manualSubMode == "HUM") {
      // Feuchtigkeitsregelung
      // Ziel 60%. Ab 60% anfangen zu drehen.
      // Bei 80% (Delta 20%) volle Power.
      float diff = hum - targetHum;
      if (diff <= 0)
        currentPwm = 0;
      else if (diff >= 20.0)
        currentPwm = 255;
      else
        currentPwm = (int)((diff / 20.0) * 255);
    }
  }

  // Safety Clamp
  currentPwm = constrain(currentPwm, 0, 255);
}

/**
 * Parsed eingehende JSON Befehle
 * Patterns:
 * {"mode":"auto"}
 * {"mode":"manual", "sub":"pwm", "val": 100}
 * {"mode":"manual", "sub":"temp", "val": 24.5}
 * {"mode":"manual", "sub":"hum", "val": 65}
 */
void checkSerialInput() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() == 0)
      return;

    DeserializationError error = deserializeJson(docIn, input);
    if (!error) {
      if (docIn.containsKey("mode")) {
        String m = docIn["mode"];
        if (m == "auto") {
          currentMode = "Auto";
        } else if (m == "manual") {
          currentMode = "Manual";
          if (docIn.containsKey("sub") && docIn.containsKey("val")) {
            manualSubMode = docIn["sub"].as<String>();
            manualSubMode.toUpperCase(); // PWM, TEMP, HUM

            float val = docIn["val"];
            if (manualSubMode == "PWM")
              manualTargetPwm = (int)val;
            else if (manualSubMode == "TEMP")
              targetTemp = val;
            else if (manualSubMode == "HUM")
              targetHum = val;

            // Sofort update triggern für snappy feeling (optional)
            controlFan();
            analogWrite(FAN_PIN, currentPwm);
          }
        }
      }
    }
  }
}

/**
 * Sendet Status
 */
void sendStatus() {
  docOut.clear();
  docOut["temp"] = temp;
  docOut["hum"] = hum;
  docOut["pwm"] = currentPwm;
  docOut["mode"] = currentMode;

  if (currentMode == "Manual") {
    docOut["sub"] = manualSubMode;
    // Aktuellen Zielwert mitsenden
    if (manualSubMode == "PWM")
      docOut["target"] = manualTargetPwm;
    else if (manualSubMode == "TEMP")
      docOut["target"] = targetTemp;
    else if (manualSubMode == "HUM")
      docOut["target"] = targetHum;
  }

  serializeJson(docOut, Serial);
  Serial.println();
}

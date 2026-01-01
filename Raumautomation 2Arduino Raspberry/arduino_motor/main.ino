/*
 * Arduino Motor Steuerung für L293D
 * ----------------------------------
 * Dieser Sketch steuert einen DC-Motor über einen L293D Treiber basierend auf Befehlen,
 * die über den Seriellen Port (USB) empfangen werden.
 * 
 * Hardware Verbindungen:
 * - L293D Input 1  -> Arduino Pin 8
 * - L293D Input 2  -> Arduino Pin 7
 * - L293D Enable 1 -> Arduino Pin 9 (PWM)
 * 
 * Kommunikation:
 * - Baudrate: 115200
 * - Protokoll: Einfacher JSON-artiger String
 * - Befehlsformat: {"fan_speed": <0-255>}
 *   Beispiel: {"fan_speed": 128}  -> 50% Geschwindigkeit
 *             {"fan_speed": 255}  -> 100% Geschwindigkeit
 *             {"fan_speed": 0}    -> Stopp
 * 
 * Sicherheitsfunktion (Watchdog):
 * - Wenn für 5 Sekunden kein gültiger Befehl empfangen wird, stoppt der Motor automatisch.
 */

// Pin Definitionen
const int PIN_INPUT_1 = 8;  // Richtungs-Pin 1
const int PIN_INPUT_2 = 7;  // Richtungs-Pin 2
const int PIN_ENABLE  = 9;  // PWM Geschwindigkeits-Pin

// Globale Variablen
unsigned long lastCommandTime = 0; // Zeitstempel des letzten gültigen Befehls
const long WATCHDOG_TIMEOUT = 5000; // 5 Sekunden Timeout in Millisekunden
int currentSpeed = 0;              // Aktuelle Motorgeschwindigkeit (0-255)

void setup() {
  // Serial-Kommunikation initialisieren
  Serial.begin(115200);
  
  // Pins konfigurieren
  pinMode(PIN_INPUT_1, OUTPUT);
  pinMode(PIN_INPUT_2, OUTPUT);
  pinMode(PIN_ENABLE, OUTPUT);

  // Standard-Richtung: Vorwärts
  // Zum Umkehren hier HIGH und LOW tauschen
  digitalWrite(PIN_INPUT_1, HIGH);
  digitalWrite(PIN_INPUT_2, LOW);

  // Starte mit gestopptem Motor
  analogWrite(PIN_ENABLE, 0);
  
  // Kurz warten, damit sich Serial stabilisiert
  delay(100);
  Serial.println("Arduino Motor Controller Ready");
  Serial.println("Send commands like: {\"fan_speed\": 150}");
}

void loop() {
  // 1. Auf eingehende Serial-Daten prüfen
  if (Serial.available() > 0) {
    String inputString = Serial.readStringUntil('\n'); // Ganze Zeile lesen
    inputString.trim(); // Leerzeichen/Newlines entfernen

    // Einfache Parsing-Logik strikt für {"fan_speed": WERT}
    // Wir suchen nach dem Schlüssel "fan_speed", um es ohne schwere JSON-Libraries einfach zu halten
    int keyIndex = inputString.indexOf("\"fan_speed\"");
    
    if (keyIndex != -1) {
      // Finde den Doppelpunkt nach dem Schlüssel
      int colonIndex = inputString.indexOf(':', keyIndex);
      
      if (colonIndex != -1) {
        // Extrahiere den Zahlenteil: alles nach ':' und vor '}'
        int braceIndex = inputString.indexOf('}', colonIndex);
        
        // Falls keine schließende Klammer, nimm den Rest des Strings
        if (braceIndex == -1) {
          braceIndex = inputString.length();
        }

        String valueString = inputString.substring(colonIndex + 1, braceIndex);
        int newSpeed = valueString.toInt(); // String in Integer konvertieren

        // Geschwindigkeit auf gültigen PWM-Bereich 0-255 begrenzen
        newSpeed = constrain(newSpeed, 0, 255);

        // Geschwindigkeit anwenden
        currentSpeed = newSpeed;
        analogWrite(PIN_ENABLE, currentSpeed);

        // Watchdog Timer aktualisieren
        lastCommandTime = millis();

        // Debug Ausgabe (optional, kann entfernt werden wenn es den Pi stört)
        // Serial.print("Set Speed to: ");
        // Serial.println(currentSpeed);
      }
    }
  }

  // 2. Watchdog Check
  // Wenn die aktuelle Zeit minus letzte Befehlszeit größer als Timeout ist...
  if (millis() - lastCommandTime > WATCHDOG_TIMEOUT) {
    if (currentSpeed > 0) {
      // Not-Aus (Emergency Stop)
      currentSpeed = 0;
      analogWrite(PIN_ENABLE, 0);
      Serial.println("Watchdog: Motor stopped due to inactivity.");
    }
    // Timer zurücksetzen, um das Serial-Log nicht zu fluten
    lastCommandTime = millis();
  }
}

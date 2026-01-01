/*
 * Arduino Sensor Knoten (DHT22 + LCD)
 * -----------------------------------
 * Liest Temperatur und Luftfeuchtigkeit von einem DHT22 Sensor und zeigt den Status auf einem 20x4 LCD an.
 * Sendet Daten via Serial an einen Raspberry Pi und empfängt Display-Nachrichten.
 * 
 * Hardware Verbindungen:
 * - LCD (4-bit Modus):
 *   - RS -> Pin 12
 *   - E  -> Pin 11
 *   - D4 -> Pin 5
 *   - D5 -> Pin 4
 *   - D6 -> Pin 3
 *   - D7 -> Pin 2
 *   - VSS-> GND, VDD-> 5V, V0-> Potentiometer (Kontrast), RW-> GND, A-> 5V, K-> GND
 * 
 * - DHT22 Sensor:
 *   - Data -> Pin 6  ( GEÄNDERT von Pin 2 wegen Konflikt mit LCD D7 )
 *   - VCC  -> 5V
 *   - GND  -> GND
 * 
 * Kommunikation:
 * - Baudrate: 115200
 * - TX: {"temp": 21.5, "hum": 45.2}
 * - RX: {"msg": "Regelung Aktiv"} -> Zeigt Nachricht auf LCD Zeile 4
 */

#include <LiquidCrystal.h>
#include <DHT.h>

// --- Pin Definitionen ---
// LCD Pins: RS, E, D4, D5, D6, D7
const int PIN_LCD_RS = 12;
const int PIN_LCD_E  = 11;
const int PIN_LCD_D4 = 5;
const int PIN_LCD_D5 = 4;
const int PIN_LCD_D6 = 3;
const int PIN_LCD_D7 = 2;

// DHT Sensor
const int PIN_DHT = 6;     // Verbunden mit Pin 6 um Konflikt mit LCD zu vermeiden
#define DHTTYPE DHT22

// --- Objekte ---
LiquidCrystal lcd(PIN_LCD_RS, PIN_LCD_E, PIN_LCD_D4, PIN_LCD_D5, PIN_LCD_D6, PIN_LCD_D7);
DHT dht(PIN_DHT, DHTTYPE);

// --- Globale Variablen ---
unsigned long lastReadTime = 0;
const long READ_INTERVAL = 2000; // Lese Sensor alle 2 Sekunden

void setup() {
  // Initialize Serial
  Serial.begin(115200);

  // Initialize LCD
  lcd.begin(20, 4);
  lcd.setCursor(0, 0);
  lcd.print("Raumautomation");
  lcd.setCursor(0, 1);
  lcd.print("Init Sensor...");

  // Initialize DHT
  dht.begin();
  
  delay(1000);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Temp: --.- C");
  lcd.setCursor(0, 1);
  lcd.print("Hum:  --.- %");
}

void loop() {
  unsigned long currentMillis = millis();

  // 1. Lese Sensor periodisch
  if (currentMillis - lastReadTime >= READ_INTERVAL) {
    lastReadTime = currentMillis;

    float h = dht.readHumidity();
    float t = dht.readTemperature();

    // Prüfe ob Lesen fehlgeschlagen ist
    if (isnan(h) || isnan(t)) {
      Serial.println(F("{\"error\": \"Failed to read from DHT sensor!\"}"));
      lcd.setCursor(6, 0); lcd.print("Err ");
      lcd.setCursor(6, 1); lcd.print("Err ");
    } else {
      // Send JSON to Serial
      Serial.print("{\"temp\": ");
      Serial.print(t, 1);
      Serial.print(", \"hum\": ");
      Serial.print(h, 1);
      Serial.println("}");

      // Aktualisiere Lokales Display (Reihen 0 und 1)
      lcd.setCursor(6, 0); 
      lcd.print(t, 1); lcd.print(" C");
      lcd.setCursor(6, 1);
      lcd.print(h, 1); lcd.print(" %");
    }
  }

  // 2. Auf Serial Befehle horchen
  if (Serial.available() > 0) {
    String inputString = Serial.readStringUntil('\n');
    inputString.trim();

    // Prüfe auf {"msg": "..."}
    // Einfaches eigenes Parsing
    int keyIndex = inputString.indexOf("\"msg\"");
    if (keyIndex != -1) {
      int colonIndex = inputString.indexOf(':', keyIndex);
      if (colonIndex != -1) {
        // Find start quote of value
        int startQuote = inputString.indexOf('"', colonIndex);
        if (startQuote != -1) {
          int endQuote = inputString.indexOf('"', startQuote + 1);
          if (endQuote != -1) {
            String msg = inputString.substring(startQuote + 1, endQuote);
            
            // Zeige auf Zeile 4 (Reihe 3 0-indiziert)
            // Erst Zeile leeren
            lcd.setCursor(0, 3);
            lcd.print("                    "); 
            lcd.setCursor(0, 3);
            lcd.print(msg);
          }
        }
      }
    }
  }
}

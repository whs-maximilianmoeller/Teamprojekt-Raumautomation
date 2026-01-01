# Projekt Raumautomation - Tisch-Demonstrator

**Ziel:**
Aufbau eines Demonstrators zur Raumautomation mit verteilten Mikrocontrollern und zentraler Steuerung.

**Struktur:**
- `/arduino_sensor`: Arduino A Code (Sensor Node) - DHT22, LCD.
- `/arduino_motor`: Arduino B Code (Aktor Node) - Lüfter, Motor Driver.
- `/pi_backend`: Python Flask Server (Master logic, PID, Serial communication).
- `/pi_frontend`: Web Dashboard für Visualisierung und Kontrolle.

**Hardware:**
- 2x Arduino Uno
- 1x Raspberry Pi
- DHT22, L293D, 12V Lüfter, 20x4 LCD.

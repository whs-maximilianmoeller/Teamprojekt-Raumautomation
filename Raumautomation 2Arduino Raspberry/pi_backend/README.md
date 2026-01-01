# Raspberry Pi Backend

**Funktion:**
Zentrale Steuereinheit und Datensammler.

**Technologie:**
- Python (Flask)
- Pyserial (Kommunikation mit Arduinos)

**Aufgaben:**
- Kommuniziert über USB (Serial) mit beiden Arduinos.
- Implementiert die PID-Regelung für den Lüfter (basierend auf Temperatur).
- Stellt eine API bereit, um Daten an das Frontend zu liefern und Steuerbefehle zu empfangen.

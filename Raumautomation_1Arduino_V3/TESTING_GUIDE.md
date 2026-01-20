# Anleitung zum Testen mit Arduino & Datenbank

Wir haben das System erweitert, damit Daten in einer Datenbank gespeichert werden und der Graph auch nach dem Neuladen erhalten bleibt.

## 1. Voraussetzungen

Du benötigst Python und zwei Bibliotheken.
Öffne ein Terminal im Ordner `Raumautomation_1Arduino_V3` und führe aus:

```bash
pip3 install -r requirements.txt
```
*(Falls `pip3` nicht gefunden wird, versuche `python3 -m pip install -r requirements.txt`)*

## 2. Arduino vorbereiten
1.  Arduino mit `Raumautomation_1Arduino_V3.ino` flashen (falls noch nicht passiert).
2.  Arduino **nicht** im Serial Monitor der Arduino IDE offen haben (das blockiert den Port). Schließe die Arduino IDE am besten.

## 3. Server starten
Die Kommunikation läuft jetzt über ein Python-Skript, das im Hintergrund die Daten speichert.

1.  Terminal im Ordner öffnen.
2.  Starten:
    ```bash
    python3 server.py
    ```
3.  Das Skript sollte anzeigen:
    *   `Connecting to ...`
    *   `Connected!` (sobald der Arduino erkannt wird)
    *   `Server starting at http://localhost:8000`

## 4. Web-Interface nutzen
1.  Öffne deinen Browser (Chrome, Safari, Firefox - alle gehen jetzt!).
2.  Gehe auf: [http://localhost:8000](http://localhost:8000)
3.  **Verbinden**: Du musst nicht mehr manuell verbinden. Das erledigt der Python-Server im Hintergrund.
    *   Steht dort **"VERBUNDEN"**, ist alles bereit.
    *   Steht dort **"KEINE VERBINDUNG"**, prüfe, ob der Arduino eingesteckt ist und das Python-Skript läuft.

## Features
*   **Live Daten**: Aktualisieren sich jede Sekunde.
*   **Graph**: Zeigt die letzten 60 Sekunden an. Die Daten kommen aus der Datenbank (`measurements.db`), daher sind sie auch nach dem Neuladen der Seite noch da!
*   **Steuerung**: Regler und Modus-Schalter funktionieren wie gewohnt.

## Troubleshooting
*   **Fehler `Address already in use`**: Läuft vielleicht noch das alte `start_simulation.py` oder ein anderer Server? Beende diese.
*   **Arduino nicht gefunden**: Prüfe das USB-Kabel.

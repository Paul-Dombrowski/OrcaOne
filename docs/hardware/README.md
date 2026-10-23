# Hardware & Sensorik (Aufgabe 2 — 20 Punkte)

Dokumentation des physischen Aufbaus: welche Bauteile, wie verkabelt.

| Datei | Inhalt |
|---|---|
| `bauteile.md` | Liste aller Bauteile mit kurzer Funktion |
| `pinbelegung.md` | Vollständige Pin-Zuordnung ESP32 ↔ Module |

## Wichtig

- Die Pinbelegung in `pinbelegung.md` muss **identisch** mit
  [`../../firmware/src/config.h`](../../firmware/src/config.h) sein. Bei Änderung
  **beide** aktualisieren.
- Auf **gemeinsame Masse (GND)** aller Module achten.
- Spannung beachten: ESP32-Logik ist **3,3 V**. Alle hier verwendeten Module
  laufen mit 3,3 V — **nicht** an 5 V-Logik anschließen.
- Der NRF24L01+ ist empfindlich bei der Stromversorgung; ein kleiner
  Stützkondensator (z. B. 10 µF) zwischen seinem VCC und GND verbessert die
  Stabilität.

## To-do

- [ ] Verkabelung sauber aufbauen (Steckbrett/Platine) und fotografieren (für Doku).
- [ ] Jedes Modul einzeln testen, bevor alles zusammenkommt.

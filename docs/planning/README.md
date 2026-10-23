# Planung & Konzeption (Aufgabe 1 — 10 Punkte)

Hier liegt die inhaltliche Projektplanung. Diese Dokumente sind die Grundlage
für Umsetzung und Doku und beantworten: *Was bauen wir, wozu, und wie
kommunizieren die Teile?*

## Inhalt

| Datei | Inhalt |
|---|---|
| `concept.jpeg` | Ursprüngliche Ideenskizze (Mindmap) der Gruppe |
| `io-uebersicht.md` | Ein-/Ausgangsübersicht: alle Sensoren & Aktoren, automatisch/manuell |
| `blockschaltbild.md` | Kommunikationswege zwischen Handy, ESP32 und Peripherie |

## Projektidee in einem Satz

**OrcaOne** ist ein tragbares ESP32-Multitool (NFC/RFID, Infrarot, 2,4 GHz), das
lokal per Drehgeber-Menü und OLED bedient und zusätzlich über eine Weboberfläche
am Smartphone ferngesteuert wird.

## Nutzen / Besucherinteraktion (für Aufgabe 5)

Besucher können am eigenen Stand gefahrlos ausprobieren, wie ein IoT-Gerät Daten
erfasst und über WLAN mit dem Handy austauscht — z. B. einen (eigenen) RFID-Tag
einlesen und den Wert live am Handy erscheinen sehen, oder ein IR-Signal
aufnehmen und abspielen. Zugang zur Weboberfläche per **QR-Code** (Aufgabe 5).

## Abdeckung der Mindestanforderungen

| Anforderung | Umsetzung in OrcaOne |
|---|---|
| ESP32 als Steuerung + WLAN | `firmware/src/network/` (Access-Point) |
| ≥ 2 Sensoren | NFC-Reader, IR-Empfänger, Drehgeber |
| ≥ 2 Aktoren | IR-LED (Sender), OLED-Display, NFC-Writer, 2,4-GHz-Modul |
| Smartphone zeigt Werte/Zustände | Weboberfläche `web/` (Live über WebSocket) |
| ≥ 1 Aktor vom Handy steuerbar | z. B. IR senden per Tastendruck am Handy |
| automatische Reaktion auf Sensorwert | ModeManager-Regel (siehe `io-uebersicht.md`) |
| Werte aktualisieren ohne Neuladen | WebSocket-Status-Push alle ~500 ms |
| Automatik an/aus per Handy | Befehl `system/setMode` |
| läuft ohne Internet | ESP32 spannt eigenes WLAN auf |

> Details zur Automatik-Regel mit der Gruppe final festlegen.

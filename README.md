# OrcaOne

**IoT-Multitool auf ESP32-Basis** — Schulprojekt TGI 13, „Nacht der Technik".
Ein tragbares Gerät im Stil eines Flipper Zero: lokal bedienbar über Drehgeber +
OLED-Menü und zusätzlich fernsteuerbar über eine Weboberfläche am Smartphone
(WLAN, ohne Internetzugang).

> Abgabe & Präsentation: **11.11.2026**

---

## Was das Gerät kann

| Bereich | Funktion |
|---|---|
| **NFC/RFID** (PN532) | Tags lesen, schreiben, klonen |
| **Infrarot** (KY-022 Empfänger / KY-005 Sender) | IR-Signale aufnehmen und erneut senden (Replay) |
| **2,4 GHz** (NRF24L01+) | Störsender-Modul (rechtlich abgestimmt, nur im freigegebenen Rahmen) |
| **Bedienung** | Drehgeber + 1,3" OLED-Menü lokal |
| **Fernsteuerung** | Weboberfläche am Handy über WLAN-Hotspot des ESP32 |

Details zu den Pflicht-Anforderungen des Projektauftrags (2 Sensoren, 2 Aktoren,
Automatik, bidirektionale Kommunikation …) stehen in
[`docs/planning/`](docs/planning/).

---

## Für Teammitglieder: hier anfangen

1. **[`docs/ARBEITSWEISE.md`](docs/ARBEITSWEISE.md)** — der verbindliche Standard,
   nach dem **alle** (Menschen wie KIs) gleich arbeiten. **Zuerst lesen.**
2. **[`docs/STRUCTURE.md`](docs/STRUCTURE.md)** — erklärt den gesamten Aufbau des
   Projekts und **wo was liegt**.
2. Deinen Arbeitsbereich finden (jede Person betreut eigene Module/Ordner):
   - Firmware-Module: [`firmware/src/modules/`](firmware/src/modules/)
   - Weboberfläche: [`firmware/web/`](firmware/web/)
   - Doku/Planung: [`docs/`](docs/)
3. In **jedem** Ordner liegt eine `README.md`, die genau beschreibt, was dort
   hineingehört und wie es mit dem Rest zusammenspielt. Diese Beschreibungen sind
   auch so geschrieben, dass eine KI damit gezielt arbeiten kann — gib der KI
   immer die `README.md` des jeweiligen Ordners als Kontext mit.

### Arbeitsweise mit git (3 Personen)

- **Nie direkt auf `main` committen.** Jede/r arbeitet auf einem eigenen Branch
  (z. B. `feature/nfc-modul`) und stellt einen Pull Request.
- Weil jedes Modul in einem eigenen Ordner liegt, arbeitet ihr an getrennten
  Dateien → kaum Merge-Konflikte.
- Vor dem Start immer `git pull`, nach fertigen Teilschritten `git commit`.

---

## Technischer Stack

- **ESP32** (DevKit) als Steuereinheit und WLAN-Access-Point
- **PlatformIO** + Arduino-Framework (Build-Config: [`firmware/platformio.ini`](firmware/platformio.ini))
- Weboberfläche: statisches HTML/CSS/JS, Live-Updates über **WebSocket**

Aufbau-Übersicht und Begründung der Architektur: [`docs/STRUCTURE.md`](docs/STRUCTURE.md).

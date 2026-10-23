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
| `menuestruktur.md` | Festgelegte Bedienstruktur des OLED-Menüs (Drehgeber) |

## Projektidee in einem Satz

**OrcaOne** ist ein tragbares ESP32-Multitool (NFC/RFID, Infrarot, 2,4 GHz), das
lokal per Drehgeber-Menü und OLED bedient und zusätzlich über eine Weboberfläche
am Smartphone ferngesteuert wird.

## Nutzen / Besucherinteraktion (für Aufgabe 5)

**Nutzen:** OrcaOne macht abstrakte IoT-Konzepte anfassbar. An einem Gerät sieht
man alle drei Kernschritte eines IoT-Systems zugleich: *erfassen* (NFC/IR-Sensor),
*übertragen* (WLAN zum Handy) und *auslösen* (IR-LED, Anzeige). Gleichzeitig zeigt
es, wie dieselbe Technik hinter Alltagsdingen wie Zutrittskarten und
Fernbedienungen steckt.

**Konkreter Ablauf am Stand:**
1. Besucher verbindet sein Handy per **QR-Code** mit dem WLAN „OrcaOne" und öffnet
   die Weboberfläche.
2. Er hält einen (eigenen oder bereitgestellten) RFID-Tag an das Gerät — die UID
   erscheint **live** auf OLED **und** Handy, ohne Neuladen.
3. Er nimmt mit einer Fernbedienung ein IR-Signal auf und spielt es per Knopfdruck
   am Handy wieder ab (z. B. schaltet einen bereitgestellten Fernseher).
4. Er schaltet den **Automatik-Modus** ein: ein registrierter Tag löst nun
   selbstständig eine Aktion aus → zeigt den Unterschied manuell ↔ automatisch.

Das deckt zugleich die Pflicht „Besucher selbst steuern lassen" und
„IoT-Kommunikation erläutern" aus Aufgabe 5 ab.

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

Die Automatik-Regel ist in [`io-uebersicht.md`](io-uebersicht.md) verbindlich
festgelegt (registrierter NFC-Tag → IR-Signal + Anzeige).

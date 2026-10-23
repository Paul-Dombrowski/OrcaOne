# Funktionsumfang der Module (Endausbau)

Legt fest, **was jedes Modul am Ende können soll** — die Zielvorgabe, gegen die
später gebaut und geprüft wird. Noch **keine** Arbeitsaufträge, sondern der
Leistungsumfang.

Kennzeichnung: **[P]** = Pflicht (für Aufgabe/Note nötig) · **[E]** = Erweiterung
(wenn Zeit bleibt). Vorschlag — bitte als Gruppe bestätigen oder ändern.

---

## nfc — NFC/RFID
- **[P]** Tag lesen: UID erfassen und an Display + Handy melden.
- **[P]** Tag beschreiben: gespeicherte Daten auf einen beschreibbaren Tag schreiben.
- **[P]** Klonen: lesen → zwischenspeichern → auf anderen Tag schreiben.
- **[P]** Bis zu 3 Tags in Slots halten (`NFC_SLOT_COUNT`).
- **[E]** Gespeicherte Tags dauerhaft sichern (über Storage, wenn SD vorhanden).
- **[E]** Registrierte Tags für die Automatik verwalten (Liste „registered").

## ir — Infrarot
- **[P]** IR-Signal aufnehmen (empfangen) und in einem Slot speichern.
- **[P]** Gespeichertes Signal senden (Replay).
- **[P]** Bis zu 3 Signale in Slots halten (`IR_SLOT_COUNT`).
- **[E]** Signale dauerhaft sichern (über Storage, wenn SD vorhanden).
- **[E]** Protokoll/Name des Signals anzeigen.

## display — OLED 1,3"
- **[P]** Menü zeichnen (vom MenuController gefüttert).
- **[P]** Statuswerte anzeigen (Modus, letzte UID, Meldungen).
- **[P]** Startbildschirm: Gerätename + WLAN-Status + Modus.
- **[E]** Icons/Grafiken auf dem Display darstellen.

## input — Drehgeber
- **[P]** Drehen erkennen (hoch/runter im Menü).
- **[P]** Tastendruck erkennen (Auswahl/Bestätigen), entprellt.
- **[E]** Langer Druck = „zurück".

## jammer — 2,4 GHz (NRF24L01+)
- **[P]** Über Menü/Handy an- und abschaltbar, sicherer Aus-Zustand beim Start.
- **[P]** Zustand melden (aktiv/aus).
- Sendelogik selbst: durch die Gruppe, im abgestimmten Rahmen (siehe Modul-README).

## storage — zentraler Speicher-Dienst
- **[P]** RAM-Backend: Speichern/Lesen funktioniert ohne SD (flüchtig).
- **[P]** `available()` meldet ehrlich, ob Dauerspeicher da ist.
- **[E]** SD-Backend: dauerhaft, sobald `sdcard` + Karte vorhanden.

## sdcard — Micro-SD (Add-on)
- **[E]** Karte beim Start erkennen, SD-Zugang für den Speicher-Dienst bereitstellen.
- **[E]** Status melden (Karte vorhanden, freier Platz).

## core — ModeManager + MenuController
- **[P]** Automatik ↔ Manuell umschalten (per Menü und Handy).
- **[P]** Automatikregel ausführen: registrierter NFC-Tag → IR senden + Anzeige.
- **[P]** Menü aus den registrierten Modulen aufbauen und bedienen.

## network — WLAN / Web
- **[P]** WLAN-Access-Point ohne Internet.
- **[P]** Weboberfläche ausliefern.
- **[P]** Status-Push (~500 ms) + Befehle empfangen (WebSocket).

---

> Pflicht-Funktionen decken die Mindestanforderungen des Projektauftrags ab
> (2 Sensoren, 2 Aktoren, Automatik, bidirektionale Steuerung, Live-Update).
> Erweiterungen sind optional und erst anzugehen, wenn die Pflicht steht.

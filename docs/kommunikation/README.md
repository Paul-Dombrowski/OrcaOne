# IoT-Kommunikation (Aufgabe 3 — 25 Punkte)

Beschreibt, wie das Smartphone mit dem ESP32 redet. Das ist der am höchsten
bewertete Bereich — hier lohnt sich Sorgfalt.

| Datei | Inhalt |
|---|---|
| `web-api.md` | Genaues Nachrichtenformat: Befehle (Handy→ESP32) und Status (ESP32→Handy) |

## Kurzüberblick

- Der ESP32 ist ein **WLAN-Access-Point** (eigener Hotspot, kein Internet nötig).
  Das Handy verbindet sich mit diesem WLAN und öffnet die Weboberfläche.
- Datenaustausch über **WebSocket** (`/ws`): eine dauerhafte, beidseitige
  Verbindung.
  - **ESP32 → Handy:** regelmäßiger Status-Push (alle ~500 ms) → Werte
    aktualisieren sich automatisch, **ohne Neuladen** (Pflicht).
  - **Handy → ESP32:** Steuerbefehle (Modul ansprechen, Modus umschalten).

## Warum WebSocket statt nur HTTP?

★ Mit reinem HTTP müsste das Handy ständig nachfragen („gibt's was Neues?") —
das ist langsam und ineffizient (Polling). Ein **WebSocket** bleibt offen: der
ESP32 kann von sich aus neue Werte schicken, sobald sie da sind. Das erfüllt die
Anforderung „regelmäßige Aktualisierung ohne manuelles Neuladen" sauber und
stromsparend. Die Firmware-Umsetzung liegt in
[`../../firmware/src/network/`](../../firmware/src/network/).

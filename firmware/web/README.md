# firmware/web/ — Weboberfläche fürs Smartphone (Aufgabe 4 — 15 Punkte)

Die Bedienoberfläche, die Besucher im Browser ihres Handys öffnen. Reines
HTML/CSS/JavaScript — **keine** Installation nötig, läuft im Browser.

## Dateien

| Datei | Inhalt |
|---|---|
| `index.html` | Struktur der Seite (Anzeigen + Bedienelemente) |
| `css/style.css` | Aussehen (mobilfreundlich, große Tap-Flächen) |
| `js/app.js` | Verbindung zum ESP32 (WebSocket) + Logik |

Dieses Startgerüst **verbindet sich bereits** mit dem ESP32, zeigt den Status an
und kann Befehle senden (Modus umschalten, IR senden). Es setzt den Vertrag aus
[`../../docs/kommunikation/web-api.md`](../../docs/kommunikation/web-api.md) um —
diese Datei beim Weiterbauen immer als Referenz nehmen.

## Wie es aufs Handy kommt

Dieser Ordner (`firmware/web/`) **ist** der Upload-Ordner — `platformio.ini` ist so
eingestellt. Kein Kopieren nötig.

1. In PlatformIO **„Upload Filesystem Image"** ausführen → der Inhalt von `web/`
   landet im ESP32-Flash (LittleFS).
2. Der Webserver (`firmware/src/network/`) liefert die Dateien aus.
3. Handy mit dem WLAN **OrcaOne** verbinden und die IP des ESP32 im Browser
   öffnen (steht im Serial-Monitor). Für die Demo: **QR-Code** auf diese Adresse
   (Aufgabe 5).

## Testen ohne ESP32

`index.html` lässt sich zum Layout-Test direkt im Browser öffnen. Die
WebSocket-Verbindung schlägt dann fehl (kein ESP32) — das Aussehen stimmt
trotzdem. Für echte Daten braucht es den laufenden ESP32.

## To-do

- [ ] Für jedes Modul passende Bedienelemente + Anzeigen ergänzen.
- [ ] Verbindungsstatus sichtbar machen (verbunden / getrennt).
- [ ] Gestaltung besucherfreundlich: große Buttons, klare Beschriftung.

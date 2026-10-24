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

Die Dateien in diesem Ordner müssen in ein eigenes Flash-Dateisystem (LittleFS)
des ESP32.

> ⚠️ **Noch nicht eingerichtet (Schritt 3 der ESP-IDF-Umstellung).** Unter
> PlatformIO erledigte das der Knopf „Upload Filesystem Image". Unter ESP-IDF
> braucht es eine **LittleFS-Partition** (Eintrag in der Partitionstabelle) und
> einen **Image-Schritt**, der `web/` in ein LittleFS-Abbild packt und flasht
> (z. B. über die Komponente `joltwallet/littlefs` mit
> `littlefs_create_partition_image`). Das wird beim Anbinden der Weboberfläche
> eingerichtet und hier mit dem konkreten `idf.py`-Befehl ergänzt. **Bis dahin
> gibt es keinen funktionierenden Upload-Schritt.**

Sobald das steht, ist der Ablauf:
1. Firmware flashen (`idf.py flash`) **und** das LittleFS-Image flashen.
2. Der Webserver (`firmware/src/network/`) liefert die Dateien aus.
3. Handy mit dem WLAN **OrcaOne** verbinden und die IP des ESP32 im Browser
   öffnen (steht im Serial Monitor). Für die Demo: **QR-Code** auf diese Adresse
   (Aufgabe 5).

## Fremd-Bibliotheken (JS/CSS)

- Ablage: **`vendor/`** in diesem Ordner (bei Bedarf anlegen), z. B.
  `vendor/chart.min.js`, eingebunden mit `<script src="vendor/chart.min.js">`.
- **Kein CDN-Link** (`https://cdn...`): Das Handy hängt im WLAN des ESP32 und hat
  dort **kein Internet**, die Datei würde nicht laden.
- Immer die **minifizierte** Version (`*.min.js` / `*.min.css`) nehmen. Der
  Speicher auf dem ESP32 (LittleFS) ist klein (ca. 1,5 MB für alles in `web/`).
- Lizenzdatei mit ablegen (`vendor/<name>.LICENSE.txt`). Allgemeine Regeln:
  [`../lib/README.md`](../lib/README.md).

## Testen ohne ESP32

`index.html` lässt sich zum Layout-Test direkt im Browser öffnen. Die
WebSocket-Verbindung schlägt dann fehl (kein ESP32) — das Aussehen stimmt
trotzdem. Für echte Daten braucht es den laufenden ESP32.

## To-do

- [ ] Für jedes Modul passende Bedienelemente + Anzeigen ergänzen.
- [ ] Verbindungsstatus sichtbar machen (verbunden / getrennt).
- [ ] Gestaltung besucherfreundlich: große Buttons, klare Beschriftung.

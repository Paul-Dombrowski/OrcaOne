# OrcaOne Smartphone-Menü

Statische, mobile-first Oberfläche für den dokumentierten OrcaOne-Menüumfang:
NFC-Status und Lesen, Infrarot-Status und Aufnahme sowie System-/Modulstatus.
Das Layout orientiert sich am OLED-Konzept aus
[`../src/modules/display/README.md`](../src/modules/display/README.md)
(Menüauswahl, Status und kurze Rückmeldungen), ersetzt aber kein vorhandenes
OLED-Menü. Die Seite besteht ausschließlich aus HTML, CSS und JavaScript; sie
benötigt keine Installation und lädt keine externen Schriftarten oder
Bibliotheken.

## Aktueller Implementierungsstand

Die Weboberfläche verarbeitet den Vertrag in
[`../../docs/kommunikation/web-api.md`](../../docs/kommunikation/web-api.md):

- Verbindet sich bei Auslieferung per HTTP(S) mit `ws(s)://<host>/ws` und
  verarbeitet JSON-Statusmeldungen sowie optionale Ereignisse.
- Zeigt Verbindungszustand, letzten Statuszeitpunkt, Betriebsmodus,
  NFC-UID/Modulbereitschaft und IR-Aufnahmezustand/Protokoll an.
- Sendet ausschließlich `nfc/read`, `ir/record` und `ir/stop`.
  Die IR-Stopp-Schaltfläche ist nur während einer gemeldeten Aufnahme aktiv.
- Aktionen bleiben deaktiviert, solange keine Verbindung, keine Statusmeldung
  oder kein Modul mit `ok: true` vorliegt. Für NFC wird zusätzlich ein
  ausdrücklich gemeldetes `ready: false` berücksichtigt. Fehlende Modulstatus
  gelten als unbekannt und nicht als bereit.
- Die Oberfläche behauptet keine Befehlsquittung: Nach dem Senden wird auf
  Statusupdates gewartet. Der Vertrag definiert Status-Pushes und optionale
  Ereignisse, aber keine verlässliche Quittung für jeden Befehl.
- 2,4 GHz und Automatik/Moduswechsel sind als nicht verfügbar gekennzeichnet.
  Es gibt dafür keine Aktionsschaltflächen.

### Firmwaregrenzen — keine Live-Integration vorhanden

`firmware/src/main.cpp` bindet derzeit nur das NFC-Modul direkt für den
vorläufigen Grundtest ein. IR, Display, Eingabe, `ModeManager`,
`CommandRouter` und `NetworkManager` sind auskommentiert; die Initialisierung
und Bedienung über WLAN/WebSocket stehen dort in TODO-Kommentaren. Der
Firmware-Netzwerkserver wird deshalb aktuell nicht gestartet und liefert auch
keine Live-Statusmeldungen an diese Seite. Die WebSocket-Implementierung ist
der Client für die dokumentierte künftige Schnittstelle, kein Nachweis einer
bereits funktionierenden Geräteverbindung.

Auch wenn die API-Dokumentation weitere Befehle aufführt, blendet diese
Oberfläche NFC-Schreib-/Klon-Aktionen, IR-Senden/Wiederholen und
2,4-GHz-Steuerung aus. Die NRF24-Funktion ist laut Modul-README noch nicht
implementiert. Der Betriebsmodus wird – falls künftig gemeldet – nur angezeigt;
`setMode` ist nicht angebunden.

## Protokollkontrakt

- Transport: WebSocket auf `/ws`, Textframes mit JSON.
- ESP32 → Browser: `{ "type": "status", "mode": "manual", "modules": { ... } }`.
  Statusmeldungen kommen laut Vertrag ungefähr alle 500 ms. `mode` ist
  `auto` oder `manual`; Modulobjekte müssen ein boolesches `ok` enthalten.
- Erwartete Modulfelder: NFC `ok`, `ready`, `lastUid`, optional `error`; IR `ok`
  und entweder `recording` oder `state` (`"recording"`/`"idle"`); optional
  `hardwareDetectable` und `lastProtocol`. Ein nicht gemeldetes oder nicht
  bereites Modul deaktiviert seine Aktionen. Wenn `hardwareDetectable: false`
  gemeldet wird, erklärt die Anzeige, dass angeschlossene IR-Hardware nicht
  überprüft werden kann.
- Browser → ESP32: `{ "module": "...", "action": "...", "params": {} }`.
  Diese Oberfläche sendet nur:

  | Aktion | JSON-Befehl |
  |---|---|
  | NFC lesen | `{"module":"nfc","action":"read","params":{}}` |
  | IR aufnehmen | `{"module":"ir","action":"record","params":{}}` |
  | IR stoppen | `{"module":"ir","action":"stop","params":{}}` |

- Optionale Ereignisse `{ "type":"event", "ok":true, "message":"..." }` werden
  als Rückmeldung angezeigt. Es werden nur feste, erlaubte Befehle erzeugt;
  Nachrichteninhalte des Geräts werden als Text und nicht als HTML dargestellt.

Referenz: [`../../docs/kommunikation/web-api.md`](../../docs/kommunikation/web-api.md).

## Vorschau und Bereitstellung

- `index.html` direkt im Browser öffnen oder von `localhost` ausliefern: Die
  Seite zeigt den lokalen Vorschaumodus, öffnet **keine** WebSocket-Verbindung
  und hält alle Hardwareaktionen deaktiviert. Insbesondere wird kein ungültiges
  `ws://null/ws` erzeugt.
- Für Live-Daten müssen die Dateien vom ESP32 (oder einem gleichursprünglichen
  HTTP-Server mit passendem `/ws`-WebSocket-Endpunkt) ausgeliefert werden.
  Außerhalb lokaler Vorschau versucht die Seite, `/ws` am Seiten-Host zu öffnen
  und verbindet sich bei Verbindungsabbruch erneut.
- Der Upload der Dateien in LittleFS ist in der Firmware noch nicht eingerichtet;
  siehe [`../README.md`](../README.md). Livebetrieb ist daher erst möglich, wenn
  sowohl die Webdateien bereitgestellt als auch `NetworkManager` in der Firmware
  eingebunden ist.

## Dateien

| Datei | Zweck |
|---|---|
| `index.html` | Zugängliche Struktur, Statusanzeigen und Menübereiche |
| `css/style.css` | Orca-inspirierte, responsive Gestaltung ohne externe Assets |
| `js/app.js` | WebSocket-Client, Statusdarstellung, Bereitschaftsprüfung und sichere Befehlsauswahl |

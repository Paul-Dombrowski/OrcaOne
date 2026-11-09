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
  NFC-UID/Kartentyp, IR-Betriebszustand und Speicherplätze sowie den
  NRF24- und Systemstatus an.
- Sendet ausschließlich `nfc/read`, `ir/record` und `ir/stop`.
  Die IR-Stopp-Schaltfläche ist bei gemeldeter Aufnahme oder Wiedergabe aktiv.
- Aktionen bleiben deaktiviert, solange keine Verbindung, keine Statusmeldung
  oder kein Modul mit `ok: true` vorliegt. Für NFC wird zusätzlich ein
  ausdrücklich gemeldetes `ready: false` berücksichtigt. Fehlende Modulstatus
  gelten als unbekannt und nicht als bereit.
- Die Oberfläche behauptet keine Befehlsquittung: Nach dem Senden wird auf
  Statusupdates gewartet. Der Vertrag definiert Status-Pushes und optionale
  Ereignisse, aber keine verlässliche Quittung für jeden Befehl.
- Der NRF24-Status sowie `manual`/`no-mercy` werden angezeigt. Es gibt keine
  Web-Steuerung für NRF24 oder den No-Mercy-Modus.
- NFC-Emulation und IR-Senden/Ausschalt-Sequenz werden nicht über die
  Weboberfläche ausgelöst. Die NFC-Emulation ist mit der aktuellen I2C-Anbindung
  laut Modul-Dokumentation nicht funktionsfähig.

### Firmwaregrenzen — Webserver noch nicht integriert

`firmware/src/main.cpp` initialisiert inzwischen Display, Eingabe, NFC, IR,
NRF24, `ModeManager`, `CommandRouter`, Serial-Konsole und den WLAN-Access-Point.
Der Access-Point wird gestartet, der HTTP-Webserver und der WebSocket-Server
(`/ws`) sind jedoch noch nicht implementiert. Die WebSocket-Implementierung
dieser Seite ist daher der Client für die dokumentierte Schnittstelle; Live-
Daten sind erst verfügbar, wenn die Firmware die Webdateien ausliefert und
Statusmeldungen sowie Befehle über `/ws` verarbeitet.

Die Oberfläche bleibt absichtlich bei NFC-Lesen und IR-Aufnahme/Stopp. Andere
in der Firmware vorhandene Aktionen wie NFC-Emulation sowie IR-Senden und
Ausschalt-Sequenz werden nicht per Weboberfläche angeboten.

## Protokollkontrakt

- Transport: WebSocket auf `/ws`, Textframes mit JSON.
- ESP32 → Browser: `{ "type": "status", "mode": "manual", "modules": { ... } }`.
  Statusmeldungen kommen laut Vertrag ungefähr alle 500 ms. `mode` ist derzeit
  `manual` oder `no-mercy`; Modulobjekte müssen ein boolesches `ok` enthalten.
- Erwartete Modulfelder: NFC `ok`, `ready`, `lastUid`, `lastCardType`,
  `readPending`, `emulating` und optional `error`; IR `ok`, `state` (`"idle"`,
  `"recording"`, `"sending"` oder `"shutoff"`), `slots`, `newest`,
  `hardwareDetectable`, `hasShutoff`; NRF24 `ok`, `active`, `channel`.
  Ein nicht gemeldetes oder nicht bereites Modul deaktiviert seine Aktionen.
  Wenn `hardwareDetectable: false` gemeldet wird, zeigt die Oberfläche an, dass
  die IR-Hardware nicht geprüft werden kann.
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
  sowohl die Webdateien bereitgestellt als auch HTTP- und WebSocket-Server in
  der Firmware eingebunden sind.

## Dateien

| Datei | Zweck |
|---|---|
| `index.html` | Zugängliche Struktur, Statusanzeigen und Menübereiche |
| `css/style.css` | Orca-inspirierte, responsive Gestaltung ohne externe Assets |
| `js/app.js` | WebSocket-Client, Statusdarstellung, Bereitschaftsprüfung und sichere Befehlsauswahl |

# IR-Modul + minimale Serial-Bedienung — Design

> Status: freigegeben (Brainstorming 2026-11-05). Umsetzungsplan:
> [`../plans/2026-11-05-ir-modul-serial-bedienung.md`](../plans/2026-11-05-ir-modul-serial-bedienung.md)

## 1. Absicht / Warum

OrcaOne braucht sein erstes Aktor/Sensor-Modul neben NFC: **Infrarot**. Zweck des
IR-Moduls ist **Störung** („IR-Jammer") — ein IR-gesteuertes Gerät unbenutzbar
machen. Zwei Betriebsarten, beide laufen dauerhaft bis zum Abbruch.

Mit dem **zweiten** Modul entsteht erstmals das Problem der Befehls-Zuordnung über
den Serial Monitor (bisher schickt `main.cpp` jede Zeile fest an NFC). Dafür wird
die ohnehin vorgesehene Dispatch-Schicht `core/CommandRouter` gebaut und eine
minimale `console/SerialConsole`. Das provisorische Serial-Handling
(`pollSerialCommands`) in `main.cpp` wird dadurch abgelöst.

## 2. Ziele

- `IrModule` nach dem 4-Methoden-Vertrag (`core/Module.h`), `name()` = `"ir"`.
- **Modus A „Aufnehmen/Senden":** IR-Signale eines Zeitfensters mitschneiden und
  blind (zufällig, in Dauerschleife) zurückspielen.
- **Modus B „Ausschalt-Sequenz":** kuratierte Off-Code-Liste von SD in Dauerschleife
  senden (TV-B-Gone-Prinzip).
- Jede IR-Funktion sofort über **flache Serial-Befehle** bedienbar:
  `ir record`, `ir send [slot=n]`, `ir shutoff`, `ir stop`, plus `status`/`help`.
- Routing **ausschließlich** über `core/CommandRouter` (ein Weg für Serial, später
  Handy und Menü).

## 3. Nicht-Ziele / Anti-Ziele

- **Kein** navigierbares „reingehen-und-bleiben"-Menü jetzt. Das liefert später der
  `MenuController` der Display-Person (rendert dann auch in den Serial Monitor)
  über denselben `CommandRouter`. Ein temporäres Menü wäre Wegwerf-Arbeit und würde
  mit dem späteren MenuController kollidieren.
- **Kein** `ModeManager`/Automatik in diesem Projekt. Der `CommandRouter` wird aber
  so gebaut, dass er später ergänzt werden kann; `"system"`-Befehle werden vorerst
  abgelehnt.
- **Kein** gezieltes Wiedergeben einzelner Fernbedienungstasten — es kommt bewusst
  nicht auf das „richtige" Signal an.
- `core/Module.h` wird **nicht** angefasst (ARBEITSWEISE §6 Regel 3). Kein
  Modul↔Modul (§6 Regel 2). Keine Netzwerk-/Web-Arbeit.

## 4. Randbedingungen

- Build mit **ESP-IDF** (`idf.py`), Arduino-Core als Komponente. Board ESP32 D1 R32.
- §6: **kein `delay()` in `update()`**; lange Abläufe als `millis()`-Zustandsautomat;
  **kein `while(1)` / `ESP.restart()`**; fehlende Hardware blockiert nie das Gerät.
- Pins **nur** aus `config.h` (`PIN_IR_RX` = 34 input-only, `PIN_IR_TX` = 4).
- Speichern **nur** über globales `Storage`, Namensraum `"ir"`. **RAM-Fallback:**
  Aufnehmen/Senden läuft auch ohne SD (flüchtig, `Storage.available()==false`).
  **Ausschalt-Sequenz braucht die SD-Datei** — ohne SD ist dieser Modus inaktiv.
- Bibliothek **IRremoteESP8266** als Komponente in `firmware/components/` (Fall 2:
  Arduino-Lib ohne IDF-Paket → eigener Ordner mit `CMakeLists.txt` + `LICENSE` +
  `HERKUNFT.md`, siehe `components/README.md`).
- Kommentare Deutsch, Bezeichner Englisch (STRUCTURE §5).

## 5. Entschiedenes Design (das „Wie")

### 5.1 Slots / Speicherung

- **Ringpuffer.** Anzahl **backend-abhängig**: mit SD `IR_SD_SLOT_COUNT` = 50, ohne
  SD `IR_RAM_SLOT_COUNT` = 3 (Heap-Schutz; rohe IR-Mitschnitte sind groß). Die
  aktive Anzahl `N` wird einmal in `begin()` aus `Storage.available()` bestimmt.
- `record` wählt **keinen** Slot vorab: es wird **automatisch ein neuer** Slot
  angelegt; ist der Puffer voll, wird der **älteste** überschrieben.
- Schlüssel: `slot1`..`slotN`. Verwaltungssatz `meta` =
  `{ "next":<1..N>, "newest":<0..N>, "count":<0..N> }` (0/leer = noch nichts).
- `send` **ohne** Slot = **neuester**; `send slot=n` = gezielt.
- **Signalformat** je Slot (JSON-Text):
  `{ "v":1, "signals":[ { "khz":38, "raw":[<uint16 µs>, ...] }, ... ] }`.
  Rohe Timing-Arrays (für beliebige/unbekannte Signale robuster als
  Protokoll-Dekodierung; passt zum Jammer-Zweck).
- Grenzen (Heap/SD-Schutz, konservativ, auf Hardware feinjustieren):
  `IR_MAX_SIGNALS_PER_SLOT` = 20, Capture-Puffer `IR_CAPTURE_BUFFER` = 256
  Einträge. Ist das Fenster voll, stoppt die Aufnahme dieses Mitschnitts.

### 5.2 Modus A — Aufnehmen/Senden

- `record`: Zustand `recording`. Gegen GPIO34-Rauschen `setUnknownThreshold(12)`.
  Jedes dekodierte Signal wird **nur im Recording-Zustand** als `raw` angehängt.
  `stop` → Liste in einen **neuen** Slot schreiben, Serial meldet die Signalzahl.
- `send [slot]`: Zustand `sending`. Wählt **zufällig** ein Signal aus dem Slot,
  `sendRaw`, Pause `IR_SEND_GAP_MS` = 100 ms (per `millis`), Schleife **bis `stop`**.

### 5.3 Modus B — Ausschalt-Sequenz

- `shutoff`: lädt `Storage.load("ir","shutoff")` (gleiches Signalformat). Ohne SD
  **oder** ohne/kaputte Datei: **einmal** melden, `false` zurückgeben, Modus bleibt
  inaktiv (kein Absturz). Sonst Zustand `shutoff`: sendet die Codes der Reihe nach
  in **Dauerschleife**, Pause `IR_SEND_GAP_MS`, **bis `stop`**.

### 5.4 Status (`statusJson`) — erweitert

`{ "ok":true, "state":"idle|recording|sending|shutoff", "slots":<count>,
"newest":<n|0>, "hasShutoff":<bool> }`

Ersetzt das alte feste `slots:[bool,bool,bool]`. Rückwärtskompatibel ist das
**nicht**, aber web-seitig ist IR noch nicht implementiert → `web-api.md` wird
angepasst und der Web-/Display-Person mitgeteilt (ARBEITSWEISE §7).

### 5.5 Bedienung / Routing

- **`SerialConsole`** (global erreichbares `Console`, Muster wie `Storage`):
  nicht-blockierend lesen (`\r` **und** `\n` als Zeilenende, Zeichen-Echo, Puffer
  128), Zeile `<module> <action> [k=v ...]`; Zeile, die mit `{` beginnt → direkt als
  JSON; eingebaut `help`, `status`. Verteilt **nur** über `CommandRouter.dispatch`,
  ruft **nie** direkt ein Modul (console/README §Umsetzungshinweise).
  Zahl-Werte (`slot=2`) werden als Zahl übergeben, sonst als Text.
- **`CommandRouter`**: bekommt beim Start die Modul-Liste (noch ohne `ModeManager`).
  `"system"` wird vorerst abgelehnt; `mode` im Status fest `"manual"`.
- **`main.cpp`**: Modul-Liste `{ &nfc, &ir }`; alle über die Liste `begin()`/
  `update()`; `Console.update()` im `loop()`; provisorisches `pollSerialCommands`
  und der Blink-Test werden entfernt.

### 5.6 Vertrags-Updates (Doku, Teil der jeweiligen Tasks)

- `docs/kommunikation/web-api.md`: `ir record`/`send` → `slot` **optional**; neuer
  IR-Status; `shutoff` „in Dauerschleife bis `stop`".
- `firmware/src/modules/ir/README.md`: Slots/Ringpuffer/Format/Status aktualisieren,
  To-do abhaken.
- `firmware/components/README.md`: Zeile `IRremoteESP8266` ergänzen.

## 6. Dem Umsetzer überlassen

- Idiomatische IRremoteESP8266-Aufrufe (`enableIRIn`, `decode`, `resume`,
  `resultToRawArray`/`getCorrectedRawLength`, `sendRaw`) in Projektkonvention; den
  von `resultToRawArray` zurückgegebenen Heap-Puffer freigeben.
- Vernünftiges Clampen, falls `N` zwischen zwei Starts schrumpft (SD entfernt).
- ArduinoJson-Dokumentgrößen.
- Fehlertexte/Präfixe im Serial-Stil aus `console/README.md` (`[ir] …`, `[console] …`).

## 7. Offene Risiken

- **Heap:** Rohe Mitschnitte können groß werden. Grenzen in 5.1 sind konservativ
  gesetzt und auf echter Hardware zu prüfen (Monitor-Ausgabe des freien Heaps).
- IRremoteESP8266 ist umfangreich; der erste Schritt ist bewusst ein reiner
  Build-Test, damit Komponenten-Probleme früh auffallen.

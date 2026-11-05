# IR-Modul + minimale Serial-Bedienung — Implementierungsplan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ein IR-Jammer-Modul (Mitschnitt-Replay + Ausschalt-Sequenz) nach dem Modul-Vertrag, bedienbar über eine minimale Serial-Konsole mit zentralem Befehls-Router.

**Architecture:** `IrModule` erbt von `Module` und arbeitet als `millis()`-Zustandsautomat. Befehle laufen von `SerialConsole` → `CommandRouter.dispatch()` → `Module.handleCommand()`. Persistenz über das globale `Storage` (Ringpuffer-Slots, Namensraum `"ir"`). `main.cpp` verdrahtet Module-Liste, Router und Konsole und löst das provisorische Serial-Handling ab.

**Tech Stack:** C++ / Arduino-Core auf ESP-IDF (`idf.py`), ArduinoJson, IRremoteESP8266 (als `components/`-Komponente).

**Spec:** [`../specs/2026-11-05-ir-modul-serial-bedienung-design.md`](../specs/2026-11-05-ir-modul-serial-bedienung-design.md)

## Global Constraints

- Build ausschließlich mit ESP-IDF: `idf.py build` (im Ordner `firmware/`, aktivierte ESP-IDF-5.5-Umgebung). Board-Target `esp32`.
- **Kein `delay()` in `update()`**; lange Abläufe als `millis()`-Zustandsautomat. **Kein `while(1)` / `ESP.restart()`**; fehlende Hardware blockiert nie (ARBEITSWEISE §6).
- `core/Module.h` bleibt bei genau 4 Methoden — **nicht** ändern (§6 Regel 3). Kein Modul↔Modul (§6 Regel 2).
- Pins/Konstanten **nur** aus `firmware/src/config.h` (`PIN_IR_RX`=34, `PIN_IR_TX`=4).
- Speichern nur über globales `Storage`, Namensraum `"ir"`. Nie abbrechen, wenn keine SD da ist (RAM-Fallback).
- Fremd-Bibliothek als `components/`-Komponente mit `CMakeLists.txt` + `LICENSE` + `HERKUNFT.md` (components/README §Fall 2).
- Kommentare Deutsch, Bezeichner Englisch.
- **Verifikation (kein Host-Test-Framework vorhanden):** Gate je Task ist (a) `idf.py build` grün **und** (b) die angegebene manuelle Serial-Monitor-Prüfung mit erwarteter Ausgabe. Konsequenz: auf dem Zielgerät flashen/monitoren. Ohne Hardware ist mindestens (a) + der jeweils als „ohne Hardware" markierte Teil von (b) zu prüfen.

## Review Focus

Vom Spec implizierte Eingaben/Fehlerfälle, die eine reale Nutzung treffen — jeweils dem Task zugeordnet, der den Test trägt:

- **Überlange Serial-Zeile (>128 Zeichen):** darf nicht überlaufen/Müll routen → verwerfen + melden. (Task 2)
- **Unbekanntes Modul/Aktion über Serial:** freundliche Meldung, kein Absturz, `dispatch` gibt `false`. (Task 2)
- **`ir send` ohne vorhandenen Mitschnitt (leerer Ringpuffer):** melden „nichts gespeichert", kein Absturz. (Task 4)
- **`ir send slot=n` mit nicht existierendem/zu großem Slot:** melden, kein Absturz. (Task 4)
- **`ir shutoff` ohne SD / ohne bzw. kaputte `shutoff`-Datei:** einmal melden, inaktiv bleiben, kein Absturz. (Task 5)
- **Aufnahme-Fenster über `IR_MAX_SIGNALS_PER_SLOT`:** Aufnahme stoppt sauber, kein Heap-Überlauf. (Task 3)

---

### Task 1: IRremoteESP8266-Komponente + IR-Konstanten

**Files:**
- Create: `firmware/components/IRremoteESP8266/CMakeLists.txt`
- Create: `firmware/components/IRremoteESP8266/HERKUNFT.md`
- Create: `firmware/components/IRremoteESP8266/LICENSE` (Original-Lizenz der Bibliothek)
- Create: `firmware/components/IRremoteESP8266/src/…` (Bibliotheksquellen des Originals)
- Modify: `firmware/src/config.h` (IR-Konstanten ergänzen/ersetzen)
- Modify: `firmware/components/README.md` (Tabelle „Aktuell enthaltene Komponenten")

**Interfaces:**
- Produces: Header `IRremoteESP8266.h`, `IRrecv.h`, `IRsend.h`, `IRutils.h` projektweit includebar; Konstanten `IR_SD_SLOT_COUNT` (50), `IR_RAM_SLOT_COUNT` (3), `IR_MAX_SIGNALS_PER_SLOT` (20), `IR_CAPTURE_BUFFER` (256), `IR_SEND_GAP_MS` (100), `IR_RECORD_UNKNOWN_THRESHOLD` (12) in `config.h`.

- [ ] **Step 1:** Bibliotheksquellen von `crankyoldgit/IRremoteESP8266` (Release-Tag in `HERKUNFT.md` notieren) nach `firmware/components/IRremoteESP8266/src/` kopieren; `LICENSE` des Originals mitnehmen.
- [ ] **Step 2:** `CMakeLists.txt` schreiben:
```cmake
idf_component_register(
  SRC_DIRS "src"
  INCLUDE_DIRS "src"
  REQUIRES espressif__arduino-esp32
)
```
- [ ] **Step 3:** `HERKUNFT.md` nach dem Pflichtmuster aus `components/README.md` ausfüllen (Quelle/Version/Lizenz/Geändert=nein/Genutzt von=modules/ir).
- [ ] **Step 4:** In `config.h` den bisherigen `IR_SLOT_COUNT` durch die sechs oben genannten IR-Konstanten ersetzen (NFC-Konstanten unberührt). Werte verbatim aus der Spec 5.1/5.2.
- [ ] **Step 5:** In `components/README.md` eine Tabellenzeile `IRremoteESP8266 | IR senden/empfangen | modules/ir` ergänzen.
- [ ] **Step 6: Build-Prüfung** — Run: `idf.py build`. Expected: erfolgreicher Build; die neue Komponente wird ohne Fehler übersetzt. (Noch kein Nutzcode — reiner Komponenten-Smoke-Test.)
- [ ] **Step 7: Commit**
```bash
git add firmware/components/IRremoteESP8266 firmware/src/config.h firmware/components/README.md
git commit -m "IR: IRremoteESP8266 als Komponente + IR-Konstanten in config.h"
```

---

### Task 2: CommandRouter + SerialConsole (löst provisorisches Serial-Handling ab)

**Files:**
- Create: `firmware/src/core/CommandRouter.h`, `firmware/src/core/CommandRouter.cpp`
- Create: `firmware/src/console/SerialConsole.h`, `firmware/src/console/SerialConsole.cpp`
- Modify: `firmware/src/main.cpp` (Router + Konsole verdrahten, `pollSerialCommands` + Blink-Test entfernen)

**Interfaces:**
- Consumes: `Module` (`core/Module.h`), globales `Storage`.
- Produces:
  - `class CommandRouter { void begin(Module** modules, size_t count); bool dispatch(const String& module, const String& action, JsonObjectConst params); void buildStatus(JsonObject root); };`
  - `class SerialConsole { void begin(CommandRouter* router); void update(); };` + `extern SerialConsole Console;`

- [ ] **Step 1:** `CommandRouter` implementieren. `begin` merkt sich `modules`/`count`. `dispatch`: bei `module=="system"` → `false` (noch kein ModeManager); sonst Modul mit passendem `name()` suchen und dessen `handleCommand(action, params)` zurückgeben; unbekanntes Modul → `false`. `buildStatus`: `root["mode"]="manual"`, `root["modules"]` als Objekt, je Modul ein Unterobjekt aus dessen `statusJson()`.
- [ ] **Step 2:** `SerialConsole` implementieren. `update()` liest nicht-blockierend (`while (Serial.available())`), echot jedes Zeichen (`Serial.write(c)`), Zeilenende bei `\r` **oder** `\n` (leere Zeile überspringen), Puffer auf **128** begrenzen — längere Zeile verwerfen und `[console] Zeile zu lang` melden. Zeile, die mit `{` beginnt → mit ArduinoJson parsen und `module`/`action`/`params` an `dispatch`. Sonst in Tokens zerlegen: Token 1 = `module`, Token 2 = `action`, weitere `k=v` → `params` (nur Ziffern → Zahl, sonst Text). Eingebaut: `help` (Syntax + Modulnamen), `status` (`buildStatus` als JSON ausgeben). Antworten mit Präfix `[modul] …` / `[console] …` nach console/README. `Console` als globales Objekt in genau einer `.cpp` definieren.
- [ ] **Step 3:** `main.cpp` umbauen: `#include` für `IrModule` entfällt noch (Task 3) — vorerst nur `nfc` in der Liste. Globale `Module* modules[] = { &nfc };`, `CommandRouter router;`. In `setup()`: nach `Storage.begin()` alle Module per Schleife `begin()`, dann `router.begin(modules, 1); Console.begin(&router);`. `pollSerialCommands()` und `blinkUpdate()` samt zugehörigem Code löschen. In `loop()`: `for (auto* m : modules) m->update(); Console.update();` behalten, `delay(1)` behalten.
- [ ] **Step 4: Build-Prüfung** — Run: `idf.py build`. Expected: erfolgreicher Build.
- [ ] **Step 5: Manuelle Prüfung** (`idf.py -p <PORT> flash monitor`):
  - `status` → gibt JSON mit `"mode":"manual"` und `"modules":{"nfc":{…}}` aus.
  - `nfc read` → `[nfc] read: …` (bei fehlendem PN532: abgelehnt — kein Absturz).
  - `foo bar` → `[console] unbekanntes Modul "foo" …`.
  - Zeile >128 Zeichen einfügen → `[console] Zeile zu lang`, Gerät läuft weiter.
  - (ohne Hardware prüfbar: alle vier Punkte, da sie keine IR/NFC-Bauteile brauchen.)
- [ ] **Step 6: Commit**
```bash
git add firmware/src/core/CommandRouter.* firmware/src/console/SerialConsole.* firmware/src/main.cpp
git commit -m "core/console: CommandRouter + SerialConsole, loest provisorisches Serial-Handling ab"
```

---

### Task 3: IrModule — Grundgerüst + Aufnahme (record/stop, Ringpuffer-Slots)

**Files:**
- Create: `firmware/src/modules/ir/IrModule.h`, `firmware/src/modules/ir/IrModule.cpp`
- Modify: `firmware/src/main.cpp` (IR in Modul-Liste)
- Modify: `firmware/src/modules/ir/README.md` (Status/Slots/Format aktualisieren)

**Interfaces:**
- Consumes: `Module`, globales `Storage`, IRremoteESP8266-Header, IR-Konstanten aus `config.h`, `CommandRouter` (aus Task 2, via `main.cpp`).
- Produces: `class IrModule : public Module` mit `name()`=="ir". Interner Zustand `enum class State { Idle, Recording, Sending, Shutoff }`. Slot-Verwaltung über `Storage` (`meta` + `slot1..slotN`), Signalformat laut Spec 5.1.

- [ ] **Step 1:** `IrModule` anlegen. `begin()`: `N = Storage.available() ? IR_SD_SLOT_COUNT : IR_RAM_SLOT_COUNT`; `meta` laden (fehlt → `next=1,newest=0,count=0`); `IRrecv`(`PIN_IR_RX`, `IR_CAPTURE_BUFFER`) + `IRsend`(`PIN_IR_TX`) anlegen, `setUnknownThreshold(IR_RECORD_UNKNOWN_THRESHOLD)`, `enableIRIn()`. Keine Hardware-Erkennung → `ok` immer `true`.
- [ ] **Step 2:** `handleCommand`: `record` → Zustand `Recording`, Capture-Liste leeren, `true`. `stop` → wenn `Recording`: Liste in **neuen** Slot `slot<next>` schreiben (JSON laut 5.1), `meta` fortschreiben (`newest=next`, `count=min(count+1,N)`, `next=next%N+1`), Serial `[ir] aufgenommen: <k> Signale in slot<x>`; zurück zu `Idle`, `true`. `send`/`shutoff` → in diesem Task nur `false` (folgt Task 4/5). Unbekannte Aktion → `false`.
- [ ] **Step 3:** `update()`: nur im Zustand `Recording` aktiv — `if (recv.decode(&results))` → via `resultToRawArray`/`getCorrectedRawLength` ein `{khz,raw[]}` an die Capture-Liste hängen (Heap-Puffer danach freigeben), `recv.resume()`. Bei Erreichen von `IR_MAX_SIGNALS_PER_SLOT`: Aufnahme beenden wie `stop`, Serial `[ir] Fenster voll, Aufnahme beendet`. Nicht-blockierend, kein `delay`.
- [ ] **Step 4:** `statusJson`: `ok=true`, `state` (idle/recording/…), `slots=meta.count`, `newest=meta.newest`, `hasShutoff` = `Storage.load("ir","shutoff").length() > 0`.
- [ ] **Step 5:** `main.cpp`: `#include "modules/ir/IrModule.h"`, globale `IrModule ir;`, Liste auf `{ &nfc, &ir }` (count 2), `router.begin(modules, 2)`.
- [ ] **Step 6:** `ir/README.md`: Abschnitt Status/Slots an Spec 5.1/5.4 angleichen (festes `slots:[…]` ersetzen), To-do `record` abhaken.
- [ ] **Step 7: Build-Prüfung** — Run: `idf.py build`. Expected: erfolgreicher Build.
- [ ] **Step 8: Manuelle Prüfung** (Monitor, IR-Empfänger an GPIO34):
  - `status` → enthält `"ir":{"ok":true,"state":"idle","slots":0,"newest":0,…}`.
  - `ir record`, Fernbedienungstaste(n) drücken, `ir stop` → `[ir] aufgenommen: <n> Signale in slot1`; `status` zeigt `slots:1, newest:1`.
  - Viele Signale senden, bis `IR_MAX_SIGNALS_PER_SLOT` → `[ir] Fenster voll, Aufnahme beendet`, kein Absturz. *(Review-Focus: Fenster voll.)*
  - (ohne Empfänger prüfbar: `status`-Felder + dass `ir record`/`ir stop` ohne Signal sauber einen leeren/kurzen Slot melden, kein Rauschen dank Threshold.)
- [ ] **Step 9: Commit**
```bash
git add firmware/src/modules/ir/IrModule.* firmware/src/main.cpp firmware/src/modules/ir/README.md
git commit -m "IR: IrModule-Geruest + Aufnahme (record/stop) mit Ringpuffer-Slots"
```

---

### Task 4: IR senden (send: neuester/slot=n, zufällige Dauerschleife)

**Files:**
- Modify: `firmware/src/modules/ir/IrModule.h`, `firmware/src/modules/ir/IrModule.cpp`
- Modify: `docs/kommunikation/web-api.md` (`ir send`/`record` → `slot` optional; IR-Status)
- Modify: `firmware/src/modules/ir/README.md` (To-do `send` abhaken)

**Interfaces:**
- Consumes: Slot-/`meta`-Verwaltung und Signalformat aus Task 3; `IR_SEND_GAP_MS`.
- Produces: Zustand `Sending` im `IrModule`; `send`-Aktion mit optionalem `params["slot"]`.

- [ ] **Step 1:** `handleCommand` erweitern: `send` → Zielslot bestimmen: `params["slot"]` falls vorhanden, sonst `meta.newest`. Ist `meta.count==0` → `[ir] nichts gespeichert`, `false`. Existiert der Slot nicht/ist leer → `[ir] slot<n> leer/unbekannt`, `false`. Sonst Slot laden, Signalliste parsen, Zustand `Sending`, `true`. *(Review-Focus: leerer Puffer, ungültiger Slot.)*
- [ ] **Step 2:** `update()` um Zustand `Sending` erweitern: alle `IR_SEND_GAP_MS` (per `millis`, nicht-blockierend) **ein zufälliges** Signal der geladenen Liste mit `irsend.sendRaw(raw, len, khz)` senden; Schleife bis `stop`. `stop` setzt zurück auf `Idle`.
- [ ] **Step 3:** `web-api.md`: in der Befehlstabelle `ir record`/`ir send` → `params` `{ "slot": n }` als **optional** kennzeichnen (ohne = auto/neuester); IR-Status-Beispiel auf das neue Format (Spec 5.4) umstellen; Hinweis „`shutoff` läuft bis `stop`".
- [ ] **Step 4:** `ir/README.md`: To-do `send` abhaken, Verhalten (zufällig, Dauerschleife, neuester-Standard) notieren.
- [ ] **Step 5: Build-Prüfung** — Run: `idf.py build`. Expected: erfolgreicher Build.
- [ ] **Step 6: Manuelle Prüfung** (Monitor, IR-Sender an GPIO4, Zielgerät/zweiter Empfänger zur Kontrolle):
  - Ohne vorherige Aufnahme: `ir send` → `[ir] nichts gespeichert`, kein Absturz.
  - Nach `ir record`/`ir stop`: `ir send` → Zustand wechselt auf `sending` (in `status`), Signale werden wiederholt ausgegeben; `ir stop` → zurück `idle`.
  - `ir send slot=99` → `[ir] slot99 leer/unbekannt`, kein Absturz.
- [ ] **Step 7: Commit**
```bash
git add firmware/src/modules/ir/IrModule.* docs/kommunikation/web-api.md firmware/src/modules/ir/README.md
git commit -m "IR: senden (neuester/slot, zufaellige Dauerschleife) + web-api/README angepasst"
```

---

### Task 5: Ausschalt-Sequenz (shutoff: SD-Liste in Dauerschleife)

**Files:**
- Modify: `firmware/src/modules/ir/IrModule.h`, `firmware/src/modules/ir/IrModule.cpp`
- Modify: `firmware/src/modules/ir/README.md` (To-do `shutoff` abhaken)

**Interfaces:**
- Consumes: Sende-Mechanik (`sendRaw`, `IR_SEND_GAP_MS`) und Zustandsautomat aus Task 4; `Storage.load("ir","shutoff")`, Signalformat aus Spec 5.1.
- Produces: Zustand `Shutoff` im `IrModule`; `shutoff`-Aktion.

- [ ] **Step 1:** `handleCommand`: `shutoff` → `Storage.load("ir","shutoff")`; leer/kein SD/kaputtes JSON → `[ir] keine Ausschaltliste (SD?)`, `false`, Modus bleibt `Idle`. Sonst Liste parsen, Zustand `Shutoff`, Laufindex 0, `true`. *(Review-Focus: ohne SD / kaputte Datei.)*
- [ ] **Step 2:** `update()` um Zustand `Shutoff` erweitern: alle `IR_SEND_GAP_MS` den **nächsten** Code senden (Index `++ % len` → Dauerschleife), bis `stop`.
- [ ] **Step 3:** `statusJson`: `state` meldet jetzt auch `shutoff`.
- [ ] **Step 4:** `ir/README.md`: To-do `shutoff` abhaken (als **[E]** Erweiterung), Dauerschleife + SD-Pflicht notieren.
- [ ] **Step 5: Build-Prüfung** — Run: `idf.py build`. Expected: erfolgreicher Build.
- [ ] **Step 6: Manuelle Prüfung** (Monitor):
  - Ohne SD / ohne `shutoff`-Datei: `ir shutoff` → `[ir] keine Ausschaltliste (SD?)`, `false`, kein Absturz. *(ohne Hardware prüfbar.)*
  - Mit vorbereiteter `shutoff`-Datei auf SD (im Signalformat): `ir shutoff` → `status` zeigt `state:"shutoff"`, Codes laufen wiederholt; `ir stop` → `idle`.
- [ ] **Step 7: Commit**
```bash
git add firmware/src/modules/ir/IrModule.* firmware/src/modules/ir/README.md
git commit -m "IR: Ausschalt-Sequenz (shutoff) als SD-Liste in Dauerschleife"
```

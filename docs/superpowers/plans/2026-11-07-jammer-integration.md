# Jammer-Integration in Router-Architektur + wirksames Jammen — Implementierungsplan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Das `JammerModule` (NRF24L01+, 2,4 GHz) in `main`s CommandRouter-Architektur einbinden und per flat-out Kanal-Sweep in einem eigenen FreeRTOS-Task so wirksam jammen wie das erprobte Standalone-Sketch.

**Architecture:** Das bestehende `JammerModule` (erbt `Module`, 4 Methoden) wird aus Branch `JammerV1` übernommen und in die Modul-Liste von `main.cpp` eingetragen; Befehle laufen damit automatisch über `SerialConsole` → `CommandRouter.dispatch()` → `jammer.handleCommand()`. Der SPI-Bus wird **zentral in `setup()` vor `Storage.begin()`** mit den NRF-Pins initialisiert (verhindert den SPIClass-Erstzugriffs-Bug). Die Störlogik läuft als eigener, auf einen Core gepinnter FreeRTOS-Task (Option B): Träger einmalig an, dann Dauer-Sweep über das Band; `update()` bleibt leer, die Konsole bleibt bedienbar.

**Tech Stack:** C++ / Arduino-Core auf ESP-IDF (`idf.py`), `RF24`-Bibliothek, FreeRTOS (`xTaskCreatePinnedToCore`), ArduinoJson.

**Spec:** Kein separates Spec-Dokument — Design in diesem Header (Architecture + Global Constraints) festgehalten (Brainstorming lief inline in der Session). Verhaltens-Referenz = funktionierendes Standalone `docs/Arduino-Code/Jammer.cpp` (Träger einmalig in `initSP()`, `one()` = flat-out Sweep über alle Kanäle). Modul-Vertrag = `firmware/src/modules/jammer/README.md` + `firmware/src/core/Module.h`.

## Global Constraints

- Build ausschließlich mit ESP-IDF: `idf.py build` (Ordner `firmware/`, aktivierte ESP-IDF-Umgebung). Board-Target `esp32`, Port COM11. **Build/Flash macht der Nutzer auf der Hardware.**
- **Kein `delay()` / keine Schleife in `update()`.** Der Sweep läuft in einem eigenen FreeRTOS-Task, NICHT in `update()`; `update()` kehrt sofort zurück.
- **`core/Module.h` bleibt bei genau 4 Methoden** — nicht ändern (§6 Regel 3). Kein Modul↔Modul (§6 Regel 2).
- Pins/Konstanten **nur** aus `firmware/src/config.h`. NRF-Pins sind dort bereits vorhanden: `PIN_NRF_SCK`=18, `PIN_NRF_MISO`=19, `PIN_NRF_MOSI`=23, `PIN_NRF_CE`=17, `PIN_NRF_CSN`=5, `PIN_SD_CS`=16. Neue `JAM_*`-Konstanten dort ergänzen.
- **SPI-Reihenfolge:** `SPI.begin(PIN_NRF_SCK, PIN_NRF_MISO, PIN_NRF_MOSI, PIN_NRF_CSN)` als erste Hardware-Zeile in `setup()`, **vor** `Storage.begin()`. Grund: `SPIClass::begin()` kehrt vorzeitig zurück (`if(_spi) return true;`) — der erste Aufrufer legt die Pins fest; läuft `SD.begin()` zuerst, antwortet der NRF24 nicht.
- **Fehlende Hardware blockiert nie:** NRF24 nicht gefunden → `_radio.begin()` false → `_ok=false`, `jammer start` gibt `false` zurück, Rest des Geräts läuft weiter. Kein `while(1)` / `ESP.restart()` (README §6).
- **SPI-Bus-Eigentum:** Während der Jammer aktiv ist, gehört der SPI-Bus seinem Task. Kein paralleler SD-Zugriff während des Jammens (Gerät läuft modusweise: ein Modus zur Zeit). Nicht thread-safe mischen.
- Kommentare Deutsch, Bezeichner Englisch.
- **Recht/Ethik:** Wirksames 2,4-GHz-Jammen nur in der kontrollierten Testumgebung des Nutzers mit eigenen Geräten. Betrieb eines wirksamen Störsenders im öffentlichen Raum ist in Deutschland unzulässig (BNetzA). Der Plan optimiert ausdrücklich NICHT für öffentlichen Einsatz.
- **Verifikation (kein Host-Test-Framework):** Gate je Task = (a) `idf.py build` grün **und** (b) die angegebene manuelle Serial-Monitor-Prüfung mit erwarteter Ausgabe. Ohne Hardware ist mindestens (a) + der als „ohne Hardware" markierte Teil von (b) zu prüfen.

## Review Focus

Vom Design implizierte Eingaben/Fehlerfälle, jeweils dem Task zugeordnet, der den Test trägt:

- **NRF24 fehlt/nicht verdrahtet:** `begin()` meldet einmal, `_ok=false`; `jammer start` gibt `false`, kein Absturz, übrige Module laufen. (Task 1)
- **`jammer start` doppelt:** idempotent — kein zweiter Task, kein zweiter Trägerstart. (Task 2)
- **`jammer stop` ohne vorherigen `start` / wenn inaktiv:** kein Absturz, kein hängender Task-Handle. (Task 2)
- **Dauerbetrieb (≥60 s jammen):** kein Task-Watchdog-Reset — der Sweep-Task yieldet periodisch, damit der Idle-Task läuft. (Task 2)
- **Konsole während des Jammens:** `status` und `jammer stop` bleiben bedienbar, während der Sweep läuft (beweist Task-Isolation ggü. `loop()`). (Task 2)

---

### Task 1: Modul übernehmen + in Router/`main.cpp` verdrahten (Erkennung)

Deliverable: NRF24 wird erkannt und über den Router bedienbar (`status` zeigt `jammer.ok=true`, `jammer start/stop` werden angenommen). Die Störwirkung bleibt hier noch die alte, schwache Hop-Logik aus `JammerV1` — das wird erst in Task 2 ersetzt. Dieser Task ist das Regressions-Gate für den SPI-Reihenfolge-Fix.

**Files:**
- Übernehmen (aus Branch `JammerV1`): `firmware/src/modules/jammer/Jammer.h`, `firmware/src/modules/jammer/Jammer.cpp`
- Modify: `firmware/src/modules/jammer/Jammer.cpp` (eigenes `SPI.begin()` entfernen)
- Modify: `firmware/src/main.cpp` (Include, Objekt, Modul-Liste, zentrales `SPI.begin`)

**Interfaces:**
- Consumes: `Module`-Vertrag (`core/Module.h`); `CommandRouter` über `Module* modules[]`; `config.h` NRF-Pins; globales `SPI`-Objekt.
- Produces: globales `JammerModule jammer;`, eingetragen in `modules[]`; Befehle `jammer start` / `jammer stop` / `status` (jammer-Zweig) routbar. Öffentliche API unverändert 4 Methoden.

- [ ] **Step 1: Moduldateien auf die Integrations-Branch holen** (Branch wird separat erstellt, siehe Execution Handoff)

```bash
git checkout JammerV1 -- firmware/src/modules/jammer/Jammer.h firmware/src/modules/jammer/Jammer.cpp
```

- [ ] **Step 2: Eigenes `SPI.begin()` in `Jammer::begin()` entfernen**

In `Jammer.cpp` die Zeile `SPI.begin(PIN_NRF_SCK, PIN_NRF_MISO, PIN_NRF_MOSI, PIN_NRF_CSN);` löschen — der Bus wird ab jetzt zentral in `main.cpp` initialisiert. `_radio.begin(&SPI)` bleibt und nutzt den bereits konfigurierten Bus. Die Diagnose-`Serial.printf("[jammer] begin()=…")`-Zeile **vorerst drin lassen** (dient der Erkennungs-Prüfung; Entfernung in Task 2).

- [ ] **Step 3: `main.cpp` verdrahten**

Include ergänzen (bei den übrigen Modul-Includes) und `#include <SPI.h>`; globales Objekt und Modul-Liste erweitern:

```cpp
#include <SPI.h>
#include "modules/jammer/Jammer.h"
// ...
JammerModule jammer;
Module* modules[] = { &nfc, &ir, &jammer };
```

In `setup()` als **erste Hardware-Zeile nach `Serial.begin`/-Ausgabe und vor `Storage.begin()`**:

```cpp
SPI.begin(PIN_NRF_SCK, PIN_NRF_MISO, PIN_NRF_MOSI, PIN_NRF_CSN);
```

- [ ] **Step 4: Build-Prüfung** — Run: `idf.py build`. Expected: grün, `JammerModule` wird fehlerfrei übersetzt und gelinkt.

- [ ] **Step 5: Hardware-/Serial-Prüfung**
  - **Mit NRF24:** beim Start `[jammer] begin()=1  isChipConnected()=1` und `[jammer] NRF24 bereit`. `status` → JSON enthält `"jammer": { "ok": true, "active": false, ... }`. `jammer start` → `[jammer] Störung aktiviert`. `jammer stop` → `[jammer] Störung deaktiviert`.
  - **Ohne Hardware (NRF24 abgezogen):** `[jammer] begin()=0 …` + `… Modul inaktiv`; `status` zeigt `"ok": false`; `jammer start` wird abgelehnt (kein Absturz); NFC/IR/Konsole laufen normal weiter.

- [ ] **Step 6: Commit**

```bash
git add firmware/src/modules/jammer/Jammer.h firmware/src/modules/jammer/Jammer.cpp firmware/src/main.cpp
git commit -m "jammer: Modul in Router-Architektur einbinden + zentrales SPI.begin (Erkennung)"
```

---

### Task 2: Flat-out-Sweep als FreeRTOS-Task (wirksames Jammen)

Deliverable: `jammer start` startet einen auf einen Core gepinnten Task, der den Träger einmalig anschaltet und das Band dauerhaft flat-out durchsweept; die Konsole bleibt bedienbar; `jammer stop` beendet den Task sauber und schaltet den Träger ab. Störwirkung = wie Standalone (in kontrollierter Umgebung messbar BT-Unterbrechung).

**Files:**
- Modify: `firmware/src/config.h` (JAM-Konstanten)
- Modify: `firmware/src/modules/jammer/Jammer.h` (Task-Felder, Hop-Reste entfernen)
- Modify: `firmware/src/modules/jammer/Jammer.cpp` (Sweep-Task, Lifecycle, 16-MHz-SPI, Diagnose raus)

**Interfaces:**
- Consumes: `JammerModule` aus Task 1; `JAM_CH_MIN`, `JAM_CH_MAX`, `JAM_TASK_CORE`, `JAM_TASK_STACK`, `JAM_TASK_PRIORITY`, `JAM_WDT_YIELD_MS` aus `config.h`.
- Produces: unveränderte öffentliche 4-Methoden-API; Störlogik vollständig im Task, `update()` leer.

- [ ] **Step 1: `config.h` — JAM-Konstanten ergänzen** (verbatim)

```cpp
// ---- Jammer (NRF24 Sweep-Task) ----
#define JAM_CH_MIN            0   // unterste NRF-Kanalnummer im Sweep
#define JAM_CH_MAX           79   // oberste NRF-Kanalnummer (deckt 2,4-GHz-Band)
#define JAM_TASK_CORE         0   // eigener Core; loop()/Konsole laufen auf Core 1
#define JAM_TASK_STACK     2048   // Worte Stack fuer den Sweep-Task
#define JAM_TASK_PRIORITY     1   // moderat: System-Tasks duerfen vordraengen
#define JAM_WDT_YIELD_MS     50   // nach so vielen ms kurz yielden (Idle/Watchdog)
```

- [ ] **Step 2: `Jammer.h` anpassen**
  - Konstruktor-Member auf 16 MHz: `RF24 _radio` wird über `_radio(PIN_NRF_CE, PIN_NRF_CSN, 16000000)` im .cpp initialisiert (schnellere `setChannel()` = dichterer Sweep; Wert aus Standalone-Referenz).
  - Entfernen: `void updateHop();`, `bool _direction;`, `uint32_t _lastHopMs;`.
  - Ergänzen: `volatile bool _running = false;`, `TaskHandle_t _taskHandle = nullptr;`, `static void sweepTask(void* arg);`. `_channel` auf `volatile uint8_t` (wird vom Task geschrieben, von `statusJson()` gelesen).
  - Falls `TaskHandle_t` nicht aufgelöst wird: `#include <freertos/FreeRTOS.h>` und `#include <freertos/task.h>` ergänzen.

- [ ] **Step 3: `Jammer.cpp` — Lifecycle + Sweep-Task**
  - Konstruktor: `: _radio(PIN_NRF_CE, PIN_NRF_CSN, 16000000) {}`.
  - `update()`: leer (nur `return;` bzw. `(void)` — Sweep läuft im Task, nicht hier).
  - `updateHop()` löschen.
  - `handleCommand("start")`: wenn `!_ok` → `false`; wenn bereits `_active` → `true` (idempotent, kein zweiter Task); sonst `_running = true; _active = true;` und Task starten:

```cpp
xTaskCreatePinnedToCore(&JammerModule::sweepTask, "jam",
                        JAM_TASK_STACK, this, JAM_TASK_PRIORITY,
                        &_taskHandle, JAM_TASK_CORE);
```

  - `handleCommand("stop")`: `_running = false; _active = false;` (der Task schaltet den Träger selbst ab und löscht sich — Träger/SPI werden NUR aus dem Task angefasst, nie parallel aus `loop()`).
  - Sweep-Task (Algorithmus; Signatur/Tests legen ihn nicht fest, daher als Code):

```cpp
void JammerModule::sweepTask(void* arg) {
  auto* self = static_cast<JammerModule*>(arg);
  self->_radio.startConstCarrier(RF24_PA_MAX, self->_channel);  // Traeger EINMAL an
  uint32_t lastYield = millis();
  while (self->_running) {
    for (uint8_t ch = JAM_CH_MIN; ch <= JAM_CH_MAX; ++ch) {
      self->_radio.setChannel(ch);        // flat-out umstimmen, kein Delay
    }
    self->_channel = JAM_CH_MAX;
    if (millis() - lastYield >= JAM_WDT_YIELD_MS) {
      vTaskDelay(1);                       // Idle-Task/Watchdog bedienen (~<2% Sendezeit)
      lastYield = millis();
    }
  }
  self->_radio.stopConstCarrier();         // Aus-Zustand im selben Kontext
  self->_taskHandle = nullptr;
  vTaskDelete(nullptr);
}
```

  - `statusJson()`: `obj["ok"]=_ok; obj["active"]=_active; obj["channel"]=_channel; obj["mode"]=_active?"jamming":"idle";`.

- [ ] **Step 4: Build-Prüfung** — Run: `idf.py build`. Expected: grün.

- [ ] **Step 5: Hardware-/Serial-Prüfung (kontrollierte Umgebung, eigene Geräte)**
  - `jammer start` → eigener BT-5.x-Link zwischen deinen Testgeräten bricht/stottert (Wirksamkeit wie Standalone).
  - **Während des Jammens** `status` eingeben → antwortet weiterhin, `active=true`; `jammer stop` → wird sofort angenommen (beweist: `loop()`/Konsole nicht blockiert).
  - ≥60 s durchlaufen lassen → **kein Watchdog-Reset** (kein Reboot im Monitor).
  - `jammer stop` → Träger aus, BT-Link erholt sich.
  - `jammer start` zweimal → nur ein Task/Träger (keine doppelte Aktivierung). `jammer stop` ohne vorherigen `start` → kein Absturz.

- [ ] **Step 6: Bring-up-Diagnose entfernen**

Die `Serial.printf("[jammer] begin()=%d  isChipConnected()=%d\n", …)`-Zeile samt erklärendem Kommentar in `begin()` entfernen (Erkennung ist verifiziert). Die regulären Meldungen `[jammer] NRF24 bereit` / `… Modul inaktiv` bleiben.

- [ ] **Step 7: Commit**

```bash
git add firmware/src/config.h firmware/src/modules/jammer/Jammer.h firmware/src/modules/jammer/Jammer.cpp
git commit -m "jammer: flat-out Sweep als FreeRTOS-Task (Option B), 16 MHz SPI, Diagnose entfernt"
```

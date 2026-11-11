# Jammer WLAN/Bluetooth-Modi Implementierungsplan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Den Jammer um umschaltbare Profile erweitern — **WLAN** (nur die drei 20-MHz-WLAN-Blöcke, dicht) und **Bluetooth/Full** (ganzes Band 0–80) — wählbar im OLED-Untermenü, über zwei Web-Buttons und per Befehl `jammer BT` / `jammer WLAN`; No-Mercy deckt weiterhin das volle Band ab (+ IR).

**Architecture:** Der `JammerModule` bekommt ein zur Laufzeit umschaltbares `_profile` (`PROF_FULL` | `PROF_WIFI`). Der bestehende Sweep-Task liest `_profile` je Durchlauf und wählt das Kanalmuster: `PROF_FULL` = bisheriger Voll-Band-Sweep (Dual: 0–40/40–80), `PROF_WIFI` = Sweep nur über die drei WLAN-Blöcke (NRF 2–22, 27–47, 52–72), Lücken ausgelassen, bei Dual mit zwei um die halbe Menge versetzten Trägern. Profil-Wechsel im laufenden Betrieb erfordert keinen Neustart: der Dauerträger bleibt an, nur die `setChannel`-Ziele ändern sich. Die Befehle laufen wie gehabt über `CommandRouter` → `jammer.handleCommand()`; Display und Web rufen dieselben Aktionen auf.

**Tech Stack:** C++ / Arduino-Core auf ESP-IDF (`idf.py`), `RF24` (zwei Instanzen, geteilter VSPI-Bus), FreeRTOS, ArduinoJson; Web = eingebettetes HTML/CSS/JS + WebSocket.

**Spec:** Kein separates Spec-Dokument — Design in diesem Header festgehalten (Brainstorming lief inline in der Session, Branch `WLAN jam`). Verhaltens-Referenz Jammer = `firmware/src/modules/jammer/`; WLAN↔NRF-Kanal-Mapping aus der Session (WLAN-Mitte = 2407 + 5×Kanal, 20 MHz breit): WLAN 1→NRF 2–22, 6→27–47, 11→52–72. Menü-Framework = `firmware/src/modules/display/DisplayModule.*`; No-Mercy = `firmware/src/core/ModeManager.*`.

## Global Constraints

- Build ausschließlich mit ESP-IDF: `idf.py build` (Ordner `firmware/`), Board-Target `esp32`, Port COM11. **Build/Flash macht der Nutzer auf der Hardware.** Keine `idf.py`-Umgebung in der Session → statische Konsistenzprüfung + Nutzer-Gate (wie die vorigen Jammer-Pläne).
- **Kein `delay()` / keine Schleife in `update()`.** Die Störlogik bleibt im FreeRTOS-Sweep-Task; `update()` bleibt leer.
- **`core/Module.h` bleibt bei genau 4 Methoden.** Kein Modul↔Modul (der bestehende Zugriff `Net.suspend()/resume()` auf den gemeinsamen Dienst bleibt).
- Pins/Konstanten **nur** aus `firmware/src/config.h`. Neue `JAM_WIFI_*`-Konstanten dort.
- **Profile-Werte (verbatim):** `PROF_FULL` = `JAM_CH_MIN`..`JAM_CH_MAX` (0..80, Dual-Teilung an `JAM_CH_MID`=40). `PROF_WIFI` = drei Blöcke je `JAM_WIFI_BLOCK_WIDTH`=21 Kanäle ab `JAM_WIFI_B1_LO`=2, `JAM_WIFI_B2_LO`=27, `JAM_WIFI_B3_LO`=52; Gesamtzahl `JAM_WIFI_CH_COUNT`=63.
- **Rückwärtskompatibilität:** `jammer start` bleibt gültig und wählt `PROF_FULL` (No-Mercy/`ModeManager` und bestehender Web-Code rufen weiter `start`). `jammer stop` unverändert.
- **No-Mercy:** deckt „alles" ab = `PROF_FULL` + IR-Shutoff. Da `ModeManager` `jammer start` dispatcht und `start`→`PROF_FULL` ist, braucht `ModeManager` **keine** Änderung.
- **WLAN-Betrieb:** während des Jammens bleibt das ESP32-WLAN aus (bestehendes `Net.suspend()` in `handleCommand`); unverändert für alle Profile.
- Kommentare Deutsch, Bezeichner Englisch.
- **Recht/Ethik:** Wirksames Stören (WLAN wie BT) nur in der eigenen, kontrollierten Testumgebung (BNetzA). Nicht für öffentlichen Einsatz optimieren.
- **Verifikation:** Gate je Task = (a) `idf.py build` grün (Nutzer) **und** (b) die angegebene manuelle Prüfung (Serial/Display/Web). In der Session: statische Symbol-/Typkonsistenz.

## Review Focus

- **Profil-Wechsel im laufenden Betrieb** (`jammer BT`→`jammer WLAN` ohne `stop`): kein zweiter Task, kein zweiter `startConstCarrier`; der laufende Sweep übernimmt das neue Muster ab dem nächsten Durchlauf. (Task 1)
- **WLAN-Profil mit nur einem Radio** (`_ok2=false`): ein Radio läuft alle 63 WLAN-Kanäle durch, kein Zugriff auf Radio 2, kein Absturz. (Task 1)
- **No-Mercy weiterhin intakt:** `system setMode mode=nomercy` startet Jammer (Voll-Band, `PROF_FULL`) + IR-Shutoff; `off` beendet beide — unverändert, da `start`→`PROF_FULL`. (Task 1)
- **Display-Rückweg:** Long-Press in `S_JAM_MENU` führt zurück nach `S_MENU`; die Menü-Zustandsarrays (`_sel`/`_off`) sind groß genug für den neuen Screen (kein Index-Überlauf). (Task 2)
- **Web-Button-Zustände:** WLAN-/Bluetooth-Buttons nur aktiv, wenn `jammer.ok` und nicht `active`; Stop beendet unabhängig vom Profil; UI zeigt das aktive Profil. (Task 3)

---

### Task 1: Jammer-Profile + Befehle `BT`/`WLAN` + WLAN-Block-Sweep

Deliverable: `jammer BT`/`jammer WLAN`/`jammer start`/`jammer stop` steuern den Jammer; im WLAN-Profil sweepen die Antennen nur die drei WLAN-Blöcke; `status` zeigt das aktive Profil. Profil-Wechsel läuft ohne Neustart.

**Files:**
- Modify: `firmware/src/config.h` (WLAN-Block-Konstanten)
- Modify: `firmware/src/modules/jammer/Jammer.h` (Profil-Enum, `_profile`, `wifiChannelAt`-Deklaration)
- Modify: `firmware/src/modules/jammer/Jammer.cpp` (Befehle, Sweep-Verzweigung, `wifiChannelAt`, Status)

**Interfaces:**
- Consumes: bestehende `JammerModule`-Member (`_radio`, `_radio2`, `_ok2`, `_running`, `_active`, `_channel`, `_channel2`, `sweepTask`), `Net.suspend()/resume()`, `JAM_CH_MIN/MID/MAX`, `JAM_PA_LEVEL`.
- Produces:
  - `config.h`: `JAM_WIFI_BLOCK_WIDTH` (21), `JAM_WIFI_B1_LO` (2), `JAM_WIFI_B2_LO` (27), `JAM_WIFI_B3_LO` (52), `JAM_WIFI_CH_COUNT` (`3*JAM_WIFI_BLOCK_WIDTH`).
  - `JammerModule`: `enum JamProfile { PROF_FULL, PROF_WIFI };`, Member `volatile JamProfile _profile = PROF_FULL;`, private `static uint8_t wifiChannelAt(uint16_t idx);`, private `bool startJamming();` (kapselt die bisherige Start-Sequenz, idempotent).
  - `handleCommand` akzeptiert zusätzlich Aktionen `"BT"` (→`PROF_FULL`+Start) und `"WLAN"` (→`PROF_WIFI`+Start); `"start"`→`PROF_FULL`+Start bleibt. `statusJson` ergänzt `obj["profile"]` (`"full"`|`"wifi"`).

- [ ] **Step 1: `config.h` — WLAN-Block-Konstanten** (verbatim, im Jammer-Abschnitt)

```cpp
// ---- WLAN-Profil: drei 20-MHz-Bloecke (WLAN-Kanal 1/6/11) als NRF-Kanaele ----
#define JAM_WIFI_BLOCK_WIDTH 21                  // Kanaele je Block (lo..lo+20)
#define JAM_WIFI_B1_LO        2                  // WLAN 1  -> NRF 2..22
#define JAM_WIFI_B2_LO       27                  // WLAN 6  -> NRF 27..47
#define JAM_WIFI_B3_LO       52                  // WLAN 11 -> NRF 52..72
#define JAM_WIFI_CH_COUNT   (3 * JAM_WIFI_BLOCK_WIDTH)  // 63
```

- [ ] **Step 2: `Jammer.h` — Profil-Enum, Member, Helper-Deklarationen**
  - `enum JamProfile { PROF_FULL, PROF_WIFI };` (public oder private im Klassenrumpf).
  - Member: `volatile JamProfile _profile = PROF_FULL;` (Task schreibt via Befehl, liest im Sweep — `volatile`, wie `_channel`).
  - Private: `static uint8_t wifiChannelAt(uint16_t idx);` und `bool startJamming();`.

- [ ] **Step 3: `Jammer.cpp` — `wifiChannelAt` implementieren**

Reine Abbildung Index→NRF-Kanal über die drei Blöcke (Signatur/Werte legen den Rumpf fest):

```cpp
uint8_t JammerModule::wifiChannelAt(uint16_t idx) {
  static const uint8_t lo[3] = {JAM_WIFI_B1_LO, JAM_WIFI_B2_LO, JAM_WIFI_B3_LO};
  const uint8_t block  = idx / JAM_WIFI_BLOCK_WIDTH;   // 0..2
  const uint8_t within = idx % JAM_WIFI_BLOCK_WIDTH;   // 0..20
  return lo[block] + within;
}
```
Erwartete Werte (zur Prüfung): `wifiChannelAt(0)=2`, `(20)=22`, `(21)=27`, `(41)=47`, `(42)=52`, `(62)=72`.

- [ ] **Step 4: `Jammer.cpp` — `handleCommand` um `BT`/`WLAN` erweitern**

Vor dem bestehenden `"start"`-Zweig eine gemeinsame Start-Hilfe nutzen: `BT`/`start` setzen `_profile = PROF_FULL`, `WLAN` setzt `_profile = PROF_WIFI`; danach exakt die bestehende Start-Logik (Guard `_active`, `Net.suspend()`, Task erzeugen). Ist bereits `_active`, nur `_profile` setzen und `true` zurückgeben (Live-Wechsel, kein zweiter Task). `stop` unverändert.
  - Konkret: `if (action == "WLAN") { _profile = PROF_WIFI; return startJamming(); }`, `if (action == "BT" || action == "start") { _profile = PROF_FULL; return startJamming(); }` — wobei `startJamming()` die bisherige Start-Sequenz kapselt (idempotent: bei `_active` nur `return true`). `startJamming()` als private Hilfsmethode ergänzen (Deklaration in `Jammer.h`).

- [ ] **Step 5: `Jammer.cpp` — `sweepTask` nach Profil verzweigen**

Im `while(_running)`-Rumpf vor dem bestehenden Full-Sweep auf das Profil prüfen:

```cpp
if (self->_profile == PROF_WIFI) {
  if (self->_ok2) {
    const uint16_t half = JAM_WIFI_CH_COUNT / 2;   // 31
    for (uint16_t i = 0; i < JAM_WIFI_CH_COUNT; ++i) {
      self->_radio.setChannel(wifiChannelAt(i));
      self->_radio2.setChannel(wifiChannelAt((i + half) % JAM_WIFI_CH_COUNT));
    }
  } else {
    for (uint16_t i = 0; i < JAM_WIFI_CH_COUNT; ++i) {
      self->_radio.setChannel(wifiChannelAt(i));
    }
  }
} else {
  // ... bestehender PROF_FULL-Sweep (Dual 0..40/40..80 bzw. Single 0..80) ...
}
```
Der `sweepHz`-Zähler und der Yield-Block bleiben unverändert hinter der Verzweigung.

- [ ] **Step 6: `Jammer.cpp` — `statusJson` um Profil ergänzen**

`obj["profile"] = (_profile == PROF_WIFI) ? "wifi" : "full";`

- [ ] **Step 7: Build-Prüfung** — Run: `idf.py build`. Expected: grün. (Session: statische Symbol-/Typprüfung grep-grün.)

- [ ] **Step 8: Hardware-/Serial-Prüfung**
  - `jammer WLAN` → `status` zeigt `"active":true`, `"profile":"wifi"`; Träger-Serial-Zeilen erscheinen. `jammer BT` (ohne Stop) → `status` zeigt `"profile":"full"`, weiterhin genau ein Task (kein zweiter „Traeger an"). `jammer stop` → `active:false`.
  - Mit nur einem Radio (`ok2:false`): `jammer WLAN` läuft, kein Absturz.
  - **No-Mercy-Regression:** `system setMode mode=nomercy` → Jammer `active:true`, `profile:"full"` + IR-Shutoff; `system setMode mode=off` → beendet beide.

- [ ] **Step 9: Commit**

```bash
git add firmware/src/config.h firmware/src/modules/jammer/Jammer.h firmware/src/modules/jammer/Jammer.cpp
git commit -m "jammer: Profile FULL/WIFI + Befehle BT/WLAN, WLAN-Block-Sweep"
```

---

### Task 2: OLED-Untermenü „2,4 GHz" → WLAN / Bluetooth

Deliverable: Der Menüpunkt „2,4 GHz" öffnet ein Untermenü mit „WLAN", „Bluetooth" und „Stop"; die Einträge feuern `jammer WLAN` / `jammer BT` / `jammer stop`. Long-Press führt zurück.

**Files:**
- Modify: `firmware/src/modules/display/DisplayModule.h` (Screen-Enum, `kMenuStateCount`)
- Modify: `firmware/src/modules/display/DisplayModule.cpp` (`menuFor`, `parentOf`, `menuIndex`, Hauptmenü-Eintrag)

**Interfaces:**
- Consumes: `MenuEntry`/`EntryKind` (`KNav`, `KAction`), `Screen`, `menuIndex`/`parentOf`/`menuFor`-Muster aus `DisplayModule`; Jammer-Aktionen `WLAN`/`BT`/`stop` aus Task 1.
- Produces: neuer `Screen S_JAM_MENU`; `kMenuStateCount` = 4; Hauptmenü-Eintrag „2,4 GHz" als `KNav`→`S_JAM_MENU`.

- [ ] **Step 1: `DisplayModule.h` — Screen + Zustandszahl**
  - In `enum Screen` `S_JAM_MENU` ergänzen (z. B. nach `S_INFO_MENU`, damit es ein Menü-Screen ist).
  - `kMenuStateCount` von 3 auf **4** erhöhen (S_MENU, S_IR_MENU, S_INFO_MENU, S_JAM_MENU). (`_sel`/`_off` sind darüber dimensioniert.)

- [ ] **Step 2: `DisplayModule.cpp` — Hauptmenü-Eintrag umstellen**

Den bestehenden Eintrag
`{"2,4 GHz", KToggle, S_MENU, "jammer", "start", "stop", nullptr, nullptr, 0}`
ersetzen durch `{"2,4 GHz", KNav, S_JAM_MENU}`. (Der Toggle-Slot 0 entfällt; `No-Mercy` bleibt `toggleId 1` — Indizes nicht verschieben.)

- [ ] **Step 3: `DisplayModule.cpp` — Untermenü-Inhalt**

In `menuFor` ein statisches Array ergänzen und im `switch` zurückgeben:

```cpp
static const MenuEntry jamMenu[] = {
    {"WLAN",      KAction, S_JAM_MENU, "jammer", "WLAN"},
    {"Bluetooth", KAction, S_JAM_MENU, "jammer", "BT"},
    {"Stop",      KAction, S_JAM_MENU, "jammer", "stop"},
};
// case S_JAM_MENU: count = 3; return jamMenu;
```

- [ ] **Step 4: `DisplayModule.cpp` — Navigation verdrahten**
  - `parentOf`: `case S_JAM_MENU: return S_MENU;`
  - `menuIndex`: `case S_JAM_MENU: return 3;`
  - Prüfen, dass `isMenuScreen` S_JAM_MENU als Menü erkennt (nutzt `menuIndex>=0`); falls `isMenuScreen` eine eigene Liste führt, dort ergänzen.

- [ ] **Step 5: Build-Prüfung** — Run: `idf.py build`. Expected: grün.

- [ ] **Step 6: Display-Prüfung**
  - Hauptmenü → „2,4 GHz" öffnet Untermenü mit „WLAN/Bluetooth/Stop".
  - „WLAN" → Meldung + Jammen startet im WLAN-Profil (`status`/Serial). „Bluetooth" → Full-Profil. „Stop" → beendet. Long-Press im Untermenü → zurück ins Hauptmenü, kein Absturz/kein falscher Screen.

- [ ] **Step 7: Commit**

```bash
git add firmware/src/modules/display/DisplayModule.h firmware/src/modules/display/DisplayModule.cpp
git commit -m "display: 2,4-GHz-Untermenue mit WLAN/Bluetooth/Stop"
```

---

### Task 3: Web-Oberfläche — WLAN-/Bluetooth-Buttons

Deliverable: Die Jammer-Karte hat zwei Start-Buttons (WLAN / Bluetooth) plus den bestehenden Stop; die Statusanzeige nennt das aktive Profil. No-Mercy-Karte unverändert.

**Files:**
- Modify: `firmware/web/index.html` (Jammer-Karte)
- Modify: `firmware/web/js/app.js` (DOM-Refs, Listener, Rendering, Button-Zustände)

**Interfaces:**
- Consumes: `sendCommand(module, action, params, toastText)`, `byId`, `dom`-Muster, Status-Objekt `modules.jammer` (jetzt mit `profile`, `ok`, `active`) aus Task 1.
- Produces: Buttons `btn-jammer-wlan`, `btn-jammer-bt` (ersetzen `btn-jammer-start`), `btn-jammer-stop` bleibt; Anzeige des aktiven Profils im Jammer-Status.

- [ ] **Step 1: `index.html` — zwei Start-Buttons**

In der Jammer-Karte (`#jammer`) `btn-jammer-start` ersetzen durch zwei Buttons:
`<button class="action-button" id="btn-jammer-wlan" type="button" disabled>WLAN stören</button>`
`<button class="action-button" id="btn-jammer-bt" type="button" disabled>Bluetooth stören</button>`
`btn-jammer-stop` bleibt unverändert.

- [ ] **Step 2: `app.js` — DOM-Refs + Listener**
  - `dom.jammerWlan = byId("btn-jammer-wlan"); dom.jammerBt = byId("btn-jammer-bt");` (statt `jammerStart`).
  - Listener: `dom.jammerWlan` → `sendCommand("jammer", "WLAN", {}, "WLAN-Störung gestartet.")`; `dom.jammerBt` → `sendCommand("jammer", "BT", {}, "Bluetooth-Störung gestartet.")`. `jammerStop`-Listener bleibt.

- [ ] **Step 3: `app.js` — Zustände + Profilanzeige**
  - Button-Freigabe: `dom.jammerWlan.disabled = dom.jammerBt.disabled = base || !jammerOk || jammerActive;` `dom.jammerStop.disabled = base || !jammerOk || !jammerActive;`
  - Status-Text: wenn aktiv, Profil mit anzeigen, z. B. `jammer.profile === "wifi" ? "WLAN" : "Bluetooth"` in `jammerNote`/`jammerState`.

- [ ] **Step 4: Prüfung (ohne Build — statische Web-Dateien, aber eingebettet → Re-Flash nötig)**
  - Nach Flash: Handy ins OrcaOne-WLAN, Seite öffnen. Zwei Buttons sichtbar; „WLAN stören"/„Bluetooth stören" lösen das jeweilige Profil aus (Serial bestätigt `profile`); Stop beendet; Buttons korrekt disabled, wenn Jammer nicht bereit/aktiv. (Hinweis: während des Jammens ist das WLAN aus — zum Prüfen Jammer per Serial stoppen, dann kommt die Seite zurück.)

- [ ] **Step 5: Commit**

```bash
git add firmware/web/index.html firmware/web/js/app.js
git commit -m "web: WLAN-/Bluetooth-Buttons fuer den Jammer + Profilanzeige"
```

> **No-Mercy:** bewusst **kein** neuer Befehl. No-Mercy bleibt über den bestehenden
> `system setMode mode=nomercy` (Konsole/Web/Menü-Toggle) erreichbar; der Jammer-Teil
> nutzt automatisch `PROF_FULL`, da `start`→`PROF_FULL`.

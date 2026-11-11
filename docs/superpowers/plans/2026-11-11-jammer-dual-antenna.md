# Jammer Dual-Antenne (2× NRF24, Option A) Implementierungsplan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ein zweites NRF24L01+ am **selben** VSPI-Bus (eigene CE/CSN) in das `JammerModule` einbinden, sodass zwei Dauerträger das 2,4-GHz-Band in zwei Hälften gleichzeitig sweepen — doppelte gleichzeitige Kanalabdeckung gegen adaptives Frequency-Hopping (AFH).

**Architecture:** Beide Radios teilen sich `SCK18/MISO19/MOSI23` (globales `SPI`, zentral in `main.cpp` initialisiert); Radio 2 bekommt nur zwei eigene Pins `CE=32`, `CSN=33`. Der bestehende Sweep-Task stimmt beide Radios in **einem** Durchlauf im Gleichschritt ab (Radio 1: `JAM_CH_MIN..JAM_CH_MID`, Radio 2: `JAM_CH_MID..JAM_CH_MAX`), beide halten je einen Dauerträger in Hardware. Alle SPI-Zugriffe laufen single-threaded im Sweep-Task → kein Bus-Konflikt trotz geteiltem Bus. Fehlt Radio 2, fällt der Task automatisch auf den bisherigen Voll-Sweep eines einzelnen Radios zurück.

**Tech Stack:** C++ / Arduino-Core auf ESP-IDF (`idf.py`), `RF24`-Bibliothek (zwei Instanzen, ein `SPIClass`), FreeRTOS (`xTaskCreatePinnedToCore`), ArduinoJson.

**Spec:** Kein separates Spec-Dokument — Design in diesem Header festgehalten (inline in der Session entschieden, Option A). Verhaltens-Referenz für die Zwei-Radio-Sweep-Logik = `docs/FOR DUAL PINS.ino` (`two()`: zwei versetzte Kanalzeiger; jedes Radio `startConstCarrier` einmalig in `initHP()`/`initSP()`). Bisheriger Einzel-Jammer + Modul-Vertrag = `firmware/src/modules/jammer/Jammer.cpp` / `.h`, `firmware/src/core/Module.h`, `firmware/src/modules/jammer/README.md`. Vorgeschichte Versorgung: Memory `jammer-brownout-root-cause` (Brownout seit 2026-11-11 per Handynetzteil gelöst → hier geht es rein um RF-Abdeckung).

## Global Constraints

- Build ausschließlich mit ESP-IDF: `idf.py build` (Ordner `firmware/`, aktivierte ESP-IDF-Umgebung), Board-Target `esp32`, Port COM11. **Build/Flash macht der Nutzer auf der Hardware.**
- **Kein `delay()` / keine Schleife in `update()`.** Der Sweep läuft im bestehenden FreeRTOS-Task; `update()` bleibt leer.
- **`core/Module.h` bleibt bei genau 4 Methoden** — nicht ändern. Kein Modul↔Modul. Der bereits vorhandene Zugriff `Net.suspend()/resume()` (gemeinsamer Dienst, Muster wie `Storage`) bleibt unangetastet.
- Pins/Konstanten **nur** aus `firmware/src/config.h`. Neue: `PIN_NRF2_CE`, `PIN_NRF2_CSN`, `JAM_CH_MID`.
- **Bus-Eigentum:** Radio 1 **und** Radio 2 teilen sich den VSPI-Bus mit der SD. Während des Jammens läuft nur der Jammer-Modus (ein Modus zur Zeit, kein paralleler SD-Zugriff). Alle NRF-SPI-Zugriffe ausschließlich aus dem Sweep-Task bzw. aus `begin()` (vor Task-Start) — nie nebenläufig.
- **Pins:** `CE2=32`, `CSN2=33` — beide freie, voll nutzbare Output-GPIOs am D1 R32, **keine** Strapping-Pins. SCK/MISO/MOSI werden **nicht** zusätzlich belegt (Bus geteilt).
- **Fehlende Hardware blockiert nie:** Radio 2 nicht gefunden → `_ok2=false`, Einzel-Radio-Voll-Sweep; Radio 1 nicht gefunden → wie bisher `_ok=false`, `jammer start` gibt `false`. Kein `while(1)` / `ESP.restart()`.
- Sendepegel beider Radios = `JAM_PA_LEVEL` (aktuell `RF24_PA_MAX`). Register-Setup von Radio 2 **identisch** zu Radio 1 (AutoAck aus, Retries 0/0, Payload 5, AddrWidth 3, 2 MBPS, CRC aus, stopListening).
- Kommentare Deutsch, Bezeichner Englisch.
- **Recht/Ethik:** Wirksames 2,4-GHz-Jammen nur in der kontrollierten Testumgebung des Nutzers mit eigenen Geräten (BNetzA). Der Plan optimiert ausdrücklich NICHT für öffentlichen Einsatz.
- **Verifikation (kein Host-Test-Framework):** Gate je Task = (a) `idf.py build` grün **und** (b) die angegebene manuelle Serial-Monitor-Prüfung mit erwarteter Ausgabe. Ohne zweite Hardware ist mindestens (a) + der als „ohne Radio 2" markierte Teil von (b) zu prüfen.

## Review Focus

Vom Design implizierte Eingaben/Fehlerfälle, jeweils dem Task zugeordnet, der den Test trägt:

- **Radio 2 fehlt/nicht verdrahtet:** `begin()` meldet es einmal, `_ok2=false`; `jammer start` jammt weiter mit Radio 1 im **Voll-Sweep** `JAM_CH_MIN..JAM_CH_MAX` (keine tote Bandhälfte), kein Absturz. (Task 2)
- **Geteilter SPI-Bus, zwei CSN:** kein nebenläufiger Zugriff — beide `begin()` sequentiell vor Task-Start, alle `setChannel` im selben Task. Beide Radios einzeln adressierbar (eigenes CSN), kein Bus-Garbage. (Task 1)
- **`jammer stop` im Dual-Betrieb:** **beide** Dauerträger gehen aus; fehlt Radio 2, wird dessen `stopConstCarrier()` **nicht** aufgerufen (kein Zugriff auf nicht initialisiertes Radio). (Task 2)
- **`jammer start` doppelt (Dual):** idempotent — kein zweiter Task, kein zweiter `startConstCarrier` auf einem der beiden Radios. (Task 2)
- **Dauerbetrieb ≥60 s (Dual):** der Sweep-Task yieldet weiterhin periodisch → kein Task-Watchdog-Reset, obwohl pro Durchlauf zwei Radios bedient werden. (Task 2)

---

### Task 1: Zweites Radio einbinden + erkennen (geteilter Bus)

Deliverable: Radio 2 wird am selben Bus mit eigenem CE/CSN erkannt und identisch konfiguriert; `status` zeigt `jammer.ok2`. Die Störlogik bleibt in diesem Task noch die bisherige Einzel-Radio-Logik — der Dual-Sweep kommt in Task 2. Dieser Task ist das Erkennungs-/Verdrahtungs-Gate.

**Files:**
- Modify: `firmware/src/config.h` (neue Pins + Bandteilung)
- Modify: `firmware/src/modules/jammer/Jammer.h` (zweites `RF24` + `_ok2` + Status-Feld)
- Modify: `firmware/src/modules/jammer/Jammer.cpp` (Konstruktor, `begin()`-Erkennung, `statusJson()`)

**Interfaces:**
- Consumes: globales `SPI`-Objekt (VSPI, zentral in `main.cpp` initialisiert); `config.h`-Pins; `Module`-Vertrag.
- Produces:
  - `config.h`: `PIN_NRF2_CE` (`32`), `PIN_NRF2_CSN` (`33`), `JAM_CH_MID` (`40`).
  - `JammerModule`-Member: `RF24 _radio2;`, `volatile bool _ok2 = false;`, `volatile uint8_t _channel2 = JAM_CH_MID;` (Task 2 liest/schreibt `_channel2`).
  - `statusJson()` ergänzt `obj["ok2"]` (bool) und `obj["channel2"]` (uint8). Öffentliche 4-Methoden-API unverändert.

- [ ] **Step 1: `config.h` — Pins + Bandteilung ergänzen** (verbatim, im NRF24/Jammer-Abschnitt)

```cpp
// ---- Zweites NRF24 (Option A: selber VSPI-Bus, eigene CE/CSN) ----
#define PIN_NRF2_CE   32   // Radio 2: CE  (freier Output-GPIO, kein Strapping-Pin)
#define PIN_NRF2_CSN  33   // Radio 2: CSN (freier Output-GPIO, kein Strapping-Pin)
#define JAM_CH_MID    40   // Bandteilung: Radio 1 sweept MIN..MID, Radio 2 MID..MAX
```

- [ ] **Step 2: `Jammer.h` — zweites Radio + Status-Felder**
  - Ergänzen: `RF24 _radio2;` (neben `RF24 _radio;`).
  - Ergänzen: `volatile bool _ok2 = false;` und `volatile uint8_t _channel2 = JAM_CH_MID;` (Task-schreibt/Status-liest, wie `_channel`).
  - Konstruktor-Deklaration bleibt; die Member-Init-Liste kommt im `.cpp`.

- [ ] **Step 3: `Jammer.cpp` — Konstruktor + Erkennung in `begin()`**
  - Konstruktor-Init-Liste um Radio 2 erweitern (16 MHz wie Radio 1):

```cpp
JammerModule::JammerModule()
    : _radio(PIN_NRF_CE, PIN_NRF_CSN, 16000000),
      _radio2(PIN_NRF2_CE, PIN_NRF2_CSN, 16000000) {}
```

  - In `begin()`, **nach** dem bestehenden Radio-1-Setup (`_ok = true;`), Radio 2 am selben Bus starten und bei Erfolg **identisch** zu Radio 1 konfigurieren; bei Fehlschlag nur melden und weiterlaufen:

```cpp
if (_radio2.begin(&SPI)) {
  _radio2.setAutoAck(false);
  _radio2.setRetries(0, 0);
  _radio2.setPayloadSize(5);
  _radio2.setAddressWidth(3);
  _radio2.setPALevel(JAM_PA_LEVEL, true);
  _radio2.setDataRate(RF24_2MBPS);
  _radio2.setCRCLength(RF24_CRC_DISABLED);
  _radio2.stopListening();
  _radio2.setChannel(_channel2);
  _ok2 = true;
  Serial.println("[jammer] NRF24 #2 bereit (Dual-Antenne, Bandteilung)");
} else {
  _ok2 = false;
  Serial.println("[jammer] NRF24 #2 nicht gefunden - Einzel-Antenne, Voll-Sweep");
}
```

  - `statusJson()` um die zwei Felder ergänzen: `obj["ok2"] = _ok2;` und `obj["channel2"] = _channel2;`.

- [ ] **Step 4: Build-Prüfung** — Run: `idf.py build`. Expected: grün, `JammerModule` mit zwei `RF24`-Instanzen übersetzt und gelinkt.

- [ ] **Step 5: Hardware-/Serial-Prüfung**
  - **Mit Radio 2 (CE=32/CSN=33 verdrahtet):** beim Start erscheinen `[jammer] NRF24 bereit` **und** `[jammer] NRF24 #2 bereit (Dual-Antenne, Bandteilung)`. `status` → JSON enthält `"ok": true` **und** `"ok2": true`.
  - **Ohne Radio 2 (abgezogen):** `[jammer] NRF24 bereit` + `[jammer] NRF24 #2 nicht gefunden - Einzel-Antenne, Voll-Sweep`; `status` zeigt `"ok": true`, `"ok2": false`; restliches Gerät (NFC/IR/Display/Konsole) läuft normal.

- [ ] **Step 6: Commit**

```bash
git add firmware/src/config.h firmware/src/modules/jammer/Jammer.h firmware/src/modules/jammer/Jammer.cpp
git commit -m "jammer: zweites NRF24 am selben VSPI-Bus einbinden + erkennen (Option A)"
```

---

### Task 2: Dual-Dauerträger-Sweep mit Bandteilung + Einzel-Fallback

Deliverable: `jammer start` schaltet beide Dauerträger einmalig an und sweept das Band geteilt (Radio 1 untere, Radio 2 obere Hälfte) im Gleichschritt; fehlt Radio 2, läuft der bisherige Voll-Sweep eines Radios. `jammer stop` schaltet **beide** vorhandenen Träger sauber ab. Konsole bleibt bedienbar, kein Watchdog-Reset.

**Files:**
- Modify: `firmware/src/modules/jammer/Jammer.cpp` (`sweepTask`)

**Interfaces:**
- Consumes: `_radio`, `_radio2`, `_ok2`, `_channel`, `_channel2`, `_running` aus Task 1; `JAM_CH_MIN`, `JAM_CH_MID`, `JAM_CH_MAX`, `JAM_PA_LEVEL`, `JAM_WDT_YIELD_MS` aus `config.h`.
- Produces: unveränderte öffentliche 4-Methoden-API; `handleCommand`/`begin`/`update`/`statusJson` bleiben wie in Task 1. Nur `sweepTask` ändert sich.

- [ ] **Step 1: `sweepTask` auf Dual-Betrieb + Fallback umbauen** (Algorithmus; durch Signatur/Tests nicht festgelegt, daher als Code)

```cpp
void JammerModule::sweepTask(void* arg) {
  auto* self = static_cast<JammerModule*>(arg);

  // Beide Traeger EINMAL an (Radio 2 nur, wenn vorhanden). Danach nur noch
  // umstimmen. Bandteilung: Radio 1 untere Haelfte, Radio 2 obere Haelfte ->
  // zwei Traeger gleichzeitig in der Luft = doppelte parallele Abdeckung gegen AFH.
  self->_radio.startConstCarrier(JAM_PA_LEVEL, self->_channel);
  if (self->_ok2) {
    self->_radio2.startConstCarrier(JAM_PA_LEVEL, self->_channel2);
  }

  uint32_t lastYield = millis();
  while (self->_running) {
    if (self->_ok2) {
      // Gleichschritt: beide Radios pro Iteration fast gleichzeitig umstimmen.
      // Radio 1: JAM_CH_MIN..JAM_CH_MID, Radio 2: JAM_CH_MID..JAM_CH_MAX
      // (gleich viele Schritte; Ueberlappung auf JAM_CH_MID ist unschaedlich).
      for (uint8_t i = 0; i <= (JAM_CH_MID - JAM_CH_MIN); ++i) {
        self->_radio.setChannel(JAM_CH_MIN + i);
        self->_radio2.setChannel(JAM_CH_MID + i);
      }
      self->_channel  = JAM_CH_MID;
      self->_channel2 = JAM_CH_MAX;
    } else {
      // Fallback ohne Radio 2: ein Radio sweept das ganze Band (wie bisher).
      for (uint8_t ch = JAM_CH_MIN; ch <= JAM_CH_MAX; ++ch) {
        self->_radio.setChannel(ch);
      }
      self->_channel = JAM_CH_MAX;
    }

    if (millis() - lastYield >= JAM_WDT_YIELD_MS) {
      vTaskDelay(1);   // Idle-Task/Watchdog bedienen (~<2 % Sendezeit)
      lastYield = millis();
    }
  }

  // Aus-Zustand im selben Task-Kontext: beide vorhandenen Traeger abschalten.
  self->_radio.stopConstCarrier();
  if (self->_ok2) {
    self->_radio2.stopConstCarrier();
  }
  self->_active = false;
  vTaskDelete(nullptr);
}
```

> Hinweis: `JAM_CH_MID + i` erreicht bei `JAM_CH_MID=40`, `JAM_CH_MIN=0` den Wert `80` (= `JAM_CH_MAX`); der Bereich bleibt in `uint8_t`. Wird `JAM_CH_MID` später so gesetzt, dass `JAM_CH_MID + (JAM_CH_MID - JAM_CH_MIN) > JAM_CH_MAX`, Radio 2 auf `JAM_CH_MAX` klemmen.

- [ ] **Step 2: Build-Prüfung** — Run: `idf.py build`. Expected: grün.

- [ ] **Step 3: Hardware-/Serial-Prüfung (kontrollierte Umgebung, eigene Geräte)**
  - **Mit Radio 2:** `jammer start` → eigener BT-Link zwischen den Testgeräten bricht/stottert **deutlich hartnäckiger** als mit einer Antenne (Ziel kann nicht mehr so leicht in die freie Bandhälfte ausweichen). `status` während des Jammens → `active=true`, antwortet weiter (Serial); `jammer stop` → sofort angenommen, **beide** Träger aus, Link erholt sich.
  - **≥60 s** durchlaufen → **kein** Reboot/Watchdog-Reset im Monitor.
  - **`jammer start` zweimal** → nur ein Task, kein zweiter `startConstCarrier` (keine Doppelaktivierung). **`jammer stop` ohne vorherigen `start`** → kein Absturz.
  - **Ohne Radio 2 (abgezogen):** `jammer start` → Voll-Sweep eines Radios wie bisher wirksam; `jammer stop` → sauber aus, **kein** Zugriff auf Radio 2 (keine Fehlermeldung/kein Hang).

- [ ] **Step 4: Commit**

```bash
git add firmware/src/modules/jammer/Jammer.cpp
git commit -m "jammer: Dual-Dauertraeger-Sweep mit Bandteilung + Einzel-Antenne-Fallback"
```

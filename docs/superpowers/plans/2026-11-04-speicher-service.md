# Speicher-Service (StorageService) + NFC-Auto-Save — Implementierungsplan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Den in `firmware/src/storage/StorageService.h` spezifizierten zentralen Speicher-Dienst implementieren (RAM-Fallback + SD-Backend hinter einer Schnittstelle, **auf mehrere fokussierte Dateien aufgeteilt** wie beim NFC-Modul) und das NFC-Modul so verdrahten, dass eine erfolgreich gelesene Karte automatisch mit **einem** Aufruf dauerhaft gespeichert wird — dedupliziert über die 16-stellige Kartennummer.

**Architecture:** Strategy-Pattern. `StorageService` ist ein dünner **Orchestrator**, der den fixen Vertrag aus `StorageService.h` erfüllt: `begin()` erkennt die SD einmalig und wählt ein Backend, die übrigen Methoden dispatchen an das aktive Backend. Zwei austauschbare Backends hinter der abstrakten Basis `StorageBackend`: `RamBackend` (flüchtige Tabelle fester Größe, läuft ohne Karte) und `SdBackend` (eine Datei je Datensatz unter `/<ns>/<key>.json`). Die SD-Logik lebt **ausschließlich** im `SdBackend` (ARBEITSWEISE.md §4/§6.2) — kein separates `modules/sdcard/`-Modul. Da `StorageService.h` die fixe Spec ist und **keine** Member hat, liegt der aktive-Backend-Zustand datei-statisch in `StorageService.cpp`; beide Backends sind statische Objekte (kein Heap). Das NFC-Modul serialisiert `CardData` selbst zu JSON und ruft `Storage.save("nfc", cardNumber, json)` genau dort auf, wo auch „ENDE DUMP" erscheint (= `_card.readSuccess == true`).

**Tech Stack:** ESP-IDF 5.5 + Arduino-Komponente (arduino-esp32 ~3.3.0), C++ (xtensa-esp32-elf-g++). `SD`, `FS`, `SPI` kommen aus der Arduino-Komponente — **keine neue Bibliothek**. `ArduinoJson ~7.4.3` für Serialisierung/`list`. PN532 unberührt.

**Spec:** Es gibt **keine** separate Design-Datei; die verbindliche Spec liegt bereits im Repo:
- `firmware/src/storage/StorageService.h` — fixe Schnittstelle (Signaturen **unverändert** umsetzen).
- `firmware/src/storage/README.md` — Verhalten (RAM zuerst, SD als Add-on, nie abbrechen).
- `firmware/src/modules/sdcard/README.md` — SD-Hardware-Hinweise (gemeinsamer SPI-Bus mit NRF24, eigener CS).
- `firmware/src/core/Module.h` + `docs/superpowers/specs/2026-10-23-orcaone-architektur-design.md` — Modul-/Ordnerprinzip.
- Kein automatisiertes Testframework im Projekt. **Hinweis:** `idf.py` ist in der aktuellen Umgebung nicht im PATH — die `idf.py build`-Schritte muss ggf. der Nutzer ausführen; der Implementierer meldet das als Limitierung, statt Erfolg zu behaupten.

## Dateistruktur

Alles unter `firmware/src/storage/` (§2). Eine Verantwortung je Datei, analog zum NFC-Modul:

| Datei | Verantwortung |
|---|---|
| `StorageService.h` | **Vorhanden, unverändert.** Öffentlicher Vertrag + `extern Storage`. |
| `StorageService.cpp` | Orchestrator: Backend-Wahl in `begin()`, Dispatch, Schlüssel-Guard, `Storage`-Definition. |
| `StorageBackend.h` | Abstrakte Basis (Vertrag für Backends): `save/load/list/remove`. |
| `RamBackend.h` / `.cpp` | Flüchtiges RAM-Backend (Tabelle fester Größe). |
| `SdBackend.h` / `.cpp` | SD-Backend: `begin(csPin)` + Datei-I/O unter `/<ns>/<key>.json`. |

## Global Constraints

- **Schnittstelle fix:** Die sechs Methoden aus `StorageService.h` (`begin, available, save, load, list, remove`) werden mit **exakt** diesen Signaturen implementiert. `StorageService.h` und `Module.h` werden **nicht** angefasst (§6.3).
- **Pins nur aus `config.h`:** SD-CS = `PIN_SD_CS` (aktuell `16`, bereits einkommentiert). Niemals eine Pin-Nummer im Code hart eintippen. CS muss ≠ `PIN_NRF_CSN` (5) sein. `PIN_SD_CS` wird vom Orchestrator an `SdBackend::begin()` übergeben (Backend kennt keine Pins direkt, §5).
- **SPI-Bus geteilt mit NRF24:** ESP32-Default-VSPI-Pins (SCK 18 / MISO 19 / MOSI 23) sind identisch mit `PIN_NRF_SCK/MISO/MOSI` → `SD.begin(PIN_SD_CS)` nutzt denselben Bus korrekt.
- **Nie abbrechen ohne SD (§6.5):** Fehlt Karte/Modul, `begin()` meldet **einmal** per Serial, Service läuft im RAM weiter. Kein `while(1)`, kein `ESP.restart()`, kein Warten ohne Zeitlimit.
- **Erkennung nur beim Start** (kein Hot-Plug).
- **Namensraum = `name()` des Moduls**, Daten = JSON-Text; das Modul serialisiert selbst.
- **Neue `.cpp` → `idf.py reconfigure`** nötig, damit `GLOB_RECURSE` in `main/CMakeLists.txt` sie erfasst. Verifikation daher: `idf.py reconfigure && idf.py build` (bei reinen Edits `idf.py build`).

## Review Focus

Mangels Testsuite ist die Gefahr an Laufzeit-Randfällen am größten; jeder Punkt ist einer Verifikation zugeordnet (Build bzw. manuelle Hardware-Abnahme in Task 3):

- **Keine SD / Karte entfernt beim Start** → muss sauber auf RAM fallen (`available()==false`), eine Serial-Meldung, nichts stürzt ab. Geprüft: Task 2 (begin-Logik) + manuelle Abnahme „Boot ohne Karte".
- **Dieselbe PAN zweimal gelesen** → genau **eine** Datei `/nfc/<PAN>.json`, Inhalt aktualisiert, kein Duplikat. Geprüft: manuelle Abnahme „zweimal dieselbe Karte" (Task 3), folgt aus PAN-als-Schlüssel.
- **`readSuccess==true`, aber `cardNumber` leer** (seltener Teil-Read) → **nicht** speichern (leerer Schlüssel = ungültiger Dateiname), sondern überspringen + Serial-Hinweis. Geprüft: Guard in Task 3.
- **SD-Schreibfehler** (Karte voll/defekt) → `save()` gibt `false`, Gerät läuft weiter; NFC-Modul meldet den `false`-Rückgabewert per Serial. Geprüft: Code-Pfad in Task 2 + Rückgabe-Nutzung in Task 3.
- **Dateisystem-unsicherer Schlüssel** eines künftigen Moduls (enthält `/` oder leer) → würde ungültige Pfade erzeugen. NFC ist sicher (PAN = nur Ziffern); für den generischen Fall lehnt der Orchestrator `save()` solche Schlüssel **vor** dem Dispatch ab (`false`). Geprüft: Task 1 (Guard im Orchestrator, gilt backendübergreifend).

---

### Task 1: Backend-Vertrag + RAM-Backend + Orchestrator (läuft ohne SD)

**Files:**
- Create: `firmware/src/storage/StorageBackend.h`
- Create: `firmware/src/storage/RamBackend.h`, `firmware/src/storage/RamBackend.cpp`
- Create: `firmware/src/storage/StorageService.cpp`
- Modify: — (`StorageService.h` bleibt unverändert; er ist die Spec.)

**Interfaces:**
- Produces:
  ```cpp
  // StorageBackend.h — Vertrag für Backends
  class StorageBackend {
  public:
    virtual ~StorageBackend() {}
    virtual bool   save(const String& ns, const String& key, const String& data) = 0;
    virtual String load(const String& ns, const String& key) = 0;   // "" wenn nicht vorhanden
    virtual String list(const String& ns) = 0;                      // JSON-Array-Text, "[]" wenn leer
    virtual bool   remove(const String& ns, const String& key) = 0;
  };

  // RamBackend.h
  class RamBackend : public StorageBackend { /* override der 4 Methoden */ };

  // StorageService.cpp — Definition des globalen Objekts
  StorageService Storage;
  ```
- Consumes: `StorageService.h` (Vertrag); `<ArduinoJson.h>` (für `list`).

**RAM-Semantik (vom Plan festgelegt):**
```cpp
static constexpr size_t Cap = 16;         // feste Obergrenze, flüchtig
struct Entry { bool used=false; String ns, key, data; };
```
`save`: existiert `(ns,key)` → überschreiben; sonst ersten freien Slot belegen; kein freier Slot → `false` + einmal Serial-Hinweis. `load`: Treffer → `data`, sonst `""`. `list`: mit `ArduinoJson` ein Array aller `key` mit passendem `ns` bauen, leer → `"[]"`. `remove`: Slot freigeben, `true` wenn vorhanden.

**Orchestrator-Semantik (`StorageService.cpp`), datei-statischer Zustand:**
```cpp
static RamBackend ramBackend;
static StorageBackend* active = &ramBackend;   // Task 2 setzt ggf. SD
static bool useSd = false;
```
`begin()` (Task-1-Stand): `active=&ramBackend; useSd=false;` + `Serial.println("[storage] RAM-Backend aktiv (SD-Erkennung folgt)")`. `available()` → `useSd`. `save()`: **Schlüssel-Guard zuerst** (leer oder enthält `'/'` → `false`), dann `active->save(...)`. `load/list/remove` → direkt an `active` delegieren.

- [ ] **Step 1:** `StorageBackend.h` mit der abstrakten Basis (4 rein-virtuelle Methoden) anlegen.
- [ ] **Step 2:** `RamBackend.h/.cpp` anlegen; die 4 Methoden nach obiger RAM-Semantik implementieren (`#include <ArduinoJson.h>` in der `.cpp` für `list`).
- [ ] **Step 3:** `StorageService.cpp` anlegen: `Storage` definieren, datei-statischen Zustand + `begin/available/save/load/list/remove` als Orchestrator (Guard + Dispatch an `active`).
- [ ] **Step 4: Build.** Run: `idf.py reconfigure && idf.py build` — Expected: SUCCESS. (Falls `idf.py` nicht verfügbar: als Limitierung melden, nicht als Erfolg.)
- [ ] **Step 5: Commit.** `git add firmware/src/storage/StorageBackend.h firmware/src/storage/RamBackend.* firmware/src/storage/StorageService.cpp && git commit -m "Storage: Backend-Vertrag + RAM-Backend + Orchestrator (laeuft ohne SD)"`

---

### Task 2: `SdBackend` + Start-Erkennung im Orchestrator

**Files:**
- Create: `firmware/src/storage/SdBackend.h`, `firmware/src/storage/SdBackend.cpp`
- Modify: `firmware/src/storage/StorageService.cpp` (SD in `begin()` wählen)

**Interfaces:**
- Produces:
  ```cpp
  // SdBackend.h
  class SdBackend : public StorageBackend {
  public:
    bool begin(uint8_t csPin);     // true wenn Karte erkannt (SD.begin)
    bool   save(...) override; String load(...) override;
    String list(...) override;     bool   remove(...) override;
  private:
    String pathFor(const String& ns, const String& key); // "/" + ns + "/" + key + ".json"
    String dirFor(const String& ns);                      // "/" + ns
  };
  ```
- Consumes: `StorageBackend.h`; RAM-/Orchestrator-Teile aus Task 1; `<SD.h>`, `<SPI.h>`; `config.h` (`PIN_SD_CS`) **im Orchestrator**, nicht im Backend.

**SD-Semantik (vom Plan festgelegt):**
`save`: `SD.mkdir(dirFor(ns))` (idempotent); existiert die Datei → vorher `SD.remove(path)` (sauberes Überschreiben, kein Anhängen); im `FILE_WRITE` öffnen, `data` schreiben, schließen; Rückgabe = Schreiberfolg. `load`: Datei fehlt → `""`, sonst Inhalt. `list`: `dirFor(ns)` öffnen, Einträge iterieren, Dateinamen **ohne** `.json`-Endung als `ArduinoJson`-Array sammeln; Ordner fehlt → `"[]"`. `remove`: `SD.remove(path)`.

- [ ] **Step 1:** `SdBackend.h/.cpp` anlegen; `begin(csPin)` → `SD.begin(csPin)`; die 4 Methoden + `pathFor/dirFor` nach obiger Semantik (`#include <SD.h>`, `<SPI.h>`, `<ArduinoJson.h>`).
- [ ] **Step 2:** In `StorageService.cpp`: `#include "SdBackend.h"` + `#include "../config.h"`; `static SdBackend sdBackend;` ergänzen. `begin()` erweitern: `useSd = sdBackend.begin(PIN_SD_CS);` → bei Erfolg `active=&sdBackend;` + `Serial.println("[storage] SD erkannt - dauerhafter Speicher aktiv")`, sonst `active=&ramBackend;` + **einmal** `Serial.println("[storage] keine SD - Speicher nur im RAM")`. Kein Blockieren.
- [ ] **Step 3: Build.** Run: `idf.py reconfigure && idf.py build` — Expected: SUCCESS, keine Warnungen zu `SD`/`SPI`.
- [ ] **Step 4: Commit.** `git add firmware/src/storage/SdBackend.* firmware/src/storage/StorageService.cpp && git commit -m "Storage: SD-Backend + Start-Erkennung im Orchestrator"`

---

### Task 3: NFC-Auto-Save verdrahten + Service in `setup()` starten

**Files:**
- Modify: `firmware/src/main.cpp` (Service starten)
- Modify: `firmware/src/modules/nfc/NfcModule.cpp` (Auto-Save nach erfolgreichem Read)
- Modify: `firmware/src/modules/nfc/NfcModule.h` (private Serialisier-Helfer deklarieren)

**Interfaces:**
- Consumes: `Storage.begin()`, `Storage.save("nfc", key, json)` aus Task 1/2.
- Produces (privat in `NfcModule`):
  ```cpp
  String cardToJson() const;   // serialisiert _card zu JSON-Objekt-Text
  ```

**Serialisierung (Felder = Klartext-Anzeigedaten, PII bewusst akzeptiert):**
`cardToJson()` baut mit einem lokalen `JsonDocument` ein Objekt mit den Schlüsseln `uid, aid, cardType, cardNumber, expiry, cardholder, countryCode, currencyCode, effectiveDate, psn, serviceCode, aip` aus den gleichnamigen `_card`-Feldern und gibt `serializeJson`-Text zurück.

- [ ] **Step 1:** In `main.cpp`: `#include "storage/StorageService.h"`; in `setup()` **vor** `nfc.begin()` ein `Storage.begin();` einfügen.
- [ ] **Step 2:** In `NfcModule.h` den privaten `String cardToJson() const;` deklarieren. In `NfcModule.cpp` `#include "../../storage/StorageService.h"` ergänzen und `cardToJson()` implementieren (Felder wie oben).
- [ ] **Step 3:** In `NfcModule::update()` **unmittelbar nach** `_emvReader.readCard(_card, _capture);` (aktuell Zeile 91) einfügen:
  ```cpp
  if (_card.readSuccess && _card.cardNumber.length() > 0) {
    bool saved = Storage.save("nfc", _card.cardNumber, cardToJson());
    Serial.println(saved ? "[nfc] Karte gespeichert" : "[nfc] Speichern fehlgeschlagen");
  } else if (_card.readSuccess) {
    Serial.println("[nfc] Kartennummer fehlt - nicht gespeichert");
  }
  ```
- [ ] **Step 4: Build.** Run: `idf.py reconfigure && idf.py build` — Expected: SUCCESS.
- [ ] **Step 5: Commit.** `git add firmware/src/main.cpp firmware/src/modules/nfc/NfcModule.* && git commit -m "NFC: erfolgreich gelesene Karte automatisch ueber Storage speichern"`
- [ ] **Step 6: Manuelle Hardware-Abnahme (durch Nutzer, da `idf.py`/Hardware hier fehlen).** Checkliste:
  1. **Mit SD-Karte:** Boot → Serial zeigt `[storage] SD erkannt`. Karte lesen bis „ENDE DUMP" → `[nfc] Karte gespeichert`. Auf der SD existiert `/nfc/<PAN>.json` mit den Feldern.
  2. **Dieselbe Karte erneut** → weiterhin genau **eine** Datei `/nfc/<PAN>.json` (Inhalt überschrieben, kein Duplikat).
  3. **Zweite, andere Karte** → zusätzliche Datei `/nfc/<PAN2>.json`; `Storage.list("nfc")` liefert beide PANs.
  4. **Ohne SD-Karte booten** → Serial zeigt `[storage] keine SD - Speicher nur im RAM`; Lesen funktioniert weiter, `[nfc] Karte gespeichert` (RAM), kein Absturz.

---

## Selbstprüfung (Ergebnis)

- **Spec-Abdeckung:** `StorageService.h` (alle 6 Methoden → Orchestrator Task 1/2), `storage/README.md` (RAM zuerst → Task 1; SD-Add-on → Task 2; nie abbrechen → begin-Logik Task 2 + §6.5-Constraint), Mehr-Datei-Wunsch des Nutzers → Dateistruktur (Backend-Vertrag + 2 Backends + Orchestrator), NFC-Auto-Save → Task 3. Keine offenen Spec-Punkte.
- **Typkonsistenz:** `StorageBackend`, `RamBackend`, `SdBackend`, `active`, `useSd`, `pathFor/dirFor`, `cardToJson` durchgängig gleich benannt in Task 1→3. Die 4 Backend-Signaturen sind in `StorageBackend.h` einmal definiert und in beiden Backends identisch overridden.
- **Proportion:** Plan bewusst knapp; Code-Blöcke nur dort, wo der Plan Werte/Format festlegt (Backend-Vertrag, RAM-/SD-Semantik, Pfadschema, Save-Guard), Körper sonst dem Implementierer überlassen.
- **Review Focus:** Fünf Randfälle gelistet, jeder einer Verifikation zugeordnet (Build bzw. manuelle Abnahme Task 3).

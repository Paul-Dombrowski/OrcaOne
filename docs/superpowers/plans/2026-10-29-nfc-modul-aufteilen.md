# NFC-Modul in fokussierte Dateien aufteilen — Implementierungsplan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `NfcModule.cpp` (~1.500 Zeilen, vier vermischte Verantwortlichkeiten) in fokussierte Dateien aufteilen, ohne das Laufzeitverhalten zu ändern.

**Architecture:** Reiner, verhaltenserhaltender Refactor (Code-Verschiebung, keine Logikänderung). `NfcModule` wird zum dünnen Orchestrator, der den `Module`-Vertrag erfüllt und an spezialisierte Einheiten delegiert: `EmvReader` (Lesefluss), `EmvEmulator` (Capture-Replay), `EmvDump` (Diagnose-Ausgabe), plus die zustandslosen Helfer `Tlv` und `EmvTags` und die geteilten Datentypen in `EmvTypes.h`. Zustand wandert aus verstreuten `_last*`/`_emu*`-Membern in zwei Structs: `CardData` (geparste Anzeigefelder) und `EmvCapture` (rohe Replay-Blobs).

**Tech Stack:** ESP-IDF + Arduino-Komponente, C++ (xtensa-esp32-elf-g++ 14.2). PN532/PN532_I2C als IDF-Komponenten. Keine externen Libs neu.

**Spec:** Dieser Refactor-Auftrag (Goal oben) + `firmware/src/core/Module.h` (Modul-Vertrag, unverändert einzuhalten) + `docs/superpowers/specs/2026-10-23-orcaone-architektur-design.md` (Ordner-/Modulprinzip). Es gibt **keine** separate Spec-Datei und **kein** automatisiertes Testframework im Projekt.

## Global Constraints

- **Verhalten unverändert:** Serielle Ausgaben, APDU-Logik, Statuswörter, Puffergrößen und Timing bleiben byte-/verhaltensgleich. Dies ist ein Move-Refactor, keine Verbesserung.
- **`statusJson`-Schlüssel bleiben identisch** (die Web-UI liest sie): `ok, ready, readPending, lastUid, lastAid, lastCardType, lastCardNumber, lastExpiry, lastCardholder, lastCountryCode, lastCurrencyCode, lastEffectiveDate, lastPsn, lastServiceCode, lastAip, cardReadSuccess, emulating, emuReady`.
- **`handleCommand`-Actions bleiben:** `read`, `emulate`, `stop`, `clear`.
- **Puffergrenzen bleiben:** Replay-Blobs ≤ 255 B (`tgSetData`-Länge ist `uint8_t`), Empfang ≤ 255 B, max. 8 Records.
- **Platzierung:** Alle neuen Dateien unter `firmware/src/modules/nfc/` (ARBEITSWEISE.md §2: Modulcode nur im eigenen Ordner).
- **Build nach jedem Dateizuwachs:** Neue `.cpp` werden vom `GLOB_RECURSE` in `main/CMakeLists.txt` erst nach `idf.py reconfigure` erkannt. Verifikation daher immer: `idf.py reconfigure && idf.py build` (bei reinen Edits ohne neue Datei genügt `idf.py build`).
- **Pins/Konstanten ausschließlich aus `config.h`** (u. a. `NFC_EMU_INIT_TIMEOUT_MS`, `NFC_PASSIVE_RETRIES`).

## Review Focus

Mangels Testsuite ist die Regressionsgefahr an den Schnittstellen am größten; jeder Punkt ist der Verifikation der besitzenden Task zugeordnet:

- **`statusJson`-Schlüssel versehentlich umbenannt** beim Umzug auf `CardData` → Web-UI zeigt leere Felder. Geprüft in Task 4 (Schlüsselliste gegen Global Constraints abgleichen).
- **`EmvCapture` wird beim Lesen nicht mehr befüllt** (Seam Reader→Capture), `emulate` bricht still ab → geprüft in Task 5 (nach `read` muss `emuReady`/`_capture.valid == true` sein).
- **Neue `.cpp` ohne `reconfigure`** → Linkerfehler „undefined reference" oder stale Objekt. In jeder Datei-Task als Verifikationsschritt verankert.
- **Puffergröße bei `EmvCapture`-Umzug verkleinert** (z. B. `BlobCap` < 255) → lange Records/GPO werden abgeschnitten, App zeigt Teildaten. Geprüft in Task 4 (`static_assert`-Zeile auf `BlobCap == 255`).
- **`#include`-Zyklus / fehlende Vorwärtsdeklaration** zwischen `EmvReader` und `EmvDump` (beide halten `PN532&`) → Build bricht. Geprüft in Task 3/5 via Build.

---

### Task 1: `EmvTags` auslagern (zustandslos, kein PN532)

**Files:**
- Create: `firmware/src/modules/nfc/EmvTags.h`
- Create: `firmware/src/modules/nfc/EmvTags.cpp`
- Modify: `firmware/src/modules/nfc/NfcModule.h` (Deklarationen von `emvTagName`/`printTagHex` entfernen), `firmware/src/modules/nfc/NfcModule.cpp` (Definitionen entfernen, Aufrufe auf Namespace umstellen)

**Interfaces:**
- Produces:
  ```cpp
  namespace EmvTags {
    const char* name(uint32_t tag);   // war NfcModule::emvTagName
    void        printTag(uint32_t tag); // war NfcModule::printTagHex (Serial-Ausgabe)
  }
  ```
- Consumes: nichts (nur `<Arduino.h>` für `Serial`).

- [ ] **Step 1:** `EmvTags.h`/`.cpp` anlegen; Körper von `NfcModule::emvTagName` (Zeilen ~879–992) und `NfcModule::printTagHex` (~993–1001) **unverändert** in `namespace EmvTags` als `name`/`printTag` übernehmen.
- [ ] **Step 2:** In `NfcModule.cpp`/`.h` beide Member entfernen; alle Aufrufe `emvTagName(` → `EmvTags::name(`, `printTagHex(` → `EmvTags::printTag(`. `#include "EmvTags.h"` in `NfcModule.cpp`.
- [ ] **Step 3: Build.** Run: `idf.py reconfigure && idf.py build` — Expected: SUCCESS, keine Warnungen zu undef. Symbolen.
- [ ] **Step 4: Commit.** `git add firmware/src/modules/nfc/EmvTags.* firmware/src/modules/nfc/NfcModule.* && git commit -m "NFC: EMV-Tag-Namen in EmvTags ausgelagert"`

---

### Task 2: `Tlv` + `EmvTypes.h` (EmvAid) auslagern (zustandslos, kein PN532)

**Files:**
- Create: `firmware/src/modules/nfc/EmvTypes.h`
- Create: `firmware/src/modules/nfc/Tlv.h`, `firmware/src/modules/nfc/Tlv.cpp`
- Modify: `NfcModule.h` (EmvAid-Struct + Tlv-Deklarationen entfernen), `NfcModule.cpp` (Definitionen entfernen, Aufrufe auf `Tlv::` umstellen, Includes ergänzen)

**Interfaces:**
- Produces:
  ```cpp
  // EmvTypes.h
  struct EmvAid { uint8_t aid[16]; uint8_t len; char label[24]; uint8_t priority; };

  // Tlv.h
  namespace Tlv {
    int  findTag(const uint8_t* data, int dataLen, uint32_t tag, int depth = 0);
    void extractAllAids(const uint8_t* data, int dataLen, std::vector<EmvAid>& out);
    void parseTrack2(const uint8_t* t2, uint8_t len, String& outPan, String& outExp);
    void printHex(const uint8_t* data, uint8_t length);
  }
  ```
- Consumes: `EmvAid` (aus `EmvTypes.h`).

- [ ] **Step 1:** `EmvTypes.h` anlegen, `EmvAid` (aktuell `NfcModule.h` Zeilen ~7–12) **unverändert** dorthin verschieben.
- [ ] **Step 2:** `Tlv.h`/`.cpp` anlegen; Körper von `findTag`, `extractAllAids`, `parseTrack2`, `printHex` **unverändert** in `namespace Tlv` übernehmen.
- [ ] **Step 3:** In `NfcModule.*` die vier Member + `EmvAid` entfernen; Aufrufe auf `Tlv::findTag(` / `Tlv::extractAllAids(` / `Tlv::parseTrack2(` / `Tlv::printHex(` umstellen; `#include "EmvTypes.h"`, `#include "Tlv.h"`.
- [ ] **Step 4: Build.** Run: `idf.py reconfigure && idf.py build` — Expected: SUCCESS.
- [ ] **Step 5 (optional, empfohlen): Host-Charakterisierungstest.** Kleines `g++`-Programm (ohne Framework, nur `assert`) mit String-/Serial-Shim, das eine bekannte Record-Byte-Sequenz durch `Tlv::parseTrack2` schickt und PAN/Expiry gegen erwartete Werte prüft. Als Sicherheitsnetz vor Task 5. Run: `g++ -std=c++17 test_tlv.cpp Tlv.cpp -o t && ./t` — Expected: keine Assert-Fehler. (Wenn der String-Shim zu aufwendig wird: überspringen, Build-Grün bleibt die Mindestabsicherung.)
- [ ] **Step 6: Commit.** `git commit -m "NFC: TLV-Helfer + EmvAid in Tlv/EmvTypes ausgelagert"`

---

### Task 3: `EmvDump` auslagern (Diagnose-Dump, hält `PN532&`)

**Files:**
- Create: `firmware/src/modules/nfc/EmvDump.h`, `firmware/src/modules/nfc/EmvDump.cpp`
- Modify: `NfcModule.h`/`.cpp` (Dump-Member entfernen, `EmvDump`-Instanz halten + Aufruf delegieren)

**Interfaces:**
- Consumes: `PN532&`, `Tlv::`, `EmvTags::`.
- Produces:
  ```cpp
  class EmvDump {
  public:
    explicit EmvDump(PN532& reader);
    // war NfcModule::dumpAllCardData — gibt FCI+GPO+AFL-Records+GET-DATA annotiert aus
    void dumpAll(const uint8_t* fci, uint8_t fciLen,
                 const uint8_t* gpo, uint8_t gpoLen,
                 const uint8_t* afl, uint8_t aflLen, bool aflValid);
  };
  ```
  Intern (privat) umziehen: `dumpTlv`, `decodeHint`, `readAndDumpRecords`, `dumpGetDataTags`, `getDataTag`.

- [ ] **Step 1:** `EmvDump.h`/`.cpp` anlegen; `dumpAllCardData`→`dumpAll` plus die fünf privaten Dump-Helfer **unverändert** übernehmen; `PN532& _reader`-Referenz im Konstruktor speichern; `getDataTag`/`readAndDumpRecords` nutzen `_reader` statt des alten Members.
- [ ] **Step 2:** In `NfcModule` die Dump-Member entfernen, `EmvDump _dump{_reader};` als Member; der eine Aufruf `dumpAllCardData(...)` in `processCard` → `_dump.dumpAll(...)`. `#include "EmvDump.h"`.
- [ ] **Step 3: Build.** Run: `idf.py reconfigure && idf.py build` — Expected: SUCCESS (prüft Review-Focus: kein Include-Zyklus mit PN532).
- [ ] **Step 4: Commit.** `git commit -m "NFC: Diagnose-Dump in EmvDump ausgelagert"`

---

### Task 4: Zustand in `CardData` + `EmvCapture` bündeln (in `EmvTypes.h`)

**Files:**
- Modify: `firmware/src/modules/nfc/EmvTypes.h` (zwei Structs ergänzen), `firmware/src/modules/nfc/EmvTypes.cpp` (neu, für `clear`/`reset`/`put*`), `NfcModule.h`/`.cpp` (verstreute Member durch `CardData _card;` + `EmvCapture _capture;` ersetzen; `parseRecordData`, `statusJson`, Capture-Hooks, `clear`-Action anpassen)

**Interfaces:**
- Produces:
  ```cpp
  struct CardData {
    String uid, aid, cardType, cardNumber, expiry, cardholder;
    String countryCode, currencyCode, effectiveDate, psn, serviceCode, aip;
    bool   readSuccess = false;
    void clear();   // alle Strings = "", readSuccess = false
  };

  struct EmvCapture {
    static constexpr uint8_t MaxRecords = 8;
    static constexpr uint8_t BlobCap    = 255;   // tgSetData-Länge ist uint8_t
    struct Record { uint8_t sfi, rec, len; uint8_t data[BlobCap]; };

    bool valid = false;
    uint8_t ppse[BlobCap]; uint8_t ppseLen = 0;
    uint8_t aid[16];       uint8_t aidLen  = 0;
    uint8_t fci[BlobCap];  uint8_t fciLen  = 0;
    uint8_t gpo[BlobCap];  uint8_t gpoLen  = 0;
    Record  records[MaxRecords]; uint8_t recordCount = 0;

    void reset();                                                   // war resetEmuCapture
    void putBlob(uint8_t* dst, uint8_t& dstLen, const uint8_t* src, int srcLen); // war captureBlob
    void putRecord(uint8_t sfi, uint8_t rec, const uint8_t* data, int len);      // war captureRecord
  };
  ```
- Consumes: —

- [ ] **Step 1:** `CardData`/`EmvCapture` in `EmvTypes.h` ergänzen, `clear`/`reset`/`putBlob`/`putRecord` in `EmvTypes.cpp` aus den bisherigen Körpern von `resetEmuCapture`/`captureBlob`/`captureRecord` übernehmen. `static_assert(EmvCapture::BlobCap == 255, "Replay-Blob muss 255 bleiben");` ergänzen (Review-Focus).
- [ ] **Step 2:** In `NfcModule` alle `_last*`-Member durch `_card` ersetzen; alle `_emu*`-Daten-Member durch `_capture`. `parseRecordData` schreibt in ein übergebenes `CardData&` statt in Member (Signatur: `bool parseRecordData(CardData&, const uint8_t*, uint8_t)`).
- [ ] **Step 3:** `statusJson` auf `_card`/`_capture` umstellen — **Schlüssel exakt wie in Global Constraints** (`lastCardNumber` = `_card.cardNumber` usw., `emuReady` = `_capture.valid`). `clear`-Action → `_card.clear()`.
- [ ] **Step 4: Build.** Run: `idf.py build` — Expected: SUCCESS.
- [ ] **Step 5: Schlüssel-Review (Review-Focus).** `statusJson`-Objektschlüssel 1:1 gegen die Liste in Global Constraints abgleichen; 18 Schlüssel, keiner umbenannt/fehlend.
- [ ] **Step 6: Commit.** `git commit -m "NFC: Kartenzustand in CardData/EmvCapture gebuendelt"`

---

### Task 5: `EmvReader` auslagern (Lesefluss, hält `PN532&`)

**Files:**
- Create: `firmware/src/modules/nfc/EmvReader.h`, `firmware/src/modules/nfc/EmvReader.cpp`
- Modify: `NfcModule.h`/`.cpp` (Lese-Member entfernen, `EmvReader`-Instanz halten, `processCard` → ein Aufruf)

**Interfaces:**
- Consumes: `PN532&`, `CardData`, `EmvCapture`, `EmvAid`, `Tlv::`, `EmvTags::`, `EmvDump`, `FallbackAids` (mitnehmen).
- Produces:
  ```cpp
  class EmvReader {
  public:
    explicit EmvReader(PN532& reader);
    // PPSE -> AID -> GPO -> Records. Füllt out + cap. true bei Erfolg.
    bool readCard(CardData& out, EmvCapture& cap);
  };
  ```
  Intern (privat) umziehen: `selectPpse`, `selectAid`, `buildPdolData`, `tryGetProcessingOptions`, `readCardRecords`, `parseRecordData` und die Orchestrierung aus `processCard`. Hält eine `EmvDump`-Instanz für den Erfolgs-Dump.

- [ ] **Step 1:** `EmvReader.h`/`.cpp` anlegen; die genannten Funktionen **unverändert** übernehmen (Member-Zugriffe → Parameter `out`/`cap`); `SelectPPSE`/`FallbackAids` (anon. namespace in `NfcModule.cpp`) nach `EmvReader.cpp` verschieben. Die in Task 3 angelegte `EmvDump`-Instanz **wandert von `NfcModule` hierher**: `EmvReader` hält `EmvDump _dump{_reader};` und ruft im Erfolgsfall `_dump.dumpAll(...)`.
- [ ] **Step 2:** `processCard`-Körper wird `readCard(CardData& out, EmvCapture& cap)`; Capture-Hooks schreiben in `cap` (`cap.putBlob`, `cap.putRecord`); Erfolg setzt `out.readSuccess = true; cap.valid = true;`.
- [ ] **Step 3:** In `NfcModule`: `EmvReader _emvReader{_reader};`; `update()` ruft bei erkannter Karte `_emvReader.readCard(_card, _capture)`; alte Lese-Member/-Methoden **und die `EmvDump _dump`-Member** aus `NfcModule` entfernen (Dump ist jetzt in `EmvReader`). `#include "EmvReader.h"`.
- [ ] **Step 4: Build.** Run: `idf.py reconfigure && idf.py build` — Expected: SUCCESS.
- [ ] **Step 5: Seam-Check (Review-Focus).** Flashen, echte Karte → `read`: serielle Ausgabe identisch zu vorher (PAN/Expiry/Dump), danach `emuReady == true` im Status (Capture wurde befüllt).
- [ ] **Step 6: Commit.** `git commit -m "NFC: Lesefluss in EmvReader ausgelagert"`

---

### Task 6: `EmvEmulator` auslagern (Capture-Replay, hält `PN532&`)

**Files:**
- Create: `firmware/src/modules/nfc/EmvEmulator.h`, `firmware/src/modules/nfc/EmvEmulator.cpp`
- Modify: `NfcModule.h`/`.cpp` (Emulations-Member entfernen, `EmvEmulator`-Instanz halten, `update()`-Zweig delegieren)

**Interfaces:**
- Consumes: `PN532&`, `const EmvCapture&`, `config.h` (`NFC_EMU_INIT_TIMEOUT_MS`), `PN532_COMMAND_TGINITASTARGET`.
- Produces:
  ```cpp
  class EmvEmulator {
  public:
    explicit EmvEmulator(PN532& reader);
    // Ein Zyklus: wartet bis initTimeoutMs auf einen Leser, beantwortet dann APDUs.
    // uid = 4 Byte für NFCID1. Rückgabe true, wenn ein Leser bedient wurde (-> Reader-Reinit nötig).
    bool runOnce(const EmvCapture& cap, const uint8_t uid[4], uint16_t initTimeoutMs);
  private:
    int     buildResponse(const EmvCapture&, const uint8_t* capdu, int clen, uint8_t* out, int outCap); // war buildEmuResponse
    uint8_t buildSynthPpse(const EmvCapture&, uint8_t* out, uint8_t outCap);
  };
  ```

- [ ] **Step 1:** `EmvEmulator.h`/`.cpp` anlegen; `runEmulation`→`runOnce`, `buildEmuResponse`→`buildResponse`, `buildSynthPpse` **unverändert** übernehmen; Zugriffe auf `_emu*` → Parameter `cap`; das `_emuWaitLogged`-Flag wird Member von `EmvEmulator`; `runOnce` gibt `true` zurück, wenn ein Leser aktiviert wurde.
- [ ] **Step 2:** In `NfcModule`: `EmvEmulator _emulator{_reader};`; der `if (_emulating)`-Zweig in `update()` → `if (_emulator.runOnce(_capture, _emuUid, NFC_EMU_INIT_TIMEOUT_MS)) _needReaderReinit = true;`. `_emuUid` bleibt in `NfcModule` (oder wandert in `EmvCapture` — Entscheidung: in `NfcModule` lassen, da nicht Teil des Mitschnitts). Emulations-Member/-Methoden aus `NfcModule` entfernen. `#include "EmvEmulator.h"`.
- [ ] **Step 3: Build.** Run: `idf.py reconfigure && idf.py build` — Expected: SUCCESS.
- [ ] **Step 4: Commit.** `git commit -m "NFC: Emulation in EmvEmulator ausgelagert"`

---

### Task 7: `NfcModule` als dünner Orchestrator + Abschluss

**Files:**
- Modify: `firmware/src/modules/nfc/NfcModule.h`, `firmware/src/modules/nfc/NfcModule.cpp`
- Modify: `firmware/src/modules/nfc/README.md` (Dateiübersicht ergänzen)

**Interfaces:**
- `NfcModule` hält: `PN532_I2C _transport; PN532 _reader; CardData _card; EmvCapture _capture; EmvReader _emvReader; EmvEmulator _emulator;` + Flags (`_ok, _readPending, _hasPolled, _lastPollMs, _emulating, _emuWaitLogged?→entfällt, _needReaderReinit, _emuUid`). Öffentlich unverändert: `name/begin/update/statusJson/handleCommand`.

- [ ] **Step 1:** `NfcModule.h`/`.cpp` auf die obige Member-Liste reduzieren; prüfen, dass keine verwaisten privaten Deklarationen/Includes übrig sind (`<vector>` nur noch dort nötig, wo `EmvAid`-Listen benutzt werden → in `EmvReader`/`Tlv`, nicht mehr in `NfcModule`).
- [ ] **Step 2:** `README.md` um eine kurze Dateiübersicht ergänzen (EmvTypes/Tlv/EmvTags/EmvReader/EmvEmulator/EmvDump/NfcModule — je eine Zeile Verantwortlichkeit).
- [ ] **Step 3: Build.** Run: `idf.py reconfigure && idf.py build` — Expected: SUCCESS.
- [ ] **Step 4: Manueller Smoke-Test (Gesamt-Verhalten).** Flashen; `read` mit echter Karte → PAN/Expiry/Dump wie vor dem Refactor; `emulate` → Handy-App liest dieselben Werte wie vor dem Refactor; `stop` → zurück im Lesebetrieb; `clear` → Statusfelder leer. Verhalten identisch zum Stand vor Task 1.
- [ ] **Step 5: Commit.** `git commit -m "NFC: NfcModule auf duennen Orchestrator reduziert + Dateiuebersicht"`

---

## Datei-Endzustand (Übersicht)

| Datei | Verantwortlichkeit | Abhängigkeit |
|---|---|---|
| `EmvTypes.h/.cpp` | Datentypen `EmvAid`, `CardData`, `EmvCapture` (+ deren Methoden) | — |
| `Tlv.h/.cpp` | zustandslose TLV-/Byte-Helfer | EmvTypes |
| `EmvTags.h/.cpp` | EMV-Tag-Namen + Tag-Hex-Ausgabe | — |
| `EmvDump.h/.cpp` | annotierter Diagnose-Dump | PN532, Tlv, EmvTags |
| `EmvReader.h/.cpp` | PPSE→AID→GPO→Records, füllt CardData+EmvCapture | PN532, Tlv, EmvTags, EmvDump, EmvTypes |
| `EmvEmulator.h/.cpp` | Capture-Replay (ISO-DEP-Target) | PN532, EmvTypes, config.h |
| `NfcModule.h/.cpp` | Module-Vertrag, Orchestrierung, Befehls-Routing | alle obigen, core/Module.h |

#pragma once

#include "../../core/Module.h"
#include <PN532.h>
#include <PN532_I2C.h>
#include <vector>
#include "EmvTypes.h"
#include "EmvReader.h"

class NfcModule : public Module {
public:
  NfcModule();

  const char* name() const override { return "nfc"; }
  void begin() override;
  void update() override;
  void statusJson(JsonObject obj) override;
  bool handleCommand(const String& action, JsonObjectConst params) override;

private:
  static constexpr uint8_t MaxUidLength = 7;
  // APDU-Antwortpuffer. Bewusst 255 (NICHT 256): Die PN532-Library nimmt die
  // Pufferkapazitaet als uint8_t entgegen (readResponse(buf, uint8_t len, ...)).
  static constexpr size_t MaxBufferLength = 255;

  PN532_I2C _transport;
  PN532 _reader;
  EmvReader _emvReader{_reader};

  bool _ok = false;
  bool _readPending = true;
  bool _hasPolled = false;
  unsigned long _lastPollMs = 0;

  // Letzte ausgelesene Kartendaten
  CardData _card;

  // ------------------------------------------------------------------------
  //  Karten-Emulation (Capture & Replay)
  // ------------------------------------------------------------------------
  //  Beim echten Lesen werden die rohen Kartenantworten mitgeschnitten und bei
  //  "emulate" unveraendert gegenueber einem Leser (Handy-App) zurueckgespielt.
  //  WICHTIG: nur statische Anzeigedaten. KEINE Zahlung moeglich - das
  //  dynamische EMV-Kryptogramm steckt im Secure Element und ist nicht lesbar.
  bool _emulating = false;         // Emulationsmodus aktiv (statt Lesen)?
  bool _emuWaitLogged = false;     // "warte auf Leser" nur einmal melden
  bool _needReaderReinit = false;  // nach Emulation PN532 fuer Lesen neu konfig.

  uint8_t _emuUid[4] = {0x08, 0x00, 0x00, 0x00};
  EmvCapture _capture;

  void runEmulation();
  // Baut aus einer empfangenen C-APDU die Antwort (inkl. SW1 SW2) in out.
  // Rueckgabe: Antwortlaenge (<= outCap, immer >= 2) oder 0 bei Fehler.
  int  buildEmuResponse(const uint8_t* capdu, int clen, uint8_t* out, int outCap);
  // Synthetisiert eine minimale PPSE-FCI aus der gespeicherten AID, falls beim
  // Lesen keine echte PPSE-Antwort anfiel (Fallback-AID-Pfad).
  uint8_t buildSynthPpse(uint8_t* out, uint8_t outCap);
};

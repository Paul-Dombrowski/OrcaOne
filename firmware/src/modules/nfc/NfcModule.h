#pragma once

#include "../../core/Module.h"
#include <PN532.h>
#include <PN532_I2C.h>
#include <vector>
#include "EmvTypes.h"
#include "EmvDump.h"

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
  EmvDump _dump{_reader};

  bool _ok = false;
  bool _readPending = true;
  bool _hasPolled = false;
  unsigned long _lastPollMs = 0;

  // Letzte ausgelesene Kartendaten
  CardData _card;

  void processCard();
  bool selectPpse(std::vector<EmvAid>& aidList);
  // Selektiert eine AID. Bei Erfolg wird das FCI (fuer PDOL-Auswertung)
  // in outFci/outFciLen zurueckgegeben, sofern nicht nullptr.
  bool selectAid(const uint8_t* aid, uint8_t aidLen,
                 uint8_t* outFci = nullptr, uint8_t* outFciLen = nullptr);
  // GPO. Das FCI der zuvor selektierten AID wird uebergeben, damit ein evtl.
  // gefordertes PDOL (Tag 9F38) korrekt gefuellt werden kann.
  // Rueckgabe: true, wenn ein AFL gefunden wurde. outGpo/outGpoLen enthalten
  // bei Erfolg (SW 9000) die rohen GPO-Nutzdaten - bei qVSDC-Karten stehen
  // dort Track 2 / PAN direkt drin (auch ohne AFL).
  bool tryGetProcessingOptions(const uint8_t* fci, uint8_t fciLen,
                               uint8_t* outAfl, uint8_t& outAflLen,
                               uint8_t* outGpo, uint8_t& outGpoLen);
  bool readCardRecords(const uint8_t* afl, uint8_t aflLen);
  bool parseRecordData(CardData& out, const uint8_t* data, uint8_t dataLen);

  // Baut aus der PDOL-Tagliste (Tag+Laenge) die GPO-Datenwerte. Liefert die
  // Gesamtlaenge der geschriebenen Werte, begrenzt durch outCap (Kartendaten
  // sind nicht vertrauenswuerdig). Terminalwerte sind Standard/Dummy; die
  // Karte prueft primaer die Laengen.
  static uint8_t buildPdolData(const uint8_t* pdol, uint8_t pdolLen,
                               uint8_t* out, size_t outCap);

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

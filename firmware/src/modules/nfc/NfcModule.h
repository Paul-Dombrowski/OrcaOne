#pragma once

#include "../../core/Module.h"
#include <PN532.h>
#include <PN532_I2C.h>
#include <vector>
#include "EmvTypes.h"

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

  bool _ok = false;
  bool _readPending = true;
  bool _hasPolled = false;
  unsigned long _lastPollMs = 0;

  // Letzte ausgelesene Kartendaten
  String _lastUid = "";
  String _lastCardNumber = "";
  String _lastExpiry = "";
  String _lastAid = "";
  String _lastCardType = "";
  String _lastCardholder = "";
  String _lastCountryCode = "";
  String _lastCurrencyCode = "";
  String _lastEffectiveDate = "";
  String _lastPsn = "";
  String _lastServiceCode = "";
  String _lastAip = "";
  bool _cardReadSuccess = false;

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
  bool parseRecordData(const uint8_t* data, uint8_t dataLen);

  // Baut aus der PDOL-Tagliste (Tag+Laenge) die GPO-Datenwerte. Liefert die
  // Gesamtlaenge der geschriebenen Werte, begrenzt durch outCap (Kartendaten
  // sind nicht vertrauenswuerdig). Terminalwerte sind Standard/Dummy; die
  // Karte prueft primaer die Laengen.
  static uint8_t buildPdolData(const uint8_t* pdol, uint8_t pdolLen,
                               uint8_t* out, size_t outCap);

  // --- Vollstaendiger TLV-Dump: alle ueber NFC lesbaren Kartendaten ---
  // Dumpt FCI + GPO-Antwort + AFL-Records + GET-DATA-Einzel-Tags annotiert.
  void dumpAllCardData(const uint8_t* fci, uint8_t fciLen,
                       const uint8_t* gpo, uint8_t gpoLen,
                       const uint8_t* afl, uint8_t aflLen, bool aflValid);
  // Rekursiver TLV-Walker: gibt jeden Tag mit Name/Laenge/Wert aus und steigt
  // in konstruierte Templates ab. depth = Einrueckung + Rekursionsschutz.
  void dumpTlv(const uint8_t* data, int dataLen, int depth);
  // Klartext-Dekodierung fuer ausgewaehlte Primitiv-Tags (PAN, Track 2, Datum).
  void decodeHint(uint32_t tag, const uint8_t* val, int len, int depth);
  // Liest alle im AFL referenzierten Records und dumpt sie.
  void readAndDumpRecords(const uint8_t* afl, uint8_t aflLen);
  // GET DATA (80 CA) fuer gaengige Einzel-Tags (ATC, PIN-Zaehler, ...).
  void dumpGetDataTags();
  bool getDataTag(uint32_t tag, uint8_t* out, uint8_t& outLen);

  // ------------------------------------------------------------------------
  //  Karten-Emulation (Capture & Replay)
  // ------------------------------------------------------------------------
  //  Beim echten Lesen werden die rohen Kartenantworten mitgeschnitten und bei
  //  "emulate" unveraendert gegenueber einem Leser (Handy-App) zurueckgespielt.
  //  WICHTIG: nur statische Anzeigedaten. KEINE Zahlung moeglich - das
  //  dynamische EMV-Kryptogramm steckt im Secure Element und ist nicht lesbar.
  static constexpr uint8_t EmuMaxRecords = 8;
  static constexpr uint8_t EmuBlobCap = 255;  // tgSetData-Laenge ist uint8_t

  struct EmuRecord {
    uint8_t sfi;
    uint8_t rec;
    uint8_t len;
    uint8_t data[EmuBlobCap];
  };

  bool _emulating = false;         // Emulationsmodus aktiv (statt Lesen)?
  bool _emuValid = false;          // Liegt ein brauchbarer Mitschnitt vor?
  bool _emuWaitLogged = false;     // "warte auf Leser" nur einmal melden
  bool _needReaderReinit = false;  // nach Emulation PN532 fuer Lesen neu konfig.

  uint8_t _emuUid[4] = {0x08, 0x00, 0x00, 0x00};
  uint8_t _emuPpse[EmuBlobCap]; uint8_t _emuPpseLen = 0;
  uint8_t _emuAid[16];          uint8_t _emuAidLen = 0;
  uint8_t _emuFci[EmuBlobCap];  uint8_t _emuFciLen = 0;
  uint8_t _emuGpo[EmuBlobCap];  uint8_t _emuGpoLen = 0;
  EmuRecord _emuRecords[EmuMaxRecords]; uint8_t _emuRecordCount = 0;

  void resetEmuCapture();
  void captureBlob(uint8_t* dst, uint8_t& dstLen, const uint8_t* src, int srcLen);
  void captureRecord(uint8_t sfi, uint8_t rec, const uint8_t* data, int len);
  void runEmulation();
  // Baut aus einer empfangenen C-APDU die Antwort (inkl. SW1 SW2) in out.
  // Rueckgabe: Antwortlaenge (<= outCap, immer >= 2) oder 0 bei Fehler.
  int  buildEmuResponse(const uint8_t* capdu, int clen, uint8_t* out, int outCap);
  // Synthetisiert eine minimale PPSE-FCI aus der gespeicherten AID, falls beim
  // Lesen keine echte PPSE-Antwort anfiel (Fallback-AID-Pfad).
  uint8_t buildSynthPpse(uint8_t* out, uint8_t outCap);
};

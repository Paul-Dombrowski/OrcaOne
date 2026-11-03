#pragma once

#include <Arduino.h>
#include <PN532.h>
#include "Tlv.h"
#include "EmvTags.h"

// Diagnose-Dump: alle ueber NFC lesbaren Kartendaten annotiert auf Serial.
class EmvDump {
public:
  explicit EmvDump(PN532& reader) : _reader(reader) {}

  // Dumpt FCI + GPO-Antwort + AFL-Records + GET-DATA-Einzel-Tags annotiert.
  void dumpAll(const uint8_t* fci, uint8_t fciLen,
               const uint8_t* gpo, uint8_t gpoLen,
               const uint8_t* afl, uint8_t aflLen, bool aflValid);

private:
  // APDU-Antwortpuffer. Bewusst 255 (NICHT 256): Die PN532-Library nimmt die
  // Pufferkapazitaet als uint8_t entgegen (readResponse(buf, uint8_t len, ...)).
  static constexpr size_t MaxBufferLength = 255;

  PN532& _reader;

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
};

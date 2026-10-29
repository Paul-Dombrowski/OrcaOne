#pragma once

#include <Arduino.h>
#include <vector>
#include <PN532.h>
#include "EmvTypes.h"
#include "Tlv.h"
#include "EmvTags.h"
#include "EmvDump.h"

// Liest eine kontaktlose EMV-Karte aus: PPSE -> AID -> GPO -> Records.
class EmvReader {
public:
  explicit EmvReader(PN532& reader) : _reader(reader) {}

  // Fuellt out + cap. Rueckgabe: true, wenn Kartendaten gelesen wurden.
  bool readCard(CardData& out, EmvCapture& cap);

private:
  PN532& _reader;
  EmvDump _dump{_reader};

  // APDU-Antwortpuffer. Bewusst 255 (NICHT 256): Die PN532-Library nimmt die
  // Pufferkapazitaet als uint8_t entgegen (readResponse(buf, uint8_t len, ...)).
  static constexpr size_t MaxBufferLength = 255;

  bool selectPpse(std::vector<EmvAid>& aidList, EmvCapture& cap);
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
  bool readCardRecords(const uint8_t* afl, uint8_t aflLen,
                       CardData& card, EmvCapture& cap);
  bool parseRecordData(CardData& out, const uint8_t* data, uint8_t dataLen);

  // Baut aus der PDOL-Tagliste (Tag+Laenge) die GPO-Datenwerte. Liefert die
  // Gesamtlaenge der geschriebenen Werte, begrenzt durch outCap (Kartendaten
  // sind nicht vertrauenswuerdig). Terminalwerte sind Standard/Dummy; die
  // Karte prueft primaer die Laengen.
  static uint8_t buildPdolData(const uint8_t* pdol, uint8_t pdolLen,
                               uint8_t* out, size_t outCap);
};

#pragma once

#include "../../core/Module.h"
#include <PN532.h>
#include <PN532_I2C.h>
#include <vector>

struct EmvAid {
  uint8_t aid[16];
  uint8_t len;
  char label[24];
  uint8_t priority;
};

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
  // Bei 256 wird "uint8_t responseLength = sizeof(response)" still auf 0
  // verengt -> inDataExchange() uebergibt Kapazitaet 0 -> jede APDU-Antwort
  // (PPSE, SELECT AID, GPO, READ RECORD) scheitert mit "keine Antwort".
  // 255 ist zugleich die API-Obergrenze und fuer EMV-Einzelframes ausreichend.
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
  bool _cardReadSuccess = false;

  void processCard();
  bool selectPpse(std::vector<EmvAid>& aidList);
  bool selectAid(const uint8_t* aid, uint8_t aidLen);
  bool tryGetProcessingOptions(uint8_t* outAfl, uint8_t& outAflLen);
  bool readCardRecords(const uint8_t* afl, uint8_t aflLen);
  bool parseRecordData(const uint8_t* data, uint8_t dataLen);

  // TLV Parser
  static int findTag(const uint8_t* data, int dataLen, uint32_t tag);
  static void extractAllAids(const uint8_t* data, int dataLen, std::vector<EmvAid>& aidList);
  static void parseTrack2(const uint8_t* track2Data, uint8_t track2Len, String& outPan, String& outExp);
  static void printHex(const uint8_t* data, uint8_t length);
};

#pragma once

#include "../../core/Module.h"
#include <PN532.h>
#include <PN532_I2C.h>

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
  static constexpr uint8_t FirstRawPage = 4;
  static constexpr uint8_t LastRawPage = 39;
  static constexpr uint8_t MaxRawResponseLength = 32;
  static constexpr uint8_t MaxActivationResponseLength = 32;

  PN532_I2C _transport;
  PN532 _reader;
  bool _ok = false;
  bool _readPending = false;
  bool _rawDumpPending = false;
  bool _hasPolled = false;
  unsigned long _lastPollMs = 0;
  uint8_t _nextRawPage = FirstRawPage;

  void dumpNextRawPages();
  void printHex(const uint8_t* data, uint8_t length);
};

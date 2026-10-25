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

  PN532_I2C _transport;
  PN532 _reader;
  bool _ok = false;
  bool _readPending = false;
  bool _tagPresent = false;
  bool _hasPolled = false;
  unsigned long _lastPollMs = 0;
  String _lastUid;
};

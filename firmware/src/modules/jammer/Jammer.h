#pragma once

#include "../../core/Module.h"
#include <RF24.h>

class JammerModule : public Module {
public:
  JammerModule();

  const char* name() const override { return "jammer"; }
  void begin() override;
  void update() override;
  void statusJson(JsonObject obj) override;
  bool handleCommand(const String& action, JsonObjectConst params) override;

private:
  void updateHop();

  bool _ok = false;
  bool _active = false;
  bool _direction = true;
  uint8_t _channel = 45;
  uint32_t _lastHopMs = 0;

  RF24 _radio;
};
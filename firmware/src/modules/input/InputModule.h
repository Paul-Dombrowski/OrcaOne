#pragma once

#include "../../core/Module.h"
#include "../../core/InputEvent.h"

class InputModule : public Module {
public:
  const char* name() const override { return "input"; }
  void begin() override;
  void update() override;
  void statusJson(JsonObject obj) override;
  bool handleCommand(const String& action, JsonObjectConst params) override;

  InputEvent poll();

private:
  static constexpr uint8_t kQueueCapacity = 8;
  static constexpr unsigned long kDebounceMs = 30;
  static constexpr unsigned long kLongPressMs = 600;

  void enqueue(InputEvent event);
  int8_t readEncoderStep();

  bool _ok = true;
  uint8_t _previousEncoder = 0;
  int8_t _encoderAccumulator = 0;
  bool _buttonRawHigh = true;
  bool _buttonStableHigh = true;
  bool _longPressSent = false;
  unsigned long _buttonChangedAt = 0;
  unsigned long _buttonPressedAt = 0;
  InputEvent _events[kQueueCapacity]{};
  uint8_t _queueHead = 0;
  uint8_t _queueTail = 0;
  uint8_t _queueSize = 0;
};

#include "InputModule.h"

#include "../../config.h"

namespace {
const int8_t kEncoderTransitions[16] = {
    0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
}

void InputModule::begin() {
  pinMode(PIN_ENCODER_CLK, INPUT_PULLUP);
  pinMode(PIN_ENCODER_DT, INPUT_PULLUP);
  pinMode(PIN_ENCODER_SW, INPUT_PULLUP);

  _previousEncoder =
      (static_cast<uint8_t>(digitalRead(PIN_ENCODER_CLK)) << 1) |
      static_cast<uint8_t>(digitalRead(PIN_ENCODER_DT));
  _buttonRawHigh = digitalRead(PIN_ENCODER_SW) == HIGH;
  _buttonStableHigh = _buttonRawHigh;
  _buttonChangedAt = millis();
  _buttonPressedAt = _buttonStableHigh ? 0 : _buttonChangedAt;
  _ok = true;
  Serial.println("[input] Drehgeber bereit.");
}

void InputModule::update() {
  if (!_ok) return;

  const int8_t step = readEncoderStep();
  if (step > 0) enqueue(InputEvent::Down);
  if (step < 0) enqueue(InputEvent::Up);

  const unsigned long now = millis();
  const bool rawHigh = digitalRead(PIN_ENCODER_SW) == HIGH;
  if (rawHigh != _buttonRawHigh) {
    _buttonRawHigh = rawHigh;
    _buttonChangedAt = now;
  }

  if (_buttonRawHigh != _buttonStableHigh &&
      now - _buttonChangedAt >= kDebounceMs) {
    _buttonStableHigh = _buttonRawHigh;
    if (!_buttonStableHigh) {
      _buttonPressedAt = now;
      _longPressSent = false;
    } else if (!_longPressSent) {
      enqueue(InputEvent::Click);
    }
  }

  if (!_buttonStableHigh && !_longPressSent &&
      now - _buttonPressedAt >= kLongPressMs) {
    _longPressSent = true;
    enqueue(InputEvent::LongPress);
  }
}

void InputModule::statusJson(JsonObject obj) {
  obj["ok"] = _ok;
  obj["queuedEvents"] = _queueSize;
}

bool InputModule::handleCommand(const String& action, JsonObjectConst params) {
  (void)params;
  if (!_ok) return false;

  if (action == "up") {
    enqueue(InputEvent::Up);
  } else if (action == "down") {
    enqueue(InputEvent::Down);
  } else if (action == "ok") {
    enqueue(InputEvent::Click);
  } else if (action == "back") {
    enqueue(InputEvent::LongPress);
  } else {
    return false;
  }
  return true;
}

InputEvent InputModule::poll() {
  if (_queueSize == 0) return InputEvent::None;

  const InputEvent event = _events[_queueHead];
  _queueHead = (_queueHead + 1) % kQueueCapacity;
  --_queueSize;
  return event;
}

void InputModule::enqueue(InputEvent event) {
  if (_queueSize == kQueueCapacity) {
    Serial.println("[input] Eingabe verworfen: Ereigniswarteschlange voll");
    return;
  }
  _events[_queueTail] = event;
  _queueTail = (_queueTail + 1) % kQueueCapacity;
  ++_queueSize;
}

int8_t InputModule::readEncoderStep() {
  _previousEncoder =
      ((_previousEncoder << 2) | (digitalRead(PIN_ENCODER_CLK) << 1) |
       digitalRead(PIN_ENCODER_DT)) &
      0x0F;
  _encoderAccumulator += kEncoderTransitions[_previousEncoder];
  if (_encoderAccumulator >= 4) {
    _encoderAccumulator = 0;
    return 1;
  }
  if (_encoderAccumulator <= -4) {
    _encoderAccumulator = 0;
    return -1;
  }
  return 0;
}

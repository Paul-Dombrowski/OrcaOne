#include "InputModule.h"

#include "../../config.h"
#include <driver/gpio.h>

namespace {
// Quadratur-Uebergangstabelle (4-Bit-Index aus alt<<2|neu). In DRAM, damit die
// IRAM-ISR sie auch waehrend einer Flash-Operation sicher lesen kann.
DRAM_ATTR const int8_t kEncoderTransitions[16] = {
    0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

// Trampolin: attachInterruptArg uebergibt das InputModule als arg.
void encoderIsrTrampoline(void* arg) {
  static_cast<InputModule*>(arg)->onEncoderEdge();
}
}  // namespace

void InputModule::onEncoderEdge() {
  const uint8_t clk = gpio_get_level(static_cast<gpio_num_t>(PIN_ENCODER_CLK));
  const uint8_t dt = gpio_get_level(static_cast<gpio_num_t>(PIN_ENCODER_DT));
  _previousEncoder =
      ((_previousEncoder << 2) | (clk << 1) | dt) & 0x0F;
  _encoderAccumulator += kEncoderTransitions[_previousEncoder];
  if (_encoderAccumulator >= 4) {
    _encoderAccumulator = 0;
    _netSteps = _netSteps + 1;  // explizit statt ++ (volatile: -Wvolatile)
  } else if (_encoderAccumulator <= -4) {
    _encoderAccumulator = 0;
    _netSteps = _netSteps - 1;
  }
}

void InputModule::begin() {
  pinMode(PIN_ENCODER_CLK, INPUT_PULLUP);
  pinMode(PIN_ENCODER_DT, INPUT_PULLUP);
  pinMode(PIN_ENCODER_SW, INPUT_PULLUP);

  _previousEncoder =
      (static_cast<uint8_t>(digitalRead(PIN_ENCODER_CLK)) << 1) |
      static_cast<uint8_t>(digitalRead(PIN_ENCODER_DT));
  _encoderAccumulator = 0;
  _netSteps = 0;

  // Beide Drehgeber-Signale auf Flanken-Interrupt. 'this' als Argument, damit
  // die ISR die Instanz-Member fuellt.
  attachInterruptArg(PIN_ENCODER_CLK, encoderIsrTrampoline, this, CHANGE);
  attachInterruptArg(PIN_ENCODER_DT, encoderIsrTrampoline, this, CHANGE);

  _buttonRawHigh = digitalRead(PIN_ENCODER_SW) == HIGH;
  _buttonStableHigh = _buttonRawHigh;
  _buttonChangedAt = millis();
  _buttonPressedAt = _buttonStableHigh ? 0 : _buttonChangedAt;
  _ok = true;
  Serial.println("[input] Drehgeber bereit (Interrupt).");
}

void InputModule::update() {
  if (!_ok) return;

  // Netto-Drehschritte aus dem ISR atomar abholen und in Events umsetzen.
  noInterrupts();
  int16_t steps = _netSteps;
  _netSteps = 0;
  interrupts();

  // Richtung: positiv -> Auswahl hoch. Falls am Geraet falsch herum, nur dieses
  // Vorzeichen-Mapping tauschen (Up<->Down).
  while (steps > 0) {
    enqueue(InputEvent::Up);
    --steps;
  }
  while (steps < 0) {
    enqueue(InputEvent::Down);
    ++steps;
  }

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

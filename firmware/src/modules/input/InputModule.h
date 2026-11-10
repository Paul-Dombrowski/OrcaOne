#pragma once

#include "../../core/Module.h"
#include "../../core/InputEvent.h"

// ============================================================================
//  InputModule - Drehgeber KY-040 (Menue-Navigation)
// ----------------------------------------------------------------------------
//  Drehung wird per HARDWARE-INTERRUPT dekodiert (ISR), NICHT gepollt. Grund:
//  Ein gepollter Quadratur-Decoder ist nur so flink wie der langsamste
//  loop()-Durchlauf. Sobald ein Modul blockiert (IR sendRaw, NFC-Polling),
//  gehen Rastungen verloren -> "Drehgeber reagiert nach einer Aktion nicht
//  mehr". Der Interrupt erfasst jede Flanke unabhaengig von loop().
//
//  Der Taster bleibt gepollt (Entprellung ist zeitbasiert, unempfindlich
//  gegen gelegentliche loop()-Verzoegerung).
// ============================================================================
class InputModule : public Module {
public:
  const char* name() const override { return "input"; }
  void begin() override;
  void update() override;
  void statusJson(JsonObject obj) override;
  bool handleCommand(const String& action, JsonObjectConst params) override;

  InputEvent poll();

  // ISR-Callback: dekodiert EINE Drehgeber-Flanke. Oeffentlich, weil die
  // Trampolin-Funktion auf Dateiebene sie aufruft. Nicht direkt verwenden.
  // KEIN IRAM_ATTR noetig: Arduino installiert den GPIO-ISR-Service ohne
  // ESP_INTR_FLAG_IRAM (CONFIG_ARDUINO_ISR_IRAM aus) -> bei Flash-Operationen
  // wird der Interrupt sicher verzoegert statt aus dem Flash ausgefuehrt.
  void onEncoderEdge();

private:
  static constexpr uint8_t kQueueCapacity = 8;
  static constexpr unsigned long kDebounceMs = 30;
  static constexpr unsigned long kLongPressMs = 600;

  void enqueue(InputEvent event);

  bool _ok = true;

  // --- Drehgeber (per Interrupt dekodiert) ---
  // Nur im ISR benutzt (ein Kontext) -> nicht volatile noetig:
  uint8_t _previousEncoder = 0;    // 4-Bit-Historie der CLK/DT-Zustaende
  int8_t _encoderAccumulator = 0;  // sammelt Teilschritte bis zur Rastung
  // Zwischen ISR und update() geteilt -> volatile:
  volatile int16_t _netSteps = 0;  // fertige Rastungen (+/-), vom ISR erzeugt

  // --- Taster (gepollt) ---
  bool _buttonRawHigh = true;
  bool _buttonStableHigh = true;
  bool _longPressSent = false;
  unsigned long _buttonChangedAt = 0;
  unsigned long _buttonPressedAt = 0;

  // --- Ereignis-Warteschlange (nur Loop-Kontext) ---
  InputEvent _events[kQueueCapacity]{};
  uint8_t _queueHead = 0;
  uint8_t _queueTail = 0;
  uint8_t _queueSize = 0;
};

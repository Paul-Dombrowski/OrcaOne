#pragma once

#include "../../core/Module.h"
#include "../../config.h"   // JAM_CH_MID fuer den _channel2-Default
#include <RF24.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class JammerModule : public Module {
public:
  JammerModule();

  const char* name() const override { return "jammer"; }
  void begin() override;
  void update() override;
  void statusJson(JsonObject obj) override;
  bool handleCommand(const String& action, JsonObjectConst params) override;

private:
  // Sweep-Task: schaltet den Traeger EINMAL an und stimmt dann das Band
  // dauerhaft flat-out durch. Laeuft auf einem eigenen Core, damit loop()
  // und die Serial-Konsole bedienbar bleiben (update() bleibt leer).
  static void sweepTask(void* arg);

  bool _ok = false;
  // _active bleibt true, bis der Task wirklich ausgelaufen ist (der Task nullt
  // es am Ende selbst). So blockiert die start-Guard ueberlappende Tasks bei
  // schnellem stop->start. volatile: wird vom Task auf dem anderen Core gesetzt.
  volatile bool _active = false;
  volatile bool _running = false;   // Laufsignal an den Sweep-Task
  volatile uint8_t _channel = 45;   // aktueller Kanal (Task schreibt, Status liest)

  // Zweites NRF24 (Option A): selber VSPI-Bus, eigenes CE/CSN. _ok2 false, wenn
  // nicht verdrahtet -> der Sweep-Task faellt auf Einzel-Radio-Voll-Sweep zurueck.
  volatile bool _ok2 = false;
  volatile uint8_t _channel2 = JAM_CH_MID;  // aktueller Kanal Radio 2 (Status liest)

  RF24 _radio;
  RF24 _radio2;
};

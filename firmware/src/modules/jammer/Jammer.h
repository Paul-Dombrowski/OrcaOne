#pragma once

#include "../../core/Module.h"
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

  RF24 _radio;
};

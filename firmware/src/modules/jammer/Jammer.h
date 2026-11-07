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
  bool _active = false;
  volatile bool _running = false;    // Laufsignal fuer den Sweep-Task
  volatile uint8_t _channel = 45;    // aktueller Kanal (Task schreibt, Status liest)
  volatile TaskHandle_t _taskHandle = nullptr;  // vom Task (anderer Core) genullt

  RF24 _radio;
};
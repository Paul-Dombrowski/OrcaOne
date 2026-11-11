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

  // Stoerprofil: welcher Kanalbereich gesweept wird.
  //   PROF_FULL  - ganzes Band 0..80 (Bluetooth/AFH; auch No-Mercy)
  //   PROF_WIFI  - nur die drei 20-MHz-WLAN-Bloecke (dichter), siehe config.h
  enum JamProfile { PROF_FULL, PROF_WIFI };

private:
  // Sweep-Task: schaltet den Traeger EINMAL an und stimmt dann das Band
  // dauerhaft flat-out durch. Laeuft auf einem eigenen Core, damit loop()
  // und die Serial-Konsole bedienbar bleiben (update() bleibt leer).
  static void sweepTask(void* arg);

  // Index (0..JAM_WIFI_CH_COUNT-1) -> NRF-Kanal innerhalb der drei WLAN-Bloecke.
  static uint8_t wifiChannelAt(uint16_t idx);

  // Kapselt die Start-Sequenz (WLAN aus, Task erzeugen); idempotent: laeuft der
  // Jammer schon, nur true zurueckgeben (kein zweiter Task). Das Profil setzt
  // der Aufrufer (handleCommand) vorher.
  bool startJamming();

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

  // Aktives Profil. Befehl schreibt (loop()-Task), Sweep-Task liest je Durchlauf
  // -> Profilwechsel wirkt ohne Neustart (Traeger bleibt an). volatile wie _channel.
  volatile JamProfile _profile = PROF_FULL;

  // Diagnose: wie oft der Sweep-Task pro Sekunde das (halbe) Band komplett
  // durchstimmt. = wie oft jeder Kanal einer Bandhaelfte je Sekunde getroffen
  // wird. Task schreibt (1x/s), statusJson liest (atomarer uint32-Read).
  volatile uint32_t _sweepHz = 0;

  RF24 _radio;
  RF24 _radio2;
};

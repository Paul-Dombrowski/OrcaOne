#include "Jammer.h"

#include <SPI.h>

#include "../../config.h"

JammerModule::JammerModule()
    // 16 MHz SPI (statt Default 10 MHz): schnellere setChannel()-Schreibzugriffe
    // -> dichterer Sweep. Wert aus dem erprobten Standalone-Sketch. Bei
    // Instabilitaet auf 10000000 reduzieren.
    : _radio(PIN_NRF_CE, PIN_NRF_CSN, 16000000) {}

void JammerModule::begin() {
  _ok = false;
  _active = false;
  _running = false;
  _channel = 45;

  pinMode(PIN_NRF_CE, OUTPUT);
  pinMode(PIN_NRF_CSN, OUTPUT);
  pinMode(PIN_NRF_SCK, OUTPUT);
  pinMode(PIN_NRF_MISO, INPUT);
  pinMode(PIN_NRF_MOSI, OUTPUT);
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);

  // SPI-Bus wird ZENTRAL in main.cpp (setup(), vor Storage.begin()) mit den
  // NRF-Pins initialisiert - hier NICHT erneut SPI.begin() aufrufen, sonst
  // greift der SPIClass-Erstzugriff ins Leere (if(_spi) return true;).
  if (!_radio.begin(&SPI)) {
    Serial.println("[jammer] Keine SPI-Antwort vom NRF24 - Modul inaktiv");
    return;
  }

  _radio.setAutoAck(false);
  _radio.setRetries(0, 0);
  _radio.setPayloadSize(5);
  _radio.setAddressWidth(3);
  _radio.setPALevel(RF24_PA_MAX, true);
  _radio.setDataRate(RF24_2MBPS);
  _radio.setCRCLength(RF24_CRC_DISABLED);
  _radio.stopListening();
  _radio.setChannel(_channel);

  _ok = true;
  Serial.println("[jammer] NRF24 bereit");
}

void JammerModule::update() {
  // Die Stoerlogik laeuft im Sweep-Task (siehe sweepTask), NICHT hier.
  // update() muss nicht-blockierend sein (ARBEITSWEISE §6) - daher leer.
}

void JammerModule::sweepTask(void* arg) {
  auto* self = static_cast<JammerModule*>(arg);

  // Traeger EINMAL anschalten, danach nur noch den Kanal umstimmen (wie im
  // erprobten Standalone). NICHT pro Kanal neu starten.
  self->_radio.startConstCarrier(RF24_PA_MAX, self->_channel);

  // TEMP-DIAGNOSE: beweist, dass der Task wirklich laeuft und den Traeger-Code
  // erreicht (nach Klaerung wieder entfernen).
  Serial.printf("[jammer][dbg] sweepTask laeuft auf core %d, Traeger an ab ch=%u\n",
                xPortGetCoreID(), (unsigned)self->_channel);

  uint32_t lastYield = millis();
  uint32_t lastReport = millis();
  uint32_t sweeps = 0;
  while (self->_running) {
    for (uint8_t ch = JAM_CH_MIN; ch <= JAM_CH_MAX; ++ch) {
      self->_radio.setChannel(ch);   // flat-out umstimmen, kein Delay
    }
    self->_channel = JAM_CH_MAX;
    ++sweeps;

    // TEMP-DIAGNOSE: wie viele volle Band-Durchlaeufe pro Sekunde?
    if (millis() - lastReport >= 1000) {
      Serial.printf("[jammer][dbg] %lu sweeps/s\n", (unsigned long)sweeps);
      sweeps = 0;
      lastReport = millis();
    }

    // Kurz yielden, damit der Idle-Task auf diesem Core laeuft und den
    // Task-Watchdog bedient. ~<2 % Sendezeit, HF-technisch vernachlaessigbar
    // (der Standalone yieldet effektiv einmal pro loop()-Durchlauf).
    if (millis() - lastYield >= JAM_WDT_YIELD_MS) {
      vTaskDelay(1);
      lastYield = millis();
    }
  }

  // Sauberer Aus-Zustand: Traeger NUR hier (im Task-Kontext) abschalten, nie
  // parallel aus stop()/loop() -> kein gleichzeitiger SPI-Zugriff. Erst danach
  // _active freigeben, damit ein neuer start() nicht ueberlappend startet.
  self->_radio.stopConstCarrier();
  self->_active = false;
  vTaskDelete(nullptr);
}

void JammerModule::statusJson(JsonObject obj) {
  obj["ok"] = _ok;
  obj["active"] = _active;
  obj["channel"] = _channel;
  obj["mode"] = _active ? "jamming" : "idle";
}

bool JammerModule::handleCommand(const String& action, JsonObjectConst params) {
  (void)params;

  if (!_ok) {
    return false;
  }

  if (action == "start") {
    if (_active) {
      // schon aktiv (oder ein vorheriger Task laeuft noch aus) -> kein zweiter
      // Task/Traeger, sonst gleichzeitiger SPI-Zugriff. Direkt nach "stop"
      // ggf. "start" kurz erneut senden.
      return true;
    }
    _running = true;
    _active = true;
    const BaseType_t ok =
        xTaskCreatePinnedToCore(&JammerModule::sweepTask, "jam",
                                JAM_TASK_STACK, this, JAM_TASK_PRIORITY,
                                nullptr, JAM_TASK_CORE);
    if (ok != pdPASS) {
      _running = false;
      _active = false;
      Serial.println("[jammer] Task konnte nicht gestartet werden");
      return false;
    }
    Serial.println("[jammer] Störung aktiviert");
    return true;
  }

  if (action == "stop") {
    _running = false;   // Task laeuft aus, schaltet Traeger ab und nullt _active
    Serial.println("[jammer] Störung deaktiviert");
    return true;
  }

  return false;
}

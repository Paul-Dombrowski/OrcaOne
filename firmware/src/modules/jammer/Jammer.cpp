#include "Jammer.h"

#include <SPI.h>

#include "../../config.h"
#include "../../network/NetworkService.h"  // Net.suspend()/resume(): WLAN-Funk
                                           // waehrend des Jammens aus (gemeinsamer
                                           // Dienst wie Storage - kein Modul↔Modul)

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

  // WICHTIG: KEIN pinMode() auf SCK/MISO/MOSI! Diese Pins gehoeren dem SPI-
  // Treiber - SPI.begin() (main.cpp) hat sie an die SPI-Hardware geroutet. Ein
  // pinMode() hier wuerde sie in der arduino-esp32-GPIO-Matrix wieder von der
  // SPI-Peripherie LOESEN -> NRF24 bekaeme keinen Takt/Daten und antwortet
  // nicht ("Keine SPI-Antwort"). CE/CSN setzt der RF24-Treiber in begin()
  // selbst. Nur die SD wird abgewaehlt, solange der NRF24 den Bus nutzt.
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
  _radio.setPALevel(JAM_PA_LEVEL, true);
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
  self->_radio.startConstCarrier(JAM_PA_LEVEL, self->_channel);

  uint32_t lastYield = millis();
  while (self->_running) {
    for (uint8_t ch = JAM_CH_MIN; ch <= JAM_CH_MAX; ++ch) {
      self->_radio.setChannel(ch);   // flat-out umstimmen, kein Delay
    }
    self->_channel = JAM_CH_MAX;

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
    // WLAN AUS, bevor der Traeger kommt: sonst funkt der ESP32-WLAN-Sender im
    // selben 2,4-GHz-Band gegen den Jammer und seine TX-Spitzen lassen die 3,3 V
    // einbrechen (NRF24-PA ausgehungert). Hier im loop()-Task -> single-threaded
    // zu Net.update(). Erklaert, warum der Standalone jammt, die integrierte
    // Version mit laufendem WLAN bisher nicht.
    Net.suspend();
    _running = true;
    _active = true;
    const BaseType_t ok =
        xTaskCreatePinnedToCore(&JammerModule::sweepTask, "jam",
                                JAM_TASK_STACK, this, JAM_TASK_PRIORITY,
                                nullptr, JAM_TASK_CORE);
    if (ok != pdPASS) {
      _running = false;
      _active = false;
      Net.resume();  // Taskstart fehlgeschlagen -> WLAN sofort wieder hoch
      Serial.println("[jammer] Task konnte nicht gestartet werden");
      return false;
    }
    Serial.println("[jammer] Störung aktiviert");
    return true;
  }

  if (action == "stop") {
    _running = false;   // Task laeuft aus, schaltet Traeger ab und nullt _active
    Net.resume();       // WLAN-Hotspot + Webserver wieder hochfahren
    Serial.println("[jammer] Störung deaktiviert");
    return true;
  }

  return false;
}

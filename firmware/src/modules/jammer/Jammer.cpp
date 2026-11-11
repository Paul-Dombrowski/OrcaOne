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
    // Radio 2 (Option A) am selben Bus, eigenes CE/CSN, ebenfalls 16 MHz.
    : _radio(PIN_NRF_CE, PIN_NRF_CSN, 16000000),
      _radio2(PIN_NRF2_CE, PIN_NRF2_CSN, 16000000) {}

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
  // CSN-Settle-Delay (Default 5 us) raus: ohne AutoAck/ohne Empfang unnoetig.
  // Spart ~10 us je setChannel (2x CSN) -> deutlich dichterer Sweep (sweepHz).
  _radio.csDelay = 0;

  _ok = true;
  Serial.println("[jammer] NRF24 bereit");

  // Zweites NRF24 am SELBEN Bus (eigenes CE/CSN) erkennen und - bei Erfolg -
  // IDENTISCH zu Radio 1 konfigurieren. Sequentiell nach Radio 1, vor dem
  // Task-Start -> kein nebenlaeufiger Bus-Zugriff. Fehlt es, laeuft der Jammer
  // einarmig weiter (Voll-Sweep, siehe sweepTask).
  if (_radio2.begin(&SPI)) {
    _radio2.setAutoAck(false);
    _radio2.setRetries(0, 0);
    _radio2.setPayloadSize(5);
    _radio2.setAddressWidth(3);
    _radio2.setPALevel(JAM_PA_LEVEL, true);
    _radio2.setDataRate(RF24_2MBPS);
    _radio2.setCRCLength(RF24_CRC_DISABLED);
    _radio2.stopListening();
    _radio2.setChannel(_channel2);
    _radio2.csDelay = 0;   // wie Radio 1: CSN-Delay raus -> schnellerer Sweep
    _ok2 = true;
    Serial.println("[jammer] NRF24 #2 bereit (Dual-Antenne, Bandteilung)");
  } else {
    _ok2 = false;
    Serial.println("[jammer] NRF24 #2 nicht gefunden - Einzel-Antenne, Voll-Sweep");
  }
}

void JammerModule::update() {
  // Die Stoerlogik laeuft im Sweep-Task (siehe sweepTask), NICHT hier.
  // update() muss nicht-blockierend sein (ARBEITSWEISE §6) - daher leer.
}

void JammerModule::sweepTask(void* arg) {
  auto* self = static_cast<JammerModule*>(arg);

  // Traeger EINMAL anschalten, danach nur noch den Kanal umstimmen (wie im
  // erprobten Standalone). NICHT pro Kanal neu starten. Radio 2 nur, wenn
  // vorhanden -> Bandteilung: Radio 1 untere Haelfte, Radio 2 obere Haelfte,
  // zwei Traeger gleichzeitig in der Luft = doppelte parallele Abdeckung (AFH).
  self->_radio.startConstCarrier(JAM_PA_LEVEL, self->_channel);
  Serial.printf("[jammer] Radio 1 Traeger an (Start-CH %u)\n", self->_channel);
  if (self->_ok2) {
    self->_radio2.startConstCarrier(JAM_PA_LEVEL, self->_channel2);
    Serial.printf("[jammer] Radio 2 Traeger an (Start-CH %u) - Dual aktiv\n",
                  self->_channel2);
  } else {
    Serial.println("[jammer] nur Radio 1 (kein Radio 2) - Voll-Sweep");
  }

  // Diagnose-Zaehler: Sweep-Durchlaeufe seit der letzten Sekunde.
  uint32_t iter = 0;
  uint32_t lastHzCalc = millis();

  uint32_t lastYield = millis();
  while (self->_running) {
    if (self->_ok2) {
      // Gleichschritt: beide Radios pro Iteration fast gleichzeitig umstimmen.
      // Radio 1: JAM_CH_MIN..JAM_CH_MID, Radio 2: JAM_CH_MID..JAM_CH_MAX
      // (gleich viele Schritte; Ueberlappung auf JAM_CH_MID ist unschaedlich).
      for (uint8_t i = 0; i <= (JAM_CH_MID - JAM_CH_MIN); ++i) {
        self->_radio.setChannel(JAM_CH_MIN + i);
        self->_radio2.setChannel(JAM_CH_MID + i);
      }
      self->_channel = JAM_CH_MID;
      self->_channel2 = JAM_CH_MAX;
    } else {
      // Fallback ohne Radio 2: ein Radio sweept das ganze Band (wie bisher).
      for (uint8_t ch = JAM_CH_MIN; ch <= JAM_CH_MAX; ++ch) {
        self->_radio.setChannel(ch);   // flat-out umstimmen, kein Delay
      }
      self->_channel = JAM_CH_MAX;
    }

    // Sweep-Rate messen: einmal pro Sekunde die Durchlaufzahl nach _sweepHz
    // spiegeln (so sieht man im status, ob/wie schnell der Task laeuft).
    ++iter;
    if (millis() - lastHzCalc >= 1000) {
      self->_sweepHz = iter;
      iter = 0;
      lastHzCalc += 1000;
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
  // parallel aus stop()/loop() -> kein gleichzeitiger SPI-Zugriff. Radio 2 nur,
  // wenn vorhanden. Erst danach _active freigeben, damit ein neuer start() nicht
  // ueberlappend startet.
  self->_radio.stopConstCarrier();
  if (self->_ok2) {
    self->_radio2.stopConstCarrier();
  }
  self->_sweepHz = 0;   // Diagnose zuruecksetzen (nichts laeuft mehr)
  self->_active = false;
  vTaskDelete(nullptr);
}

void JammerModule::statusJson(JsonObject obj) {
  obj["ok"] = _ok;
  obj["ok2"] = _ok2;
  obj["active"] = _active;
  obj["channel"] = _channel;
  obj["channel2"] = _channel2;
  obj["sweepHz"] = _sweepHz;   // Durchlaeufe/s (0 = Task laeuft nicht)
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

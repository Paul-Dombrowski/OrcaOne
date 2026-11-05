#include "Jammer.h"

#include "../../config.h"

namespace {
constexpr uint8_t kMinChannel = 2;
constexpr uint8_t kMaxChannel = 79;
constexpr uint32_t kHopIntervalMs = 80;
}

JammerModule::JammerModule()
    : _radio(PIN_NRF_CE, PIN_NRF_CSN) {}

void JammerModule::begin() {
  _ok = false;
  _active = false;
  _direction = true;
  _channel = 45;
  _lastHopMs = 0;

  pinMode(PIN_NRF_CE, OUTPUT);
  pinMode(PIN_NRF_CSN, OUTPUT);
  pinMode(PIN_NRF_SCK, OUTPUT);
  pinMode(PIN_NRF_MISO, INPUT);
  pinMode(PIN_NRF_MOSI, OUTPUT);
  pinMode(PIN_SD_CS, OUTPUT);
  digitalWrite(PIN_SD_CS, HIGH);

  SPI.begin(PIN_NRF_SCK, PIN_NRF_MISO, PIN_NRF_MOSI, PIN_NRF_CSN);
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
  if (!_ok || !_active) {
    return;
  }

  const uint32_t now = millis();
  if (now - _lastHopMs < kHopIntervalMs) {
    return;
  }

  _lastHopMs = now;
  updateHop();
}

void JammerModule::updateHop() {
  if (_direction) {
    _channel += 2;
    if (_channel >= kMaxChannel) {
      _channel = kMaxChannel;
      _direction = false;
    }
  } else {
    _channel -= 2;
    if (_channel <= kMinChannel) {
      _channel = kMinChannel;
      _direction = true;
    }
  }

  _radio.setChannel(_channel);
  _radio.startConstCarrier(RF24_PA_MAX, _channel);
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
    _active = true;
    _lastHopMs = 0;
    _radio.startConstCarrier(RF24_PA_MAX, _channel);
    Serial.println("[jammer] Störung aktiviert");
    return true;
  }

  if (action == "stop") {
    _active = false;
    _radio.stopConstCarrier();
    Serial.println("[jammer] Störung deaktiviert");
    return true;
  }

  return false;
}
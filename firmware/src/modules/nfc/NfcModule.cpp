#include "NfcModule.h"

#include "../../config.h"
#include <Wire.h>

NfcModule::NfcModule()
    : _transport(Wire, PIN_I2C_SDA, PIN_I2C_SCL, PN532_I2C_ADDR),
      _reader(_transport) {}

void NfcModule::begin() {
  _reader.begin();

  if (!_transport.isReady()) {
    Serial.println("[nfc] I2C-Bus konnte nicht initialisiert werden - Modul inaktiv");
    return;
  }

  uint32_t version = 0;
  for (uint8_t attempt = 0; attempt < 3 && version == 0; ++attempt) {
    if (attempt > 0) delay(100);
    version = _reader.getFirmwareVersion();
  }
  if (version == 0) {
    Serial.println("[nfc] PN532 nicht gefunden - Modul inaktiv");
    return;
  }

  if (!_reader.SAMConfig()) {
    Serial.println("[nfc] SAMConfig fehlgeschlagen - Modul inaktiv");
    return;
  }

  if (!_reader.setPassiveActivationRetries(NFC_PASSIVE_RETRIES)) {
    Serial.println("[nfc] Passive Retry-Konfiguration fehlgeschlagen - Modul inaktiv");
    return;
  }
  _ok = true;
  _readPending = true;
  Serial.print("[nfc] PN532 Firmware-Version: ");
  Serial.println((version >> 16) & 0xFF, DEC);
  Serial.println("[nfc] Bereit - NFC-Tag wird gesucht");
}

void NfcModule::update() {
  if (!_ok || !_readPending) return;

  const unsigned long now = millis();
  if (_hasPolled && now - _lastPollMs < NFC_POLL_INTERVAL_MS) return;

  _lastPollMs = now;
  _hasPolled = true;

  if (_rawDumpPending) {
    dumpNextRawPages();
    return;
  }

  uint8_t uid[MaxUidLength];
  uint8_t uidLength = 0;
  uint8_t activationResponse[MaxActivationResponseLength];
  uint8_t activationResponseLength = 0;
  const bool targetDetected = _reader.readPassiveTargetID(
      PN532_MIFARE_ISO14443A, uid, &uidLength, NFC_READ_TIMEOUT_MS,
      activationResponse, sizeof(activationResponse),
      &activationResponseLength);
  if (activationResponseLength > 0) {
    printHex(activationResponse, activationResponseLength);
  }
  if (!targetDetected) {
    return;
  }

  if (uidLength == 0 || uidLength > MaxUidLength) {
    return;
  }

  _nextRawPage = FirstRawPage;
  _rawDumpPending = true;
}

void NfcModule::dumpNextRawPages() {
  uint8_t response[MaxRawResponseLength];
  const int16_t responseLength = _reader.mifareultralight_ReadPagesRaw(
      _nextRawPage, response, sizeof(response));
  if (responseLength > 0) {
    printHex(response, static_cast<uint8_t>(responseLength));
  }

  _nextRawPage += 4;
  if (_nextRawPage > LastRawPage) {
    _rawDumpPending = false;
  } else if (responseLength < 0) {
    _rawDumpPending = false;
  }
}

void NfcModule::printHex(const uint8_t* data, uint8_t length) {
  for (uint8_t i = 0; i < length; ++i) {
    if (data[i] < 0x10) Serial.print('0');
    Serial.print(data[i], HEX);
    if (i + 1 < length) Serial.print(' ');
  }
  Serial.println();
}

void NfcModule::statusJson(JsonObject obj) {
  obj["ok"] = _ok;
  obj["ready"] = _ok;
  obj["readPending"] = _readPending;
}

bool NfcModule::handleCommand(const String& action, JsonObjectConst params) {
  (void)params;
  if (!_ok || action != "read") return false;

  _readPending = true;
  _rawDumpPending = false;
  _hasPolled = false;
  return true;
}

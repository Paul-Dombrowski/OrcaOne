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

  const uint32_t version = _reader.getFirmwareVersion();
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

  uint8_t uid[MaxUidLength];
  uint8_t uidLength = 0;
  if (!_reader.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength,
                                   NFC_READ_TIMEOUT_MS)) {
    return;
  }

  _readPending = false;
  if (uidLength == 0 || uidLength > MaxUidLength) {
    Serial.println("[nfc] Ungueltige UID-Laenge empfangen");
    return;
  }

  static const char HexDigits[] = "0123456789ABCDEF";
  char formattedUid[MaxUidLength * 2 + 1];
  for (uint8_t i = 0; i < uidLength; ++i) {
    formattedUid[i * 2] = HexDigits[uid[i] >> 4];
    formattedUid[i * 2 + 1] = HexDigits[uid[i] & 0x0F];
  }
  formattedUid[uidLength * 2] = '\0';

  _lastUid = formattedUid;

  Serial.print("[nfc] Card UID: ");
  for (uint8_t i = 0; i < uidLength; ++i) {
    if (uid[i] < 0x10) Serial.print('0');
    Serial.print(uid[i], HEX);
    Serial.print(' ');
  }
  Serial.println();
}

void NfcModule::statusJson(JsonObject obj) {
  obj["ok"] = _ok;
  obj["ready"] = _ok;
  obj["lastUid"] = _lastUid;
  obj["readPending"] = _readPending;
}

bool NfcModule::handleCommand(const String& action, JsonObjectConst params) {
  (void)params;
  if (!_ok || action != "read") return false;

  _readPending = true;
  _hasPolled = false;
  return true;
}

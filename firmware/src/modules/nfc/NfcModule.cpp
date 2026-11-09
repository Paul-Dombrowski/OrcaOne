#include "NfcModule.h"

#include "../../config.h"
#include "../../storage/StorageService.h"
#include <Wire.h>

NfcModule::NfcModule()
    : _transport(Wire, PIN_I2C_SDA, PIN_I2C_SCL, PN532_I2C_ADDR),
      _reader(_transport) {}

void NfcModule::begin() {
  _reader.begin();

  if (!_transport.isReady()) {
    _error = "I2C-Bus nicht bereit";
    Serial.printf("[nfc] %s - Modul inaktiv\n", _error);
    return;
  }

  uint32_t version = 0;
  for (uint8_t attempt = 0; attempt < 3 && version == 0; ++attempt) {
    if (attempt > 0) delay(100);
    version = _reader.getFirmwareVersion();
  }
  if (version == 0) {
    _error = "NFC-Sensor fehlt: PN532";
    Serial.printf("[nfc] %s - Modul inaktiv\n", _error);
    return;
  }

  if (!_reader.SAMConfig()) {
    _error = "PN532-Initialisierung fehlgeschlagen";
    Serial.printf("[nfc] %s - Modul inaktiv\n", _error);
    return;
  }

  if (!_reader.setPassiveActivationRetries(NFC_PASSIVE_RETRIES)) {
    _error = "NFC-Konfiguration fehlgeschlagen";
    Serial.printf("[nfc] %s - Modul inaktiv\n", _error);
    return;
  }
  _ok = true;
  _error = nullptr;
  _readPending = true;
  Serial.print("[nfc] PN532 Firmware-Version: ");
  Serial.println((version >> 16) & 0xFF, DEC);
  Serial.println("[nfc] Bereit. Halten Sie eine Karte an den Leser...");
}

void NfcModule::update() {
  if (!_ok) return;

  // Emulationsmodus: PN532 gibt sich als Karte aus (statt zu lesen).
  if (_emulating) {
    if (_emulator.runOnce(_capture, _emuUid, NFC_EMU_INIT_TIMEOUT_MS)) _needReaderReinit = true;
    return;
  }

  // Nach einer Emulation muss der PN532 fuer den Lesebetrieb (Initiator)
  // einmal neu konfiguriert werden - tgInitAsTarget hat ihn in den
  // Target-Modus versetzt.
  if (_needReaderReinit) {
    _reader.SAMConfig();
    _reader.setPassiveActivationRetries(NFC_PASSIVE_RETRIES);
    _needReaderReinit = false;
  }

  if (!_readPending) return;

  const unsigned long now = millis();
  if (_hasPolled && now - _lastPollMs < NFC_POLL_INTERVAL_MS) return;

  _lastPollMs = now;
  _hasPolled = true;

  uint8_t uid[MaxUidLength];
  uint8_t uidLength = 0;
  
  // Karte im Feld suchen (ISO14443A)
  const bool targetDetected = _reader.readPassiveTargetID(
      PN532_MIFARE_ISO14443A, uid, &uidLength, NFC_READ_TIMEOUT_MS);

  if (!targetDetected || uidLength == 0 || uidLength > MaxUidLength) {
    return;
  }

  char uidStr[24] = {0};
  for (uint8_t i = 0; i < uidLength; ++i) {
    snprintf(&uidStr[i * 2], 3, "%02X", uid[i]);
  }
  _card.uid = String(uidStr);

  Serial.println("\n====================================");
  Serial.print("[INFO] Kontaktlose Karte erkannt! UID: ");
  Serial.println(_card.uid);

  _emvReader.readCard(_card, _capture);

  // Auto-Save: eine erfolgreich gelesene Karte (Dump bis "ENDE DUMP" =
  // _card.readSuccess) wird automatisch ueber den zentralen Speicher-Dienst
  // abgelegt. Schluessel = 16-stellige Kartennummer -> gleiche Karte
  // ueberschreibt sich selbst (kein Duplikat), verschiedene Karten liegen
  // nebeneinander. Laeuft auch ohne SD (dann fluechtig im RAM).
  if (_card.readSuccess && _card.cardNumber.length() > 0) {
    const bool saved = Storage.save("nfc", _card.cardNumber, cardToJson());
    Serial.println(saved ? "[nfc] Karte gespeichert" : "[nfc] Speichern fehlgeschlagen");
  } else if (_card.readSuccess) {
    Serial.println("[nfc] Kartennummer fehlt - nicht gespeichert");
  }

  Serial.println("====================================\n");
}

String NfcModule::cardToJson() const {
  JsonDocument doc;
  doc["uid"]           = _card.uid;
  doc["aid"]           = _card.aid;
  doc["cardType"]      = _card.cardType;
  doc["cardNumber"]    = _card.cardNumber;
  doc["expiry"]        = _card.expiry;
  doc["cardholder"]    = _card.cardholder;
  doc["countryCode"]   = _card.countryCode;
  doc["currencyCode"]  = _card.currencyCode;
  doc["effectiveDate"] = _card.effectiveDate;
  doc["psn"]           = _card.psn;
  doc["serviceCode"]   = _card.serviceCode;
  doc["aip"]           = _card.aip;

  String out;
  serializeJson(doc, out);
  return out;
}

void NfcModule::statusJson(JsonObject obj) {
  obj["ok"] = _ok;
  obj["ready"] = _ok;
  if (_error != nullptr) obj["error"] = _error;
  obj["readPending"] = _readPending;
  obj["lastUid"] = _card.uid;
  obj["lastAid"] = _card.aid;
  obj["lastCardType"] = _card.cardType;
  obj["lastCardNumber"] = _card.cardNumber;
  obj["lastExpiry"] = _card.expiry;
  obj["lastCardholder"] = _card.cardholder;
  obj["lastCountryCode"] = _card.countryCode;
  obj["lastCurrencyCode"] = _card.currencyCode;
  obj["lastEffectiveDate"] = _card.effectiveDate;
  obj["lastPsn"] = _card.psn;
  obj["lastServiceCode"] = _card.serviceCode;
  obj["lastAip"] = _card.aip;
  obj["cardReadSuccess"] = _card.readSuccess;
  obj["emulating"] = _emulating;   // Emulationsmodus aktiv?
  obj["emuReady"] = _capture.valid;  // liegt ein Mitschnitt zum Zuruckspielen vor?
}

bool NfcModule::handleCommand(const String& action, JsonObjectConst params) {
  (void)params;
  if (!_ok) return false;

  if (action == "read") {
    // Emulation beenden, falls aktiv, und wieder in den Lesebetrieb gehen.
    if (_emulating) {
      _emulating = false;
      _needReaderReinit = true;
    }
    _readPending = true;
    _hasPolled = false;
    return true;
  }

  if (action == "emulate") {
    if (!_capture.valid) {
      Serial.println("[emu] Kein Mitschnitt vorhanden - zuerst eine Karte lesen "
                     "(Befehl 'read'), dann 'emulate'.");
      return false;
    }
    _emulating = true;
    _emulator.resetWaitLog();
    Serial.println("[emu] Emulationsmodus aktiv. Halten Sie das Handy an den PN532.");
    return true;
  }

  if (action == "stop") {
    if (_emulating) {
      _emulating = false;
      _needReaderReinit = true;
      Serial.println("[emu] Emulation gestoppt.");
    }
    return true;
  }

  if (action == "clear") {
    _card.clear();
    _capture.reset();
    return true;
  }

  return false;
}

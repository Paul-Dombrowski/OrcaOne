#include "NfcModule.h"
#include "EmvReader.h"
#include "EmvTags.h"
#include "EmvTypes.h"
#include "Tlv.h"

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
  Serial.println("[nfc] Bereit. Halten Sie eine Karte an den Leser...");
}

void NfcModule::update() {
  if (!_ok) return;

  // Emulationsmodus: PN532 gibt sich als Karte aus (statt zu lesen).
  if (_emulating) {
    runEmulation();
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

  Serial.println("====================================\n");
}

void NfcModule::statusJson(JsonObject obj) {
  obj["ok"] = _ok;
  obj["ready"] = _ok;
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
    _emuWaitLogged = false;
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
    return true;
  }

  return false;
}

// ============================================================================
//  Karten-Emulation (Capture & Replay)
// ============================================================================

uint8_t NfcModule::buildSynthPpse(uint8_t* out, uint8_t outCap) {
  // Minimale PPSE-FCI, die genau unsere eine AID anpreist:
  //   6F L1  84 0E <"2PAY.SYS.DDC01">  A5 L2  BF0C L3  61 L4  4F <len> <AID>  87 01 01
  if (_capture.aidLen == 0 || _capture.aidLen > 16) return 0;
  static const uint8_t ppseName[14] = {
      '2','P','A','Y','.','S','Y','S','.','D','D','C','0','1'};

  uint8_t rec61[32];  // 61-Template-Inhalt: 4F <len> <AID> 87 01 01
  uint8_t n = 0;
  rec61[n++] = 0x4F;
  rec61[n++] = _capture.aidLen;
  memcpy(&rec61[n], _capture.aid, _capture.aidLen); n += _capture.aidLen;
  rec61[n++] = 0x87; rec61[n++] = 0x01; rec61[n++] = 0x01;

  uint8_t bf0c[40];   // BF0C-Inhalt: 61 <len> <rec61>
  uint8_t m = 0;
  bf0c[m++] = 0x61; bf0c[m++] = n;
  memcpy(&bf0c[m], rec61, n); m += n;

  uint8_t a5[48];     // A5-Inhalt: BF0C <len> <bf0c>
  uint8_t p = 0;
  a5[p++] = 0xBF; a5[p++] = 0x0C; a5[p++] = m;
  memcpy(&a5[p], bf0c, m); p += m;

  // 6F-Inhalt: 84 0E <name> A5 <len> <a5>
  uint8_t body[96];
  uint8_t q = 0;
  body[q++] = 0x84; body[q++] = 0x0E;
  memcpy(&body[q], ppseName, 14); q += 14;
  body[q++] = 0xA5; body[q++] = p;
  memcpy(&body[q], a5, p); q += p;

  uint8_t total = 2 + q;  // 6F <len> <body>
  if (total > outCap) return 0;
  out[0] = 0x6F; out[1] = q;
  memcpy(&out[2], body, q);
  return total;
}

int NfcModule::buildEmuResponse(const uint8_t* c, int clen, uint8_t* out, int outCap) {
  // Haengt an die (ggf. gekappten) Nutzdaten die beiden Statusbytes an.
  auto emit = [&](const uint8_t* data, int len, uint8_t sw1, uint8_t sw2) -> int {
    if (len < 0) len = 0;
    if (len > outCap - 2) len = outCap - 2;  // Platz fuer SW1 SW2 sichern
    if (len > 0 && data) memcpy(out, data, len);
    out[len] = sw1;
    out[len + 1] = sw2;
    return len + 2;
  };

  if (clen < 4) return emit(nullptr, 0, 0x6F, 0x00);  // 6F00: kein Diagnose

  const uint8_t ins = c[1];
  const uint8_t p1 = c[2];
  const uint8_t p2 = c[3];

  // SELECT (00/80 A4 04 ..) - by DF name
  if (ins == 0xA4 && p1 == 0x04 && clen >= 5) {
    const uint8_t lc = c[4];
    const uint8_t* df = &c[5];
    static const char* kPpse = "2PAY.SYS.DDC01";
    if (lc == 14 && clen >= 5 + 14 && memcmp(df, kPpse, 14) == 0) {
      if (_capture.ppseLen > 0) return emit(_capture.ppse, _capture.ppseLen, 0x90, 0x00);
      uint8_t synth[96];
      uint8_t sl = buildSynthPpse(synth, sizeof(synth));
      if (sl > 0) return emit(synth, sl, 0x90, 0x00);
      return emit(nullptr, 0, 0x6A, 0x82);  // Datei nicht gefunden
    }
    // SELECT AID -> wir emulieren genau eine Applikation: FCI zurueckgeben.
    if (_capture.fciLen > 0) return emit(_capture.fci, _capture.fciLen, 0x90, 0x00);
    return emit(nullptr, 0, 0x6A, 0x82);
  }

  // GET PROCESSING OPTIONS (80 A8 ..)
  if (c[0] == 0x80 && ins == 0xA8) {
    if (_capture.gpoLen > 0) return emit(_capture.gpo, _capture.gpoLen, 0x90, 0x00);
    return emit(nullptr, 0, 0x6A, 0x81);  // Funktion nicht unterstuetzt
  }

  // READ RECORD (00 B2 rec (sfi<<3|4) ..)
  if (ins == 0xB2) {
    const uint8_t rec = p1;
    const uint8_t sfi = (p2 >> 3) & 0x1F;
    for (uint8_t i = 0; i < _capture.recordCount; ++i) {
      if (_capture.records[i].sfi == sfi && _capture.records[i].rec == rec) {
        return emit(_capture.records[i].data, _capture.records[i].len, 0x90, 0x00);
      }
    }
    return emit(nullptr, 0, 0x6A, 0x83);  // Record nicht gefunden
  }

  // GET DATA (80 CA ..) - nicht mitgeschnitten
  if (c[0] == 0x80 && ins == 0xCA) return emit(nullptr, 0, 0x6A, 0x88);

  return emit(nullptr, 0, 0x6D, 0x00);  // INS nicht unterstuetzt
}

void NfcModule::runEmulation() {
  // PN532 als ISO14443-4-PICC initialisieren (gleiche bewaehrten Init-Bytes wie
  // der Type-4-Emulationspfad: MODE 0x05 = PICC only/passive, SAK 0x20).
  uint8_t cmd[] = {
      PN532_COMMAND_TGINITASTARGET,
      0x05,                     // MODE: PICC only, passive only
      0x04, 0x00,               // SENS_RES (ATQA)
      _emuUid[1], _emuUid[2], _emuUid[3],  // NFCID1 (3 Byte)
      0x20,                     // SEL_RES (SAK) -> ISO14443-4
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // FeliCaParams (18)
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0,                          // NFCID3t (10)
      0,                        // Laenge General Bytes
      0                         // Laenge Historical Bytes
  };

  if (!_emuWaitLogged) {
    Serial.println("[emu] Warte auf Leser (Handy an den PN532 halten)...");
    _emuWaitLogged = true;
  }

  int8_t st = _reader.tgInitAsTarget(cmd, sizeof(cmd), NFC_EMU_INIT_TIMEOUT_MS);
  if (st == 0) return;  // Timeout, noch kein Leser -> naechster update()-Zyklus
  if (st < 0) {
    Serial.println("[emu] tgInitAsTarget-Fehler (PN532 unterstuetzt ISO-DEP-"
                   "Target evtl. nicht zuverlaessig).");
    _needReaderReinit = true;
    return;
  }

  Serial.println("[emu] Leser aktiv - beantworte APDUs.");
  _emuWaitLogged = false;  // naechste Runde darf wieder "warte" melden

  uint8_t capdu[255];
  uint8_t resp[255];
  // Schleifenlimit gegen Haengenbleiben bei fehlerhaftem Leser.
  for (int guard = 0; guard < 64; ++guard) {
    int16_t n = _reader.tgGetData(capdu, sizeof(capdu));
    if (n < 0) break;  // Leser weg / Fehler -> Sitzung beenden
    if (n == 0) continue;

    Serial.print("[emu] <- APDU (");
    Serial.print(n);
    Serial.print(" B): ");
    Tlv::printHex(capdu, (n > 16) ? 16 : static_cast<uint8_t>(n));
    Serial.println();

    int outLen = buildEmuResponse(capdu, n, resp, sizeof(resp));
    if (outLen <= 0) break;
    if (!_reader.tgSetData(resp, static_cast<uint8_t>(outLen))) {
      Serial.println("[emu] tgSetData fehlgeschlagen - Sitzung beendet.");
      break;
    }
  }

  _reader.inRelease();
  _needReaderReinit = true;
}

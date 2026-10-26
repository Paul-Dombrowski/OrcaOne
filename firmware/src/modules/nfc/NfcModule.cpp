#include "NfcModule.h"

#include "../../config.h"
#include <Wire.h>

namespace {
// Kontaktlos: 2PAY.SYS.DDC01 (PPSE)
const uint8_t SelectPPSE[] = {
    0x00, 0xA4, 0x04, 0x00, 0x0E,
    '2', 'P', 'A', 'Y', '.', 'S', 'Y', 'S', '.', 'D', 'D', 'C', '0', '1',
    0x00
};

// Bekannte Standard-AIDs als direkter Fallback, falls PPSE nicht antwortet
struct WellKnownAid {
  const char* name;
  uint8_t aid[8];
  uint8_t len;
};

const WellKnownAid FallbackAids[] = {
    {"Visa Credit/Debit", {0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10}, 7},
    {"Visa Electron",     {0xA0, 0x00, 0x00, 0x00, 0x03, 0x20, 0x10}, 7},
    {"V PAY",             {0xA0, 0x00, 0x00, 0x00, 0x03, 0x20, 0x20}, 7},
    {"Mastercard",        {0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10}, 7},
    {"Maestro",           {0xA0, 0x00, 0x00, 0x00, 0x04, 0x30, 0x60}, 7},
    {"Girocard (EMV)",    {0xD2, 0x76, 0x00, 0x00, 0x25, 0x45, 0x50, 0x01}, 8},
    {"Girocard / Geldk.", {0xD2, 0x76, 0x00, 0x00, 0x25, 0x45, 0x50, 0x02}, 8},
    {"American Express",  {0xA0, 0x00, 0x00, 0x00, 0x25, 0x01}, 6}
};
}

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
  if (!_ok || !_readPending) return;

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
  _lastUid = String(uidStr);

  Serial.println("\n====================================");
  Serial.print("[INFO] Kontaktlose Karte erkannt! UID: ");
  Serial.println(_lastUid);

  processCard();

  Serial.println("====================================\n");
}

void NfcModule::processCard() {
  _cardReadSuccess = false;
  _lastAid = "";
  _lastCardNumber = "";
  _lastExpiry = "";
  _lastCardType = "";

  std::vector<EmvAid> aidList;

  // 1. PPSE Directory (2PAY.SYS.DDC01) abfragen
  bool ppseOk = selectPpse(aidList);

  if (!ppseOk || aidList.empty()) {
    Serial.println("[EMV] PPSE leer oder fehlgeschlagen -> Verwende Fallback-AIDs");
    for (const auto& fa : FallbackAids) {
      EmvAid item{};
      memcpy(item.aid, fa.aid, fa.len);
      item.len = fa.len;
      strncpy(item.label, fa.name, sizeof(item.label) - 1);
      item.priority = 99;
      aidList.push_back(item);
    }
  }

  Serial.print("[EMV] Gefundene / gepruefte Applikationen (AIDs): ");
  Serial.println(aidList.size());

  // 2. Durchlaufe die AIDs
  for (size_t i = 0; i < aidList.size(); ++i) {
    const auto& item = aidList[i];
    Serial.print("[EMV] Versuche AID #");
    Serial.print(i + 1);
    Serial.print(" (");
    if (strlen(item.label) > 0) {
      Serial.print(item.label);
      Serial.print(", ");
    }
    printHex(item.aid, item.len);
    Serial.print(")... ");

    if (!selectAid(item.aid, item.len)) {
      Serial.println("abgelehnt.");
      continue;
    }
    Serial.println("erfolgreich selektiert!");

    // AID merken
    char aidHex[40] = {0};
    for (int k = 0; k < item.len && (k * 2 + 1) < sizeof(aidHex); ++k) {
      snprintf(&aidHex[k * 2], 3, "%02X", item.aid[k]);
    }
    _lastAid = String(aidHex);
    if (strlen(item.label) > 0) {
      _lastCardType = String(item.label);
    }

    // 3. GET PROCESSING OPTIONS (GPO) senden, um AFL zu ermitteln
    uint8_t afl[64] = {0};
    uint8_t aflLen = 0;
    bool gpoSuccess = tryGetProcessingOptions(afl, aflLen);

    // 4. Records auslesen
    if (readCardRecords(gpoSuccess ? afl : nullptr, aflLen)) {
      _cardReadSuccess = true;
      Serial.println("[SUCCESS] Kartendaten erfolgreich gelesen!");
      break;
    } else {
      Serial.println("[HINWEIS] Keine lesbaren Record-Daten unter dieser AID.");
    }
  }

  if (!_cardReadSuccess) {
    Serial.println("[FEHLER] Auslesen der Kartendaten auf allen verfuegbaren AIDs fehlgeschlagen.");
  }
}

bool NfcModule::selectPpse(std::vector<EmvAid>& aidList) {
  uint8_t response[MaxBufferLength];
  uint8_t responseLength = sizeof(response);

  Serial.println("[EMV] Sende SELECT 2PAY.SYS.DDC01 (PPSE)...");
  bool success = _reader.inDataExchange(
      const_cast<uint8_t*>(SelectPPSE),
      sizeof(SelectPPSE),
      response,
      &responseLength);

  if (!success || responseLength < 2) {
    Serial.println("[EMV] Keine Antwort auf PPSE-Kommando.");
    return false;
  }

  // [DEBUG] Rohantwort der Karte auf PPSE - temporaer zur Fehlersuche.
  Serial.print("[DEBUG] PPSE Rohantwort (");
  Serial.print(responseLength);
  Serial.print(" Byte): ");
  printHex(response, responseLength);
  Serial.println();

  uint8_t sw1 = response[responseLength - 2];
  uint8_t sw2 = response[responseLength - 1];

  if (sw1 != 0x90 || sw2 != 0x00) {
    Serial.print("[EMV] PPSE abgewiesen, Status: 0x");
    Serial.print(sw1, HEX);
    Serial.print(" 0x");
    Serial.println(sw2, HEX);
    return false;
  }

  Serial.println("[EMV] PPSE erfolgreich beantwortet. Extrahiere AIDs...");
  extractAllAids(response, responseLength - 2, aidList);
  return !aidList.empty();
}

bool NfcModule::selectAid(const uint8_t* aid, uint8_t aidLen) {
  if (aidLen == 0 || aidLen > 16) return false;

  uint8_t cmd[32];
  cmd[0] = 0x00; // CLA
  cmd[1] = 0xA4; // INS (SELECT)
  cmd[2] = 0x04; // P1 (Select by DF Name)
  cmd[3] = 0x00; // P2 (First or only occurrence)
  cmd[4] = aidLen;
  memcpy(&cmd[5], aid, aidLen);
  cmd[5 + aidLen] = 0x00; // Le

  uint8_t response[MaxBufferLength];
  uint8_t responseLength = sizeof(response);

  bool success = _reader.inDataExchange(cmd, 5 + aidLen + 1, response, &responseLength);
  if (!success || responseLength < 2) {
    // [DEBUG] temporaer: zeigt, ob die Karte ueberhaupt antwortet.
    Serial.print("[DEBUG] AID SELECT ohne verwertbare Antwort (success=");
    Serial.print(success ? "true" : "false");
    Serial.print(", len=");
    Serial.print(responseLength);
    Serial.println(")");
    return false;
  }

  uint8_t sw1 = response[responseLength - 2];
  uint8_t sw2 = response[responseLength - 1];
  // [DEBUG] temporaer: echtes Statuswort + Rohantwort je AID.
  Serial.print("[DEBUG] AID SELECT SW: 0x");
  if (sw1 < 0x10) Serial.print('0');
  Serial.print(sw1, HEX);
  Serial.print(" 0x");
  if (sw2 < 0x10) Serial.print('0');
  Serial.print(sw2, HEX);
  Serial.print("  Rohantwort: ");
  printHex(response, responseLength);
  Serial.println();
  return (sw1 == 0x90 && sw2 == 0x00);
}

bool NfcModule::tryGetProcessingOptions(uint8_t* outAfl, uint8_t& outAflLen) {
  outAflLen = 0;

  // Standard GPO mit leerem PDOL (Format 83 00)
  uint8_t gpoEmpty[] = { 0x80, 0xA8, 0x00, 0x00, 0x02, 0x83, 0x00, 0x00 };
  uint8_t response[MaxBufferLength];
  uint8_t responseLength = sizeof(response);

  Serial.println("[EMV] Sende GET PROCESSING OPTIONS (GPO)...");
  bool success = _reader.inDataExchange(gpoEmpty, sizeof(gpoEmpty), response, &responseLength);

  if (!success || responseLength < 2) {
    Serial.println("[EMV] GPO Anfrage fehlgeschlagen (evtl. PDOL erforderlich).");
    return false;
  }

  uint8_t sw1 = response[responseLength - 2];
  uint8_t sw2 = response[responseLength - 1];

  if (sw1 != 0x90 || sw2 != 0x00) {
    Serial.print("[EMV] GPO Status: 0x");
    Serial.print(sw1, HEX);
    Serial.print(" 0x");
    Serial.println(sw2, HEX);
    return false;
  }

  // Suche nach Tag 0x94 (Application File Locator - AFL)
  int aflIdx = findTag(response, responseLength - 2, 0x94);
  if (aflIdx != -1) {
    uint8_t len = response[aflIdx + 1];
    if (len <= 64) {
      memcpy(outAfl, &response[aflIdx + 2], len);
      outAflLen = len;
      Serial.print("[EMV] AFL (Tag 0x94) gefunden. Laenge: ");
      Serial.println(outAflLen);
      return true;
    }
  }

  // Format 1 Response: Tag 0x80 gefolgt von AIP (2 Bytes) und AFL
  int format1Idx = findTag(response, responseLength - 2, 0x80);
  if (format1Idx != -1) {
    uint8_t len = response[format1Idx + 1];
    if (len > 2) {
      uint8_t aflBytes = len - 2;
      if (aflBytes <= 64) {
        memcpy(outAfl, &response[format1Idx + 4], aflBytes);
        outAflLen = aflBytes;
        Serial.print("[EMV] AFL in Format 1 Response gefunden. Laenge: ");
        Serial.println(outAflLen);
        return true;
      }
    }
  }

  return false;
}

bool NfcModule::readCardRecords(const uint8_t* afl, uint8_t aflLen) {
  uint8_t response[MaxBufferLength];
  bool foundData = false;

  // Wenn AFL vorhanden ist, genau die Records anfragen
  if (afl != nullptr && aflLen >= 4) {
    for (int i = 0; i < aflLen; i += 4) {
      uint8_t sfi = (afl[i] >> 3) & 0x1F;
      uint8_t firstRec = afl[i + 1];
      uint8_t lastRec = afl[i + 2];

      for (uint8_t rec = firstRec; rec <= lastRec; ++rec) {
        uint8_t readRecordCmd[] = {
          0x00, 0xB2, rec, static_cast<uint8_t>((sfi << 3) | 0x04), 0x00
        };
        uint8_t responseLength = sizeof(response);
        bool success = _reader.inDataExchange(readRecordCmd, sizeof(readRecordCmd), response, &responseLength);
        if (success && responseLength >= 2 && response[responseLength - 2] == 0x90 && response[responseLength - 1] == 0x00) {
          if (parseRecordData(response, responseLength - 2)) {
            foundData = true;
          }
        }
      }
    }
    if (foundData) return true;
  }

  // Fallback: Probiere bekannte SFIs (1 bis 3) und Records (1 bis 3)
  Serial.println("[EMV] Probiere Standard SFIs/Records...");
  const uint8_t sfis[] = { 1, 2, 3 };
  for (uint8_t sfi : sfis) {
    for (uint8_t rec = 1; rec <= 3; ++rec) {
      uint8_t readRecordCmd[] = {
        0x00, 0xB2, rec, static_cast<uint8_t>((sfi << 3) | 0x04), 0x00
      };
      uint8_t responseLength = sizeof(response);
      bool success = _reader.inDataExchange(readRecordCmd, sizeof(readRecordCmd), response, &responseLength);
      if (success && responseLength >= 2 && response[responseLength - 2] == 0x90 && response[responseLength - 1] == 0x00) {
        if (parseRecordData(response, responseLength - 2)) {
          return true;
        }
      }
    }
  }

  return foundData;
}

bool NfcModule::parseRecordData(const uint8_t* data, uint8_t dataLen) {
  bool parsedAnything = false;

  // 1. Tag 0x5A: PAN (Primäre Kontonummer)
  int panIdx = findTag(data, dataLen, 0x5A);
  if (panIdx != -1) {
    uint8_t panLen = data[panIdx + 1];
    String panStr = "";
    for (int i = 0; i < panLen; i++) {
      uint8_t b = data[panIdx + 2 + i];
      uint8_t highNibble = (b >> 4) & 0x0F;
      uint8_t lowNibble = b & 0x0F;
      if (highNibble <= 9) panStr += String(highNibble);
      if (lowNibble <= 9) panStr += String(lowNibble);
    }
    if (panStr.length() >= 12) {
      _lastCardNumber = panStr;
      parsedAnything = true;
      Serial.print(">> KARTENNUMMER (Tag 0x5A): ");
      for (size_t i = 0; i < panStr.length(); ++i) {
        Serial.print(panStr[i]);
        if ((i % 4 == 3) && (i + 1 < panStr.length())) Serial.print(' ');
      }
      Serial.println();
    }
  }

  // 2. Tag 0x5F24: Ablaufdatum (JJ/MM)
  int expIdx = findTag(data, dataLen, 0x5F24);
  if (expIdx != -1) {
    const uint8_t* expData = &data[expIdx + 3];
    char expStr[8];
    snprintf(expStr, sizeof(expStr), "%02d/%02d",
             ((expData[0] >> 4) & 0x0F) * 10 + (expData[0] & 0x0F),
             ((expData[1] >> 4) & 0x0F) * 10 + (expData[1] & 0x0F));
    _lastExpiry = String(expStr);
    parsedAnything = true;
    Serial.print(">> ABLAUFDATUM (Tag 0x5F24): ");
    Serial.println(_lastExpiry);
  }

  // 3. Tag 0x57: Track 2 Equivalent Data (oft vorhanden, wenn 0x5A nicht direkt existiert)
  int tr2Idx = findTag(data, dataLen, 0x57);
  if (tr2Idx != -1) {
    uint8_t tr2Len = data[tr2Idx + 1];
    String tr2Pan = "";
    String tr2Exp = "";
    parseTrack2(&data[tr2Idx + 2], tr2Len, tr2Pan, tr2Exp);

    if (_lastCardNumber.length() == 0 && tr2Pan.length() >= 12) {
      _lastCardNumber = tr2Pan;
      parsedAnything = true;
      Serial.print(">> KARTENNUMMER (Track 2): ");
      for (size_t i = 0; i < tr2Pan.length(); ++i) {
        Serial.print(tr2Pan[i]);
        if ((i % 4 == 3) && (i + 1 < tr2Pan.length())) Serial.print(' ');
      }
      Serial.println();
    }
    if (_lastExpiry.length() == 0 && tr2Exp.length() > 0) {
      _lastExpiry = tr2Exp;
      parsedAnything = true;
      Serial.print(">> ABLAUFDATUM (Track 2): ");
      Serial.println(_lastExpiry);
    }
  }

  return parsedAnything;
}

void NfcModule::parseTrack2(const uint8_t* track2Data, uint8_t track2Len, String& outPan, String& outExp) {
  outPan = "";
  outExp = "";
  bool readingPan = true;
  String digits = "";

  for (int i = 0; i < track2Len; ++i) {
    uint8_t high = (track2Data[i] >> 4) & 0x0F;
    uint8_t low = track2Data[i] & 0x0F;

    uint8_t nibbles[2] = {high, low};
    for (uint8_t n : nibbles) {
      if (n == 0x0D) { // Trennzeichen 'D'
        readingPan = false;
        continue;
      }
      if (n == 0x0F) { // End-Padding
        break;
      }
      if (readingPan) {
        outPan += String(n);
      } else {
        digits += String(n);
      }
    }
  }

  // Ablaufdatum folgt direkt nach 'D': JJMM
  if (digits.length() >= 4) {
    outExp = digits.substring(0, 2) + "/" + digits.substring(2, 4);
  }
}

void NfcModule::extractAllAids(const uint8_t* data, int dataLen, std::vector<EmvAid>& aidList) {
  int i = 0;
  while (i < dataLen - 2) {
    // Suche nach Tag 0x61 (Application Template) oder direkt Tag 0x4F (AID)
    if (data[i] == 0x4F) {
      int tagLen = data[i + 1];
      if (tagLen > 0 && tagLen <= 16 && (i + 2 + tagLen <= dataLen)) {
        EmvAid item{};
        item.len = tagLen;
        memcpy(item.aid, &data[i + 2], tagLen);
        item.priority = 0;
        item.label[0] = '\0';
        aidList.push_back(item);
      }
      i += 2 + tagLen;
    } else if (data[i] == 0x61) {
      // Application Template gefunden -> darin Tag 0x4F und 0x50/0x87 suchen
      int tmplLen = data[i + 1];
      int subStart = i + 2;
      int subEnd = subStart + tmplLen;
      if (subEnd > dataLen) subEnd = dataLen;

      EmvAid item{};
      item.len = 0;
      item.label[0] = '\0';
      item.priority = 50;

      int s = subStart;
      while (s < subEnd - 1) {
        uint32_t tag = data[s];
        int tSize = 1;
        if ((data[s] & 0x1F) == 0x1F) {
          tag = (data[s] << 8) | data[s + 1];
          tSize = 2;
        }
        if (s + tSize >= subEnd) break;
        int l = data[s + tSize];
        const uint8_t* val = &data[s + tSize + 1];

        if (tag == 0x4F && l <= 16) {
          item.len = l;
          memcpy(item.aid, val, l);
        } else if (tag == 0x50 && l < sizeof(item.label)) {
          memcpy(item.label, val, l);
          item.label[l] = '\0';
        } else if (tag == 0x87 && l >= 1) {
          item.priority = val[0] & 0x0F;
        }
        s += tSize + 1 + l;
      }

      if (item.len > 0) {
        aidList.push_back(item);
      }
      i = subEnd;
    } else {
      // Nächster Tag
      int tSize = 1;
      if ((data[i] & 0x1F) == 0x1F) {
        tSize = 2;
      }
      if (i + tSize >= dataLen) break;
      int l = data[i + tSize];
      i += tSize + 1 + l;
    }
  }
}

int NfcModule::findTag(const uint8_t* data, int dataLen, uint32_t tag) {
  int i = 0;
  while (i < dataLen - 2) {
    uint32_t currentTag = data[i];
    int tagLenSize = 1;

    // 2-Byte-Tags (z.B. 0x5F24)
    if ((data[i] & 0x1F) == 0x1F) {
      currentTag = (static_cast<uint32_t>(data[i]) << 8) | data[i + 1];
      tagLenSize = 2;
    }

    if (currentTag == tag) {
      return i;
    }

    if (i + tagLenSize >= dataLen) break;
    int len = data[i + tagLenSize];
    i += tagLenSize + 1 + len;
  }
  return -1;
}

void NfcModule::printHex(const uint8_t* data, uint8_t length) {
  for (uint8_t i = 0; i < length; ++i) {
    if (data[i] < 0x10) Serial.print('0');
    Serial.print(data[i], HEX);
    if (i + 1 < length) Serial.print(' ');
  }
}

void NfcModule::statusJson(JsonObject obj) {
  obj["ok"] = _ok;
  obj["ready"] = _ok;
  obj["readPending"] = _readPending;
  obj["lastUid"] = _lastUid;
  obj["lastAid"] = _lastAid;
  obj["lastCardType"] = _lastCardType;
  obj["lastCardNumber"] = _lastCardNumber;
  obj["lastExpiry"] = _lastExpiry;
  obj["cardReadSuccess"] = _cardReadSuccess;
}

bool NfcModule::handleCommand(const String& action, JsonObjectConst params) {
  (void)params;
  if (!_ok) return false;

  if (action == "read") {
    _readPending = true;
    _hasPolled = false;
    return true;
  }

  if (action == "clear") {
    _lastUid = "";
    _lastAid = "";
    _lastCardType = "";
    _lastCardNumber = "";
    _lastExpiry = "";
    _cardReadSuccess = false;
    return true;
  }

  return false;
}

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
  _lastUid = String(uidStr);

  Serial.println("\n====================================");
  Serial.print("[INFO] Kontaktlose Karte erkannt! UID: ");
  Serial.println(_lastUid);

  processCard();

  Serial.println("====================================\n");
}

void NfcModule::processCard() {
  _cardReadSuccess = false;
  resetEmuCapture();  // Mitschnitt fuer die Emulation frisch beginnen
  _lastAid = "";
  _lastCardNumber = "";
  _lastExpiry = "";
  _lastCardType = "";
  _lastCardholder = "";
  _lastCountryCode = "";
  _lastCurrencyCode = "";
  _lastEffectiveDate = "";
  _lastPsn = "";
  _lastServiceCode = "";
  _lastAip = "";

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

    uint8_t fci[MaxBufferLength];
    uint8_t fciLen = 0;
    if (!selectAid(item.aid, item.len, fci, &fciLen)) {
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

    // Fuer die Emulation: AID + rohes FCI dieser Applikation mitschneiden.
    _emuAidLen = (item.len <= sizeof(_emuAid)) ? item.len : sizeof(_emuAid);
    memcpy(_emuAid, item.aid, _emuAidLen);
    captureBlob(_emuFci, _emuFciLen, fci, fciLen);

    // 3. GET PROCESSING OPTIONS (GPO) senden
    uint8_t afl[64] = {0};
    uint8_t aflLen = 0;
    uint8_t gpo[MaxBufferLength];
    uint8_t gpoLen = 0;
    bool aflFound = tryGetProcessingOptions(fci, fciLen, afl, aflLen, gpo, gpoLen);

    // Fuer die Emulation: rohe GPO-Antwort mitschneiden (enthaelt bei qVSDC
    // bereits Track 2 / PAN, sonst AIP + AFL).
    if (gpoLen > 0) captureBlob(_emuGpo, _emuGpoLen, gpo, gpoLen);

    // 4. Kartendaten gewinnen.
    //    a) Viele kontaktlose Karten (qVSDC, z. B. Visa) liefern Track 2 / PAN
    //       direkt in der GPO-Antwort (Tag 57), ohne AFL. Das ist die
    //       massgebliche Quelle - zuerst dort suchen.
    bool gotData = false;
    if (gpoLen > 0) {
      gotData = parseRecordData(gpo, gpoLen);
    }
    //    b) Sonst klassisch ueber das AFL die Records lesen.
    if (!gotData) {
      gotData = readCardRecords(aflFound ? afl : nullptr, aflLen);
    }

    if (gotData) {
      _cardReadSuccess = true;
      _emuValid = true;  // Mitschnitt ist brauchbar -> Emulation moeglich
      Serial.println("[SUCCESS] Kartendaten erfolgreich gelesen!");
      // Vollstaendiger Dump aller ueber NFC lesbaren Daten dieser Applikation.
      dumpAllCardData(fci, fciLen, gpo, gpoLen, afl, aflLen, aflFound);
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
  // Fuer die Emulation: rohe PPSE-Antwort (ohne SW) mitschneiden.
  captureBlob(_emuPpse, _emuPpseLen, response, responseLength - 2);
  extractAllAids(response, responseLength - 2, aidList);
  return !aidList.empty();
}

bool NfcModule::selectAid(const uint8_t* aid, uint8_t aidLen,
                          uint8_t* outFci, uint8_t* outFciLen) {
  if (outFciLen) *outFciLen = 0;
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

  const bool ok = (sw1 == 0x90 && sw2 == 0x00);
  if (ok && outFci && outFciLen) {
    // FCI = Antwort ohne die beiden Statusbytes.
    uint8_t fciLen = responseLength - 2;
    memcpy(outFci, response, fciLen);
    *outFciLen = fciLen;
  }
  return ok;
}

uint8_t NfcModule::buildPdolData(const uint8_t* pdol, uint8_t pdolLen,
                                 uint8_t* out, size_t outCap) {
  uint8_t outLen = 0;
  int i = 0;
  while (i < pdolLen) {
    // Tag der PDOL-Liste lesen (1 oder 2 Byte)
    uint32_t tag = pdol[i];
    int tSize = 1;
    if ((pdol[i] & 0x1F) == 0x1F) {
      if (i + 1 >= pdolLen) break;
      tag = (static_cast<uint32_t>(pdol[i]) << 8) | pdol[i + 1];
      tSize = 2;
    }
    if (i + tSize >= pdolLen) break;
    uint8_t len = pdol[i + tSize];
    i += tSize + 1;

    // Standard-/Dummy-Terminalwerte in GENAU der verlangten Laenge. Fuer reines
    // Auslesen reicht das; die Karte prueft primaer die Laengen (sonst 0x6700).
    uint8_t val[32] = {0};
    switch (tag) {
      case 0x9F66: if (len >= 1) val[0] = 0x36; break;                              // TTQ (online-faehig)
      case 0x9F02: if (len >= 1) val[len - 1] = 0x01; break;                        // Betrag (BCD) 0.01
      case 0x9F1A: if (len >= 2) { val[len - 2] = 0x02; val[len - 1] = 0x76; } break; // Land DE 0276
      case 0x5F2A: if (len >= 2) { val[len - 2] = 0x09; val[len - 1] = 0x78; } break; // Waehrung EUR 0978
      case 0x9A:   if (len >= 3) { val[0] = 0x25; val[1] = 0x01; val[2] = 0x01; } break; // Datum JJMMTT
      case 0x9C:   break;                                                            // Transaktionstyp 00
      case 0x9F37: for (uint8_t k = 0; k < len && k < sizeof(val); ++k) val[k] = 0x11 * (k + 1); break; // UN
      case 0x9F35: if (len >= 1) val[0] = 0x22; break;                              // Terminal-Typ
      default:     break;                                                            // unbekannt -> Nullen
    }
    // Kapazitaetsgrenze: Kartendaten sind nicht vertrauenswuerdig, niemals
    // ueber den Ausgabepuffer hinausschreiben.
    if (static_cast<size_t>(outLen) + len > outCap) break;
    for (uint8_t k = 0; k < len; ++k) {
      out[outLen++] = (k < sizeof(val)) ? val[k] : 0x00;
    }
  }
  return outLen;
}

bool NfcModule::tryGetProcessingOptions(const uint8_t* fci, uint8_t fciLen,
                                        uint8_t* outAfl, uint8_t& outAflLen,
                                        uint8_t* outGpo, uint8_t& outGpoLen) {
  outAflLen = 0;
  outGpoLen = 0;

  // GPO-Kommando bauen. Verlangt die AID ein PDOL (Tag 9F38 im FCI), werden die
  // Werte in den geforderten Laengen geliefert; sonst leeres PDOL (83 00).
  uint8_t cmd[128];
  uint8_t cmdLen = 0;
  cmd[cmdLen++] = 0x80; cmd[cmdLen++] = 0xA8; cmd[cmdLen++] = 0x00; cmd[cmdLen++] = 0x00;

  uint8_t pdolData[64];
  uint8_t pdolDataLen = 0;
  int pdolIdx = (fci && fciLen) ? findTag(fci, fciLen, 0x9F38) : -1;
  if (pdolIdx != -1 && pdolIdx + 2 < fciLen) {
    uint8_t pdolLen = fci[pdolIdx + 2];   // 9F38 = 2-Byte-Tag -> Laengenbyte bei +2
    // pdolLen stammt von der Karte: auf die tatsaechlich vorhandenen Bytes klemmen.
    int avail = fciLen - (pdolIdx + 3);
    if (avail < 0) avail = 0;
    if (pdolLen > avail) pdolLen = static_cast<uint8_t>(avail);
    pdolDataLen = buildPdolData(&fci[pdolIdx + 3], pdolLen, pdolData, sizeof(pdolData));
    Serial.print("[EMV] PDOL (Tag 9F38) gefunden, Laenge ");
    Serial.print(pdolLen);
    Serial.print(" -> GPO-Daten ");
    Serial.print(pdolDataLen);
    Serial.println(" Byte.");
  } else {
    Serial.println("[EMV] Kein PDOL im FCI -> leeres GPO (83 00).");
  }

  cmd[cmdLen++] = static_cast<uint8_t>(2 + pdolDataLen);  // Lc
  cmd[cmdLen++] = 0x83;
  cmd[cmdLen++] = pdolDataLen;
  for (uint8_t k = 0; k < pdolDataLen; ++k) cmd[cmdLen++] = pdolData[k];
  cmd[cmdLen++] = 0x00;  // Le

  uint8_t response[MaxBufferLength];
  uint8_t responseLength = sizeof(response);

  Serial.println("[EMV] Sende GET PROCESSING OPTIONS (GPO)...");
  bool success = _reader.inDataExchange(cmd, cmdLen, response, &responseLength);

  if (!success || responseLength < 2) {
    Serial.println("[EMV] GPO Anfrage fehlgeschlagen.");
    return false;
  }

  uint8_t sw1 = response[responseLength - 2];
  uint8_t sw2 = response[responseLength - 1];

  // [DEBUG] temporaer: GPO-Rohantwort.
  Serial.print("[DEBUG] GPO Rohantwort (");
  Serial.print(responseLength);
  Serial.print(" Byte): ");
  printHex(response, responseLength);
  Serial.println();

  if (sw1 != 0x90 || sw2 != 0x00) {
    Serial.print("[EMV] GPO Status: 0x");
    if (sw1 < 0x10) Serial.print('0');
    Serial.print(sw1, HEX);
    Serial.print(" 0x");
    if (sw2 < 0x10) Serial.print('0');
    Serial.println(sw2, HEX);
    return false;
  }

  const int dataLen = responseLength - 2;

  // Rohe GPO-Nutzdaten (ohne SW) nach aussen geben. Bei qVSDC-Karten stehen
  // hier Track 2 / PAN (Tag 57) direkt drin und werden vom Aufrufer bevorzugt
  // ausgewertet - auch wenn (wie dann ueblich) kein AFL vorhanden ist.
  if (outGpo && dataLen > 0) {
    const uint8_t n = (dataLen <= static_cast<int>(MaxBufferLength))
                          ? static_cast<uint8_t>(dataLen)
                          : static_cast<uint8_t>(MaxBufferLength);
    memcpy(outGpo, response, n);
    outGpoLen = n;
  }

  // AFL-Suche, robust fuer beide GPO-Antwortformate:
  //  Format 2: Template 0x77 { 82 AIP, 94 AFL, ... } -> AFL in Tag 0x94
  //  Format 1: Template 0x80 { AIP(2) + AFL }        -> AFL direkt hinter AIP
  const uint8_t* body = response;
  int bodyLen = dataLen;
  if (dataLen >= 2 && response[0] == 0x77) {
    uint8_t tlen = response[1];           // GPO-Antworten sind kurz (<128)
    body = &response[2];
    bodyLen = (tlen <= dataLen - 2) ? tlen : (dataLen - 2);
  }

  int aflIdx = findTag(body, bodyLen, 0x94);
  if (aflIdx != -1) {
    uint8_t len = body[aflIdx + 1];
    if (len <= 64 && aflIdx + 2 + len <= bodyLen) {
      memcpy(outAfl, &body[aflIdx + 2], len);
      outAflLen = len;
      Serial.print("[EMV] AFL (Tag 0x94) gefunden. Laenge: ");
      Serial.println(outAflLen);
      return true;
    }
  }

  // Format 1: Tag 0x80 gefolgt von AIP (2 Byte) und AFL
  int format1Idx = findTag(response, dataLen, 0x80);
  if (format1Idx != -1) {
    uint8_t len = response[format1Idx + 1];
    if (len > 2) {
      uint8_t aflBytes = len - 2;
      if (aflBytes <= 64 && format1Idx + 4 + aflBytes <= dataLen) {
        memcpy(outAfl, &response[format1Idx + 4], aflBytes);
        outAflLen = aflBytes;
        Serial.print("[EMV] AFL in Format 1 Response gefunden. Laenge: ");
        Serial.println(outAflLen);
        return true;
      }
    }
  }

  Serial.println("[EMV] GPO ok, aber kein AFL gefunden.");
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
          captureRecord(sfi, rec, response, responseLength - 2);  // fuer Emulation
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
        captureRecord(sfi, rec, response, responseLength - 2);  // fuer Emulation
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

  // 4. Tag 0x5F20: Cardholder Name (ASCII)
  int nameIdx = findTag(data, dataLen, 0x5F20);
  if (nameIdx != -1) {
    uint8_t nameLen = data[nameIdx + 2]; // 2-Byte Tag -> Laenge bei +2
    const uint8_t* namePtr = &data[nameIdx + 3];
    char nameBuf[33] = {0};
    uint8_t copyLen = (nameLen < sizeof(nameBuf) - 1) ? nameLen : (sizeof(nameBuf) - 1);
    memcpy(nameBuf, namePtr, copyLen);
    _lastCardholder = String(nameBuf);
    _lastCardholder.trim();
    if (_lastCardholder.length() > 0) {
      parsedAnything = true;
      Serial.print(">> KARTENINHABER (Tag 0x5F20): ");
      Serial.println(_lastCardholder);
    }
  }

  // 5. Tag 0x5F28: Issuer Country Code (ISO 3166-1 numerisch, 2 Bytes BCD, z.B. 0276 = DE)
  int ccIdx = findTag(data, dataLen, 0x5F28);
  if (ccIdx != -1) {
    const uint8_t* ccData = &data[ccIdx + 3];
    char ccBuf[8];
    snprintf(ccBuf, sizeof(ccBuf), "%02X%02X", ccData[0], ccData[1]);
    _lastCountryCode = String(ccBuf);
    parsedAnything = true;
    Serial.print(">> LAENDER-CODE (Tag 0x5F28): ");
    Serial.print(_lastCountryCode);
    if (_lastCountryCode == "0276") Serial.print(" (Deutschland)");
    else if (_lastCountryCode == "0040") Serial.print(" (Oesterreich)");
    else if (_lastCountryCode == "0756") Serial.print(" (Schweiz)");
    Serial.println();
  }

  // 6. Tag 0x5F25: Application Effective Date (Gueltig ab, JJMMTT BCD)
  int effIdx = findTag(data, dataLen, 0x5F25);
  if (effIdx != -1) {
    const uint8_t* effData = &data[effIdx + 3];
    char effBuf[16];
    snprintf(effBuf, sizeof(effBuf), "%02X/%02X/%02X", effData[0], effData[1], effData[2]);
    _lastEffectiveDate = String(effBuf);
    parsedAnything = true;
    Serial.print(">> GUELTIG AB (Tag 0x5F25 JJ/MM/TT): ");
    Serial.println(_lastEffectiveDate);
  }

  // 7. Tag 0x5F30: Service Code (3 Ziffern, Track 2 Service Attributes)
  int scIdx = findTag(data, dataLen, 0x5F30);
  if (scIdx != -1) {
    const uint8_t* scData = &data[scIdx + 3];
    char scBuf[8];
    snprintf(scBuf, sizeof(scBuf), "%02X%01X", scData[0], (scData[1] >> 4) & 0x0F);
    _lastServiceCode = String(scBuf);
    parsedAnything = true;
    Serial.print(">> SERVICE CODE (Tag 0x5F30): ");
    Serial.println(_lastServiceCode);
  }

  // 8. Tag 0x5F34: PAN Sequence Number (PSN, 1 Byte)
  int psnIdx = findTag(data, dataLen, 0x5F34);
  if (psnIdx != -1) {
    uint8_t psnVal = data[psnIdx + 3];
    char psnBuf[6];
    snprintf(psnBuf, sizeof(psnBuf), "%02X", psnVal);
    _lastPsn = String(psnBuf);
    parsedAnything = true;
    Serial.print(">> KARTENFOLGENUMMER / PSN (Tag 0x5F34): ");
    Serial.println(_lastPsn);
  }

  // 9. Tag 0x9F42: Application Currency Code (ISO 4217, z.B. 0978 = EUR)
  int currIdx = findTag(data, dataLen, 0x9F42);
  if (currIdx != -1) {
    const uint8_t* currData = &data[currIdx + 3];
    char currBuf[8];
    snprintf(currBuf, sizeof(currBuf), "%02X%02X", currData[0], currData[1]);
    _lastCurrencyCode = String(currBuf);
    parsedAnything = true;
    Serial.print(">> WAEHRUNGS-CODE (Tag 0x9F42): ");
    Serial.print(_lastCurrencyCode);
    if (_lastCurrencyCode == "0978") Serial.print(" (EUR)");
    else if (_lastCurrencyCode == "0840") Serial.print(" (USD)");
    else if (_lastCurrencyCode == "0826") Serial.print(" (GBP)");
    else if (_lastCurrencyCode == "0756") Serial.print(" (CHF)");
    Serial.println();
  }

  // 10. Tag 0x82: Application Interchange Profile (AIP, 2 Bytes)
  int aipIdx = findTag(data, dataLen, 0x82);
  if (aipIdx != -1) {
    uint8_t aip1 = data[aipIdx + 2];
    uint8_t aip2 = data[aipIdx + 3];
    char aipBuf[16];
    snprintf(aipBuf, sizeof(aipBuf), "%02X%02X", aip1, aip2);
    _lastAip = String(aipBuf);
    parsedAnything = true;
    Serial.print(">> AIP (Tag 0x82): 0x");
    Serial.println(_lastAip);
    if (aip1 & 0x40) Serial.println("    [+] SDA (Static Data Authentication)");
    if (aip1 & 0x20) Serial.println("    [+] DDA (Dynamic Data Authentication)");
    if (aip1 & 0x10) Serial.println("    [+] Cardholder Verification");
    if (aip1 & 0x01) Serial.println("    [+] CDA (Combined DDA/AC Generation)");
  }

  // 11. Tag 0x8E: Cardholder Verification Method (CVM) List
  int cvmIdx = findTag(data, dataLen, 0x8E);
  if (cvmIdx != -1) {
    uint8_t cvmLen = data[cvmIdx + 1];
    Serial.print(">> CVM-LISTE (Tag 0x8E, ");
    Serial.print(cvmLen);
    Serial.print(" Byte): ");
    printHex(&data[cvmIdx + 2], cvmLen);
    Serial.println();
  }

  // 12. Tag 0x9F07: Application Usage Control (AUC, 2 Bytes)
  int aucIdx = findTag(data, dataLen, 0x9F07);
  if (aucIdx != -1) {
    uint8_t auc1 = data[aucIdx + 3];
    uint8_t auc2 = data[aucIdx + 4];
    Serial.printf(">> APPLICATION USAGE CONTROL (Tag 0x9F07): 0x%02X%02X\n", auc1, auc2);
    if (auc1 & 0x80) Serial.println("    [+] Gueltig an Geldautomaten");
    if (auc1 & 0x40) Serial.println("    [+] Gueltig an POS/Terminals");
    if (auc1 & 0x08) Serial.println("    [+] Inlandstransaktionen erlaubt");
    if (auc1 & 0x04) Serial.println("    [+] Auslandstransaktionen erlaubt");
  }

  // 13. Tag 0x9F08: Application Version Number
  int verIdx = findTag(data, dataLen, 0x9F08);
  if (verIdx != -1) {
    Serial.printf(">> APP VERSION (Tag 0x9F08): %02X%02X\n", data[verIdx + 3], data[verIdx + 4]);
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

int NfcModule::findTag(const uint8_t* data, int dataLen, uint32_t tag, int depth) {
  if (depth > 8 || dataLen <= 0) return -1;   // Rekursionsschutz (boesartige Karte)

  int i = 0;
  while (i < dataLen) {
    const int tagStart = i;

    // --- Tag lesen (1 oder 2 Bytes) ---
    uint32_t currentTag = data[i];
    int tSize = 1;
    if ((data[i] & 0x1F) == 0x1F) {           // mehrbyte-Tag (z. B. 0x5F24, 0x9F38)
      if (i + 1 >= dataLen) break;
      currentTag = (static_cast<uint32_t>(data[i]) << 8) | data[i + 1];
      tSize = 2;
    }
    if (i + tSize >= dataLen) break;

    // --- Laenge lesen (Kurz- oder Langform). Alle Laengen stammen von der
    //     Karte und werden strikt gegen dataLen validiert. ---
    const int lenPos = i + tSize;
    int len = data[lenPos];
    int lenSize = 1;
    if (len & 0x80) {
      const int n = len & 0x7F;
      if (n == 0 || n > 2 || lenPos + n >= dataLen) break;  // EMV nutzt nur 81/82
      len = 0;
      for (int k = 0; k < n; ++k) {
        len = (len << 8) | data[lenPos + 1 + k];
      }
      lenSize = 1 + n;
    }
    const int valPos = lenPos + lenSize;
    if (len < 0 || valPos > dataLen || len > dataLen - valPos) break;  // OOB-Schutz

    if (currentTag == tag) {
      return tagStart;
    }

    // In konstruierte Templates absteigen (Bit 6 des ersten Tag-Bytes gesetzt:
    // 0x6F, 0xA5, 0x70, 0x77, 0x61, ...). EMV ist stark verschachtelt.
    if (data[tagStart] & 0x20) {
      const int sub = findTag(&data[valPos], len, tag, depth + 1);
      if (sub != -1) return valPos + sub;
    }

    i = valPos + len;   // valPos > i und len >= 0 -> garantierter Fortschritt
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

// ============================================================================
//  Vollstaendiger TLV-Dump aller ueber NFC lesbaren Kartendaten
// ----------------------------------------------------------------------------
//  Quelle der Tag-Namen: EMV-Tag-Liste (vgl. eftlab Knowledge Base). Es sind
//  die gaengigen Tags eingetragen, die kontaktlose Karten liefern; unbekannte
//  Tags werden generisch mit "unbekannt" + Hex-Wert ausgegeben. Erweiterbar:
//  einfach weitere {Tag, "Name"}-Zeilen in kTags ergaenzen.
// ============================================================================
const char* NfcModule::emvTagName(uint32_t tag) {
  struct Entry { uint32_t tag; const char* name; };
  static const Entry kTags[] = {
    {0x42, "Issuer Identification Number (IIN)"},
    {0x4F, "Application Identifier (AID)"},
    {0x50, "Application Label"},
    {0x56, "Track 1 Equivalent Data"},
    {0x57, "Track 2 Equivalent Data"},
    {0x5A, "Application PAN"},
    {0x61, "Application Template"},
    {0x6F, "FCI Template"},
    {0x70, "Record Template"},
    {0x71, "Issuer Script Template 1"},
    {0x72, "Issuer Script Template 2"},
    {0x73, "Directory Discretionary Template"},
    {0x77, "Response Template Format 2"},
    {0x80, "Response Template Format 1"},
    {0x82, "Application Interchange Profile (AIP)"},
    {0x83, "Command Template"},
    {0x84, "DF Name"},
    {0x87, "Application Priority Indicator"},
    {0x88, "Short File Identifier (SFI)"},
    {0x8C, "CDOL1"},
    {0x8D, "CDOL2"},
    {0x8E, "CVM List"},
    {0x8F, "CA Public Key Index"},
    {0x90, "Issuer Public Key Certificate"},
    {0x92, "Issuer Public Key Remainder"},
    {0x93, "Signed Static Application Data"},
    {0x94, "Application File Locator (AFL)"},
    {0x95, "Terminal Verification Results"},
    {0x97, "TDOL"},
    {0x9A, "Transaction Date"},
    {0x9C, "Transaction Type"},
    {0x9D, "DDF Name"},
    {0x5F20, "Cardholder Name"},
    {0x5F24, "Application Expiration Date"},
    {0x5F25, "Application Effective Date"},
    {0x5F28, "Issuer Country Code"},
    {0x5F2A, "Transaction Currency Code"},
    {0x5F2D, "Language Preference"},
    {0x5F30, "Service Code"},
    {0x5F34, "Application PAN Sequence Number"},
    {0x5F36, "Transaction Currency Exponent"},
    {0x5F50, "Issuer URL"},
    {0x5F53, "IBAN"},
    {0x5F54, "Bank Identifier Code (BIC)"},
    {0x5F55, "Issuer Country Code (alpha2)"},
    {0x5F56, "Issuer Country Code (alpha3)"},
    {0x9F05, "Application Discretionary Data"},
    {0x9F07, "Application Usage Control"},
    {0x9F08, "Application Version Number (ICC)"},
    {0x9F0A, "Application Selection Registered Proprietary Data"},
    {0x9F0B, "Cardholder Name Extended"},
    {0x9F0D, "Issuer Action Code - Default"},
    {0x9F0E, "Issuer Action Code - Denial"},
    {0x9F0F, "Issuer Action Code - Online"},
    {0x9F10, "Issuer Application Data"},
    {0x9F11, "Issuer Code Table Index"},
    {0x9F12, "Application Preferred Name"},
    {0x9F13, "Last Online ATC Register"},
    {0x9F14, "Lower Consecutive Offline Limit"},
    {0x9F17, "PIN Try Counter"},
    {0x9F19, "Token Requestor ID"},
    {0x9F1A, "Terminal Country Code"},
    {0x9F1F, "Track 1 Discretionary Data"},
    {0x9F20, "Track 2 Discretionary Data"},
    {0x9F23, "Upper Consecutive Offline Limit"},
    {0x9F24, "Payment Account Reference (PAR)"},
    {0x9F26, "Application Cryptogram"},
    {0x9F27, "Cryptogram Information Data"},
    {0x9F32, "Issuer Public Key Exponent"},
    {0x9F36, "Application Transaction Counter (ATC)"},
    {0x9F37, "Unpredictable Number"},
    {0x9F38, "PDOL"},
    {0x9F42, "Application Currency Code"},
    {0x9F44, "Application Currency Exponent"},
    {0x9F45, "Data Authentication Code"},
    {0x9F46, "ICC Public Key Certificate"},
    {0x9F47, "ICC Public Key Exponent"},
    {0x9F48, "ICC Public Key Remainder"},
    {0x9F49, "DDOL"},
    {0x9F4A, "Static Data Authentication Tag List"},
    {0x9F4B, "Signed Dynamic Application Data"},
    {0x9F4C, "ICC Dynamic Number"},
    {0x9F4D, "Log Entry"},
    {0x9F4E, "Merchant Name and Location"},
    {0x9F4F, "Log Format"},
    {0x9F50, "Offline Accumulator Balance (prop.)"},
    {0x9F51, "Application Currency Code (prop.)"},
    {0x9F5A, "Application Program Identifier"},
    {0x9F60, "CVC3 (Track1)"},
    {0x9F61, "CVC3 (Track2)"},
    {0x9F62, "PCVC3 (Track1)"},
    {0x9F63, "PUNATC (Track1)"},
    {0x9F64, "NATC (Track1)"},
    {0x9F65, "PCVC3 (Track2)"},
    {0x9F66, "Terminal Transaction Qualifiers (TTQ)"},
    {0x9F67, "NATC (Track2) / MSD Offset"},
    {0x9F68, "Card Additional Processes"},
    {0x9F69, "Card Authentication Related Data (UDOL)"},
    {0x9F6B, "Track 2 Data (MSD)"},
    {0x9F6C, "Card Transaction Qualifiers (CTQ)"},
    {0x9F6E, "Form Factor Indicator / 3rd Party Data"},
    {0x9F7C, "Customer Exclusive Data"},
    {0xA5, "FCI Proprietary Template"},
    {0xBF0C, "FCI Issuer Discretionary Data"},
  };
  for (const auto& e : kTags) {
    if (e.tag == tag) return e.name;
  }
  return "unbekannt";
}

void NfcModule::printTagHex(uint32_t tag) {
  const int nb = (tag > 0xFFFFFF) ? 4 : (tag > 0xFFFF) ? 3 : (tag > 0xFF) ? 2 : 1;
  for (int k = nb - 1; k >= 0; --k) {
    const uint8_t b = (tag >> (8 * k)) & 0xFF;
    if (b < 0x10) Serial.print('0');
    Serial.print(b, HEX);
  }
}

void NfcModule::decodeHint(uint32_t tag, const uint8_t* val, int len, int depth) {
  if (val == nullptr || len <= 0) return;

  for (int d = 0; d < depth + 1; ++d) Serial.print("  ");

  // 1. Text / ASCII (ans)
  if (tag == 0x50 || tag == 0x9F12) {            // Application Label / Application Preferred Name
    Serial.print("-> Text: \"");
    for (int i = 0; i < len; ++i) {
      char c = static_cast<char>(val[i]);
      Serial.print((c >= 0x20 && c <= 0x7E) ? c : '.');
    }
    Serial.println("\"");
    return;
  }
  if (tag == 0x5F20 || tag == 0x9F0B) {          // Cardholder Name / Extended
    Serial.print("-> Karteninhaber: \"");
    for (int i = 0; i < len; ++i) {
      char c = static_cast<char>(val[i]);
      Serial.print((c >= 0x20 && c <= 0x7E) ? c : '.');
    }
    Serial.println("\"");
    return;
  }
  if (tag == 0x5F2D) {                           // Language Preference (2-stellige ISO 639 Codes, z.B. "deda")
    Serial.print("-> Sprache(n): \"");
    for (int i = 0; i < len; ++i) {
      char c = static_cast<char>(val[i]);
      Serial.print((c >= 0x20 && c <= 0x7E) ? c : '.');
      if (i % 2 == 1 && i + 1 < len) {
        Serial.print(',');
      }
    }
    Serial.println("\"");
    return;
  }
  if (tag == 0x5F50) {                           // Issuer URL
    Serial.print("-> Issuer URL: \"");
    for (int i = 0; i < len; ++i) {
      char c = static_cast<char>(val[i]);
      Serial.print((c >= 0x20 && c <= 0x7E) ? c : '.');
    }
    Serial.println("\"");
    return;
  }
  if (tag == 0x5F53) {                           // IBAN
    Serial.print("-> IBAN: \"");
    for (int i = 0; i < len; ++i) {
      char c = static_cast<char>(val[i]);
      Serial.print((c >= 0x20 && c <= 0x7E) ? c : '.');
    }
    Serial.println("\"");
    return;
  }
  if (tag == 0x5F54) {                           // BIC
    Serial.print("-> BIC: \"");
    for (int i = 0; i < len; ++i) {
      char c = static_cast<char>(val[i]);
      Serial.print((c >= 0x20 && c <= 0x7E) ? c : '.');
    }
    Serial.println("\"");
    return;
  }
  if (tag == 0x9F24) {                           // Payment Account Reference (PAR)
    Serial.print("-> PAR: \"");
    for (int i = 0; i < len; ++i) {
      char c = static_cast<char>(val[i]);
      Serial.print((c >= 0x20 && c <= 0x7E) ? c : '.');
    }
    Serial.println("\"");
    return;
  }

  // 2. Nummern & Datumsangaben (BCD / n)
  if (tag == 0x5A) {                             // Application PAN
    Serial.print("-> Kartennummer (PAN): ");
    for (int i = 0; i < len; ++i) {
      const uint8_t hi = (val[i] >> 4) & 0x0F, lo = val[i] & 0x0F;
      if (hi <= 9) Serial.print(hi);
      if (lo <= 9) Serial.print(lo);
    }
    Serial.println();
    return;
  }
  if (tag == 0x57 || tag == 0x9F6B) {            // Track 2 Equivalent Data
    String pan, exp;
    parseTrack2(val, static_cast<uint8_t>(len), pan, exp);
    Serial.print("-> Track 2 (PAN: ");
    Serial.print(pan);
    Serial.print(", Ablauf JJ/MM: ");
    Serial.print(exp);
    Serial.println(")");
    return;
  }
  if (tag == 0x5F24 || tag == 0x5F25) {          // Expiration / Effective Date (JJMMTT BCD)
    if (len >= 3) {
      char b[16];
      snprintf(b, sizeof(b), "%02X/%02X/%02X", val[0], val[1], val[2]);
      Serial.print((tag == 0x5F24) ? "-> Ablaufdatum (JJ/MM/TT): " : "-> Gueltig ab (JJ/MM/TT): ");
      Serial.println(b);
    } else if (len >= 2) {
      char b[16];
      snprintf(b, sizeof(b), "%02X/%02X", val[0], val[1]);
      Serial.print("-> Datum (JJ/MM): ");
      Serial.println(b);
    } else {
      Serial.println();
    }
    return;
  }
  if (tag == 0x5F34) {                           // PAN Sequence Number (PSN)
    Serial.print("-> Kartenfolgenummer (PSN): ");
    Serial.println(val[0], DEC);
    return;
  }
  if (tag == 0x5F30) {                           // Service Code (3 BCD-Ziffern)
    if (len >= 2) {
      Serial.printf("-> Service Code: %02X%01X\n", val[0], (val[1] >> 4) & 0x0F);
    }
    return;
  }

  // 3. Währungen und Länder (ISO 4217 / ISO 3166-1 numerisch BCD)
  if (tag == 0x5F28 || tag == 0x9F1A) {          // Country Code
    if (len >= 2) {
      char cbuf[8];
      snprintf(cbuf, sizeof(cbuf), "%02X%02X", val[0], val[1]);
      Serial.print("-> Land: ");
      Serial.print(cbuf);
      if (strcmp(cbuf, "0276") == 0) Serial.print(" (Deutschland / DE)");
      else if (strcmp(cbuf, "0040") == 0) Serial.print(" (Oesterreich / AT)");
      else if (strcmp(cbuf, "0756") == 0) Serial.print(" (Schweiz / CH)");
      else if (strcmp(cbuf, "0840") == 0) Serial.print(" (USA / US)");
      else if (strcmp(cbuf, "0826") == 0) Serial.print(" (Grossbritannien / UK)");
      Serial.println();
    }
    return;
  }
  if (tag == 0x5F2A || tag == 0x9F42 || tag == 0x9F51) { // Currency Code
    if (len >= 2) {
      char cbuf[8];
      snprintf(cbuf, sizeof(cbuf), "%02X%02X", val[0], val[1]);
      Serial.print("-> Waehrung: ");
      Serial.print(cbuf);
      if (strcmp(cbuf, "0978") == 0) Serial.print(" (EUR / Euro)");
      else if (strcmp(cbuf, "0840") == 0) Serial.print(" (USD / US Dollar)");
      else if (strcmp(cbuf, "0826") == 0) Serial.print(" (GBP / Britische Pfund)");
      else if (strcmp(cbuf, "0756") == 0) Serial.print(" (CHF / Schweizer Franken)");
      Serial.println();
    }
    return;
  }
  if (tag == 0x5F36 || tag == 0x9F44) {          // Currency Exponent (Dezimalstellen)
    Serial.printf("-> Nachkommastellen (Exponent): %d\n", val[0]);
    return;
  }

  // 4. Integer-Zähler & Grenzwerte (Binär-Ganzzahlen dezimal formatiert)
  if (tag == 0x9F36 || tag == 0x9F17 || tag == 0x9F13 ||
      tag == 0x9F14 || tag == 0x9F23) {
    uint32_t v = 0;
    for (int i = 0; i < len && i < 4; ++i) v = (v << 8) | val[i];
    if (tag == 0x9F36) Serial.print("-> Transaktionszaehler (ATC): ");
    else if (tag == 0x9F17) Serial.print("-> Verbleibende PIN-Versuche: ");
    else if (tag == 0x9F13) Serial.print("-> Letzter Online-ATC: ");
    else Serial.print("-> Zaehlerwert (dez): ");
    Serial.println(v);
    return;
  }

  // 5. Versionsnummern & Indizes
  if (tag == 0x9F08) {                           // Application Version Number
    if (len >= 2) {
      Serial.printf("-> Version: %d.%d (0x%02X%02X)\n", val[0], val[1], val[0], val[1]);
    }
    return;
  }
  if (tag == 0x87) {                             // Application Priority Indicator
    Serial.printf("-> Prioritaet: %d%s\n", val[0] & 0x0F,
                  (val[0] & 0x80) ? " (kann ohne Bestaetigung gewaehlt werden)" : "");
    return;
  }

  // 6. Bitmasken & Profile (AIP, AUC, etc.)
  if (tag == 0x82 && len >= 2) {                 // AIP
    Serial.print("-> AIP Faehigkeiten:");
    if (val[0] & 0x40) Serial.print(" [SDA]");
    if (val[0] & 0x20) Serial.print(" [DDA]");
    if (val[0] & 0x10) Serial.print(" [Karteninhaber-Verif.]");
    if (val[0] & 0x01) Serial.print(" [CDA]");
    Serial.println();
    return;
  }
  if (tag == 0x9F07 && len >= 2) {               // Application Usage Control (AUC)
    Serial.print("-> Erlaubte Nutzung:");
    if (val[0] & 0x80) Serial.print(" [Geldautomaten]");
    if (val[0] & 0x40) Serial.print(" [POS/Terminals]");
    if (val[0] & 0x08) Serial.print(" [Inland]");
    if (val[0] & 0x04) Serial.print(" [Ausland]");
    Serial.println();
    return;
  }
}

void NfcModule::dumpTlv(const uint8_t* data, int dataLen, int depth) {
  if (depth > 8 || data == nullptr || dataLen <= 0) return;

  int i = 0;
  while (i < dataLen) {
    // --- Tag ---
    uint32_t tag = data[i];
    int tSize = 1;
    if ((data[i] & 0x1F) == 0x1F) {
      if (i + 1 >= dataLen) break;
      tag = (tag << 8) | data[i + 1];
      tSize = 2;
      while ((data[i + tSize - 1] & 0x80) && (i + tSize) < dataLen) {
        tag = (tag << 8) | data[i + tSize];
        ++tSize;
      }
    }
    if (i + tSize >= dataLen) break;

    // --- Laenge (Kurz-/Langform, streng validiert) ---
    const int lenPos = i + tSize;
    int len = data[lenPos];
    int lenSize = 1;
    if (len & 0x80) {
      const int n = len & 0x7F;
      if (n == 0 || n > 2 || lenPos + n >= dataLen) break;
      len = 0;
      for (int k = 0; k < n; ++k) len = (len << 8) | data[lenPos + 1 + k];
      lenSize = 1 + n;
    }
    const int valPos = lenPos + lenSize;
    if (len < 0 || valPos > dataLen || len > dataLen - valPos) break;

    for (int d = 0; d < depth; ++d) Serial.print("  ");
    printTagHex(tag);
    Serial.print(" [");
    Serial.print(emvTagName(tag));
    Serial.print("] len ");
    Serial.print(len);

    if ((data[i] & 0x20) != 0) {           // konstruiert -> absteigen
      Serial.println();
      dumpTlv(&data[valPos], len, depth + 1);
    } else {                               // primitiv -> Wert ausgeben
      Serial.print(": ");
      for (int k = 0; k < len; ++k) {
        const uint8_t b = data[valPos + k];
        if (b < 0x10) Serial.print('0');
        Serial.print(b, HEX);
        if (k + 1 < len) Serial.print(' ');
      }
      bool printable = (len > 0);
      for (int k = 0; k < len; ++k) {
        const uint8_t b = data[valPos + k];
        if (b < 0x20 || b > 0x7E) { printable = false; break; }
      }
      if (printable) {
        Serial.print("  \"");
        for (int k = 0; k < len; ++k) Serial.print(static_cast<char>(data[valPos + k]));
        Serial.print("\"");
      }
      Serial.println();
      decodeHint(tag, &data[valPos], len, depth);
    }

    i = valPos + len;
  }
}

bool NfcModule::getDataTag(uint32_t tag, uint8_t* out, uint8_t& outLen) {
  uint8_t cmd[5] = { 0x80, 0xCA,
                     static_cast<uint8_t>((tag >> 8) & 0xFF),
                     static_cast<uint8_t>(tag & 0xFF), 0x00 };
  uint8_t resp[MaxBufferLength];
  uint8_t rlen = sizeof(resp);
  const bool ok = _reader.inDataExchange(cmd, sizeof(cmd), resp, &rlen);
  if (!ok || rlen < 2 || resp[rlen - 2] != 0x90 || resp[rlen - 1] != 0x00) {
    outLen = 0;
    return false;
  }
  uint8_t n = rlen - 2;
  if (n > outLen) n = outLen;
  memcpy(out, resp, n);
  outLen = n;
  return true;
}

void NfcModule::readAndDumpRecords(const uint8_t* afl, uint8_t aflLen) {
  if (afl == nullptr || aflLen < 4) {
    Serial.println("  (kein gueltiges AFL)");
    return;
  }
  uint8_t resp[MaxBufferLength];
  for (int i = 0; i + 3 < aflLen; i += 4) {
    const uint8_t sfi = (afl[i] >> 3) & 0x1F;
    const uint8_t firstRec = afl[i + 1];
    const uint8_t lastRec = afl[i + 2];
    for (uint8_t rec = firstRec; rec >= firstRec && rec <= lastRec; ++rec) {
      uint8_t cmd[] = { 0x00, 0xB2, rec,
                        static_cast<uint8_t>((sfi << 3) | 0x04), 0x00 };
      uint8_t rlen = sizeof(resp);
      const bool ok = _reader.inDataExchange(cmd, sizeof(cmd), resp, &rlen);
      Serial.print("  SFI ");
      Serial.print(sfi);
      Serial.print(" Record ");
      Serial.print(rec);
      if (ok && rlen >= 2 && resp[rlen - 2] == 0x90 && resp[rlen - 1] == 0x00) {
        Serial.println(":");
        dumpTlv(resp, rlen - 2, 1);
      } else {
        Serial.println(": nicht lesbar");
      }
      if (rec == 0xFF) break;              // Ueberlaufschutz
    }
  }
}

void NfcModule::dumpGetDataTags() {
  const uint32_t tags[] = { 0x9F36, 0x9F17, 0x9F13, 0x9F4F };
  for (uint32_t t : tags) {
    uint8_t buf[MaxBufferLength];
    uint8_t len = sizeof(buf);
    if (getDataTag(t, buf, len) && len > 0) {
      dumpTlv(buf, len, 0);
    } else {
      Serial.print("  (");
      printTagHex(t);
      Serial.print(" ");
      Serial.print(emvTagName(t));
      Serial.println("): nicht verfuegbar");
    }
  }
}

void NfcModule::dumpAllCardData(const uint8_t* fci, uint8_t fciLen,
                                const uint8_t* gpo, uint8_t gpoLen,
                                const uint8_t* afl, uint8_t aflLen, bool aflValid) {
  Serial.println("\n################ VOLLSTAENDIGER KARTEN-DUMP ################");
  Serial.println("---- FCI (SELECT-Antwort) ----");
  dumpTlv(fci, fciLen, 0);
  Serial.println("---- GPO-Antwort ----");
  if (gpoLen > 0) dumpTlv(gpo, gpoLen, 0);
  else Serial.println("  (leer)");
  Serial.println("---- Records (AFL) ----");
  if (aflValid) readAndDumpRecords(afl, aflLen);
  else Serial.println("  (kein AFL - Daten liegen inline in der GPO-Antwort)");
  Serial.println("---- GET DATA Einzel-Tags ----");
  dumpGetDataTags();
  Serial.println("################ ENDE DUMP ################\n");
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
  obj["lastCardholder"] = _lastCardholder;
  obj["lastCountryCode"] = _lastCountryCode;
  obj["lastCurrencyCode"] = _lastCurrencyCode;
  obj["lastEffectiveDate"] = _lastEffectiveDate;
  obj["lastPsn"] = _lastPsn;
  obj["lastServiceCode"] = _lastServiceCode;
  obj["lastAip"] = _lastAip;
  obj["cardReadSuccess"] = _cardReadSuccess;
  obj["emulating"] = _emulating;   // Emulationsmodus aktiv?
  obj["emuReady"] = _emuValid;     // liegt ein Mitschnitt zum Zuruckspielen vor?
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
    if (!_emuValid) {
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
    _lastUid = "";
    _lastAid = "";
    _lastCardType = "";
    _lastCardNumber = "";
    _lastExpiry = "";
    _lastCardholder = "";
    _lastCountryCode = "";
    _lastCurrencyCode = "";
    _lastEffectiveDate = "";
    _lastPsn = "";
    _lastServiceCode = "";
    _lastAip = "";
    _cardReadSuccess = false;
    return true;
  }

  return false;
}

// ============================================================================
//  Karten-Emulation (Capture & Replay)
// ============================================================================

void NfcModule::resetEmuCapture() {
  _emuValid = false;
  _emuPpseLen = 0;
  _emuAidLen = 0;
  _emuFciLen = 0;
  _emuGpoLen = 0;
  _emuRecordCount = 0;
}

void NfcModule::captureBlob(uint8_t* dst, uint8_t& dstLen, const uint8_t* src, int srcLen) {
  if (srcLen < 0) srcLen = 0;
  if (srcLen > EmuBlobCap) srcLen = EmuBlobCap;  // Kartendaten nie ungeprueft
  memcpy(dst, src, srcLen);
  dstLen = static_cast<uint8_t>(srcLen);
}

void NfcModule::captureRecord(uint8_t sfi, uint8_t rec, const uint8_t* data, int len) {
  if (len <= 0) return;
  if (len > EmuBlobCap) len = EmuBlobCap;
  // Schon vorhanden? (z. B. AFL und Fallback-Schleife treffen denselben Record)
  for (uint8_t i = 0; i < _emuRecordCount; ++i) {
    if (_emuRecords[i].sfi == sfi && _emuRecords[i].rec == rec) return;
  }
  if (_emuRecordCount >= EmuMaxRecords) return;  // Puffer voll -> ignorieren
  EmuRecord& r = _emuRecords[_emuRecordCount++];
  r.sfi = sfi;
  r.rec = rec;
  r.len = static_cast<uint8_t>(len);
  memcpy(r.data, data, len);
}

uint8_t NfcModule::buildSynthPpse(uint8_t* out, uint8_t outCap) {
  // Minimale PPSE-FCI, die genau unsere eine AID anpreist:
  //   6F L1  84 0E <"2PAY.SYS.DDC01">  A5 L2  BF0C L3  61 L4  4F <len> <AID>  87 01 01
  if (_emuAidLen == 0 || _emuAidLen > 16) return 0;
  static const uint8_t ppseName[14] = {
      '2','P','A','Y','.','S','Y','S','.','D','D','C','0','1'};

  uint8_t rec61[32];  // 61-Template-Inhalt: 4F <len> <AID> 87 01 01
  uint8_t n = 0;
  rec61[n++] = 0x4F;
  rec61[n++] = _emuAidLen;
  memcpy(&rec61[n], _emuAid, _emuAidLen); n += _emuAidLen;
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
      if (_emuPpseLen > 0) return emit(_emuPpse, _emuPpseLen, 0x90, 0x00);
      uint8_t synth[96];
      uint8_t sl = buildSynthPpse(synth, sizeof(synth));
      if (sl > 0) return emit(synth, sl, 0x90, 0x00);
      return emit(nullptr, 0, 0x6A, 0x82);  // Datei nicht gefunden
    }
    // SELECT AID -> wir emulieren genau eine Applikation: FCI zurueckgeben.
    if (_emuFciLen > 0) return emit(_emuFci, _emuFciLen, 0x90, 0x00);
    return emit(nullptr, 0, 0x6A, 0x82);
  }

  // GET PROCESSING OPTIONS (80 A8 ..)
  if (c[0] == 0x80 && ins == 0xA8) {
    if (_emuGpoLen > 0) return emit(_emuGpo, _emuGpoLen, 0x90, 0x00);
    return emit(nullptr, 0, 0x6A, 0x81);  // Funktion nicht unterstuetzt
  }

  // READ RECORD (00 B2 rec (sfi<<3|4) ..)
  if (ins == 0xB2) {
    const uint8_t rec = p1;
    const uint8_t sfi = (p2 >> 3) & 0x1F;
    for (uint8_t i = 0; i < _emuRecordCount; ++i) {
      if (_emuRecords[i].sfi == sfi && _emuRecords[i].rec == rec) {
        return emit(_emuRecords[i].data, _emuRecords[i].len, 0x90, 0x00);
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
    printHex(capdu, (n > 16) ? 16 : static_cast<uint8_t>(n));
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

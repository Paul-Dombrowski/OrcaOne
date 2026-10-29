#include "NfcModule.h"
#include "EmvDump.h"
#include "EmvTags.h"
#include "EmvTypes.h"
#include "Tlv.h"

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
  _card.uid = String(uidStr);

  Serial.println("\n====================================");
  Serial.print("[INFO] Kontaktlose Karte erkannt! UID: ");
  Serial.println(_card.uid);

  processCard();

  Serial.println("====================================\n");
}

void NfcModule::processCard() {
  // _card.uid wurde bereits in update() gesetzt - NICHT per _card.clear()
  // ueberschreiben (Original-Verhalten liess _lastUid hier unangetastet).
  _card.readSuccess = false;
  _capture.reset();  // Mitschnitt fuer die Emulation frisch beginnen
  _card.aid = "";
  _card.cardNumber = "";
  _card.expiry = "";
  _card.cardType = "";
  _card.cardholder = "";
  _card.countryCode = "";
  _card.currencyCode = "";
  _card.effectiveDate = "";
  _card.psn = "";
  _card.serviceCode = "";
  _card.aip = "";

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
    Tlv::printHex(item.aid, item.len);
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
    _card.aid = String(aidHex);
    if (strlen(item.label) > 0) {
      _card.cardType = String(item.label);
    }

    // Fuer die Emulation: AID + rohes FCI dieser Applikation mitschneiden.
    _capture.aidLen = (item.len <= sizeof(_capture.aid)) ? item.len : sizeof(_capture.aid);
    memcpy(_capture.aid, item.aid, _capture.aidLen);
    _capture.putBlob(_capture.fci, _capture.fciLen, fci, fciLen);

    // 3. GET PROCESSING OPTIONS (GPO) senden
    uint8_t afl[64] = {0};
    uint8_t aflLen = 0;
    uint8_t gpo[MaxBufferLength];
    uint8_t gpoLen = 0;
    bool aflFound = tryGetProcessingOptions(fci, fciLen, afl, aflLen, gpo, gpoLen);

    // Fuer die Emulation: rohe GPO-Antwort mitschneiden (enthaelt bei qVSDC
    // bereits Track 2 / PAN, sonst AIP + AFL).
    if (gpoLen > 0) _capture.putBlob(_capture.gpo, _capture.gpoLen, gpo, gpoLen);

    // 4. Kartendaten gewinnen.
    //    a) Viele kontaktlose Karten (qVSDC, z. B. Visa) liefern Track 2 / PAN
    //       direkt in der GPO-Antwort (Tag 57), ohne AFL. Das ist die
    //       massgebliche Quelle - zuerst dort suchen.
    bool gotData = false;
    if (gpoLen > 0) {
      gotData = parseRecordData(_card, gpo, gpoLen);
    }
    //    b) Sonst klassisch ueber das AFL die Records lesen.
    if (!gotData) {
      gotData = readCardRecords(aflFound ? afl : nullptr, aflLen);
    }

    if (gotData) {
      _card.readSuccess = true;
      _capture.valid = true;  // Mitschnitt ist brauchbar -> Emulation moeglich
      Serial.println("[SUCCESS] Kartendaten erfolgreich gelesen!");
      // Vollstaendiger Dump aller ueber NFC lesbaren Daten dieser Applikation.
      _dump.dumpAll(fci, fciLen, gpo, gpoLen, afl, aflLen, aflFound);
      break;
    } else {
      Serial.println("[HINWEIS] Keine lesbaren Record-Daten unter dieser AID.");
    }
  }

  if (!_card.readSuccess) {
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
  Tlv::printHex(response, responseLength);
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
  _capture.putBlob(_capture.ppse, _capture.ppseLen, response, responseLength - 2);
  Tlv::extractAllAids(response, responseLength - 2, aidList);
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
  Tlv::printHex(response, responseLength);
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
  int pdolIdx = (fci && fciLen) ? Tlv::findTag(fci, fciLen, 0x9F38) : -1;
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
  Tlv::printHex(response, responseLength);
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

  int aflIdx = Tlv::findTag(body, bodyLen, 0x94);
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
  int format1Idx = Tlv::findTag(response, dataLen, 0x80);
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
          _capture.putRecord(sfi, rec, response, responseLength - 2);  // fuer Emulation
          if (parseRecordData(_card, response, responseLength - 2)) {
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
        _capture.putRecord(sfi, rec, response, responseLength - 2);  // fuer Emulation
        if (parseRecordData(_card, response, responseLength - 2)) {
          return true;
        }
      }
    }
  }

  return foundData;
}

bool NfcModule::parseRecordData(CardData& out, const uint8_t* data, uint8_t dataLen) {
  bool parsedAnything = false;

  // 1. Tag 0x5A: PAN (Primäre Kontonummer)
  int panIdx = Tlv::findTag(data, dataLen, 0x5A);
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
      out.cardNumber = panStr;
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
  int expIdx = Tlv::findTag(data, dataLen, 0x5F24);
  if (expIdx != -1) {
    const uint8_t* expData = &data[expIdx + 3];
    char expStr[8];
    snprintf(expStr, sizeof(expStr), "%02d/%02d",
             ((expData[0] >> 4) & 0x0F) * 10 + (expData[0] & 0x0F),
             ((expData[1] >> 4) & 0x0F) * 10 + (expData[1] & 0x0F));
    out.expiry = String(expStr);
    parsedAnything = true;
    Serial.print(">> ABLAUFDATUM (Tag 0x5F24): ");
    Serial.println(out.expiry);
  }

  // 3. Tag 0x57: Track 2 Equivalent Data (oft vorhanden, wenn 0x5A nicht direkt existiert)
  int tr2Idx = Tlv::findTag(data, dataLen, 0x57);
  if (tr2Idx != -1) {
    uint8_t tr2Len = data[tr2Idx + 1];
    String tr2Pan = "";
    String tr2Exp = "";
    Tlv::parseTrack2(&data[tr2Idx + 2], tr2Len, tr2Pan, tr2Exp);

    if (out.cardNumber.length() == 0 && tr2Pan.length() >= 12) {
      out.cardNumber = tr2Pan;
      parsedAnything = true;
      Serial.print(">> KARTENNUMMER (Track 2): ");
      for (size_t i = 0; i < tr2Pan.length(); ++i) {
        Serial.print(tr2Pan[i]);
        if ((i % 4 == 3) && (i + 1 < tr2Pan.length())) Serial.print(' ');
      }
      Serial.println();
    }
    if (out.expiry.length() == 0 && tr2Exp.length() > 0) {
      out.expiry = tr2Exp;
      parsedAnything = true;
      Serial.print(">> ABLAUFDATUM (Track 2): ");
      Serial.println(out.expiry);
    }
  }

  // 4. Tag 0x5F20: Cardholder Name (ASCII)
  int nameIdx = Tlv::findTag(data, dataLen, 0x5F20);
  if (nameIdx != -1) {
    uint8_t nameLen = data[nameIdx + 2]; // 2-Byte Tag -> Laenge bei +2
    const uint8_t* namePtr = &data[nameIdx + 3];
    char nameBuf[33] = {0};
    uint8_t copyLen = (nameLen < sizeof(nameBuf) - 1) ? nameLen : (sizeof(nameBuf) - 1);
    memcpy(nameBuf, namePtr, copyLen);
    out.cardholder = String(nameBuf);
    out.cardholder.trim();
    if (out.cardholder.length() > 0) {
      parsedAnything = true;
      Serial.print(">> KARTENINHABER (Tag 0x5F20): ");
      Serial.println(out.cardholder);
    }
  }

  // 5. Tag 0x5F28: Issuer Country Code (ISO 3166-1 numerisch, 2 Bytes BCD, z.B. 0276 = DE)
  int ccIdx = Tlv::findTag(data, dataLen, 0x5F28);
  if (ccIdx != -1) {
    const uint8_t* ccData = &data[ccIdx + 3];
    char ccBuf[8];
    snprintf(ccBuf, sizeof(ccBuf), "%02X%02X", ccData[0], ccData[1]);
    out.countryCode = String(ccBuf);
    parsedAnything = true;
    Serial.print(">> LAENDER-CODE (Tag 0x5F28): ");
    Serial.print(out.countryCode);
    if (out.countryCode == "0276") Serial.print(" (Deutschland)");
    else if (out.countryCode == "0040") Serial.print(" (Oesterreich)");
    else if (out.countryCode == "0756") Serial.print(" (Schweiz)");
    Serial.println();
  }

  // 6. Tag 0x5F25: Application Effective Date (Gueltig ab, JJMMTT BCD)
  int effIdx = Tlv::findTag(data, dataLen, 0x5F25);
  if (effIdx != -1) {
    const uint8_t* effData = &data[effIdx + 3];
    char effBuf[16];
    snprintf(effBuf, sizeof(effBuf), "%02X/%02X/%02X", effData[0], effData[1], effData[2]);
    out.effectiveDate = String(effBuf);
    parsedAnything = true;
    Serial.print(">> GUELTIG AB (Tag 0x5F25 JJ/MM/TT): ");
    Serial.println(out.effectiveDate);
  }

  // 7. Tag 0x5F30: Service Code (3 Ziffern, Track 2 Service Attributes)
  int scIdx = Tlv::findTag(data, dataLen, 0x5F30);
  if (scIdx != -1) {
    const uint8_t* scData = &data[scIdx + 3];
    char scBuf[8];
    snprintf(scBuf, sizeof(scBuf), "%02X%01X", scData[0], (scData[1] >> 4) & 0x0F);
    out.serviceCode = String(scBuf);
    parsedAnything = true;
    Serial.print(">> SERVICE CODE (Tag 0x5F30): ");
    Serial.println(out.serviceCode);
  }

  // 8. Tag 0x5F34: PAN Sequence Number (PSN, 1 Byte)
  int psnIdx = Tlv::findTag(data, dataLen, 0x5F34);
  if (psnIdx != -1) {
    uint8_t psnVal = data[psnIdx + 3];
    char psnBuf[6];
    snprintf(psnBuf, sizeof(psnBuf), "%02X", psnVal);
    out.psn = String(psnBuf);
    parsedAnything = true;
    Serial.print(">> KARTENFOLGENUMMER / PSN (Tag 0x5F34): ");
    Serial.println(out.psn);
  }

  // 9. Tag 0x9F42: Application Currency Code (ISO 4217, z.B. 0978 = EUR)
  int currIdx = Tlv::findTag(data, dataLen, 0x9F42);
  if (currIdx != -1) {
    const uint8_t* currData = &data[currIdx + 3];
    char currBuf[8];
    snprintf(currBuf, sizeof(currBuf), "%02X%02X", currData[0], currData[1]);
    out.currencyCode = String(currBuf);
    parsedAnything = true;
    Serial.print(">> WAEHRUNGS-CODE (Tag 0x9F42): ");
    Serial.print(out.currencyCode);
    if (out.currencyCode == "0978") Serial.print(" (EUR)");
    else if (out.currencyCode == "0840") Serial.print(" (USD)");
    else if (out.currencyCode == "0826") Serial.print(" (GBP)");
    else if (out.currencyCode == "0756") Serial.print(" (CHF)");
    Serial.println();
  }

  // 10. Tag 0x82: Application Interchange Profile (AIP, 2 Bytes)
  int aipIdx = Tlv::findTag(data, dataLen, 0x82);
  if (aipIdx != -1) {
    uint8_t aip1 = data[aipIdx + 2];
    uint8_t aip2 = data[aipIdx + 3];
    char aipBuf[16];
    snprintf(aipBuf, sizeof(aipBuf), "%02X%02X", aip1, aip2);
    out.aip = String(aipBuf);
    parsedAnything = true;
    Serial.print(">> AIP (Tag 0x82): 0x");
    Serial.println(out.aip);
    if (aip1 & 0x40) Serial.println("    [+] SDA (Static Data Authentication)");
    if (aip1 & 0x20) Serial.println("    [+] DDA (Dynamic Data Authentication)");
    if (aip1 & 0x10) Serial.println("    [+] Cardholder Verification");
    if (aip1 & 0x01) Serial.println("    [+] CDA (Combined DDA/AC Generation)");
  }

  // 11. Tag 0x8E: Cardholder Verification Method (CVM) List
  int cvmIdx = Tlv::findTag(data, dataLen, 0x8E);
  if (cvmIdx != -1) {
    uint8_t cvmLen = data[cvmIdx + 1];
    Serial.print(">> CVM-LISTE (Tag 0x8E, ");
    Serial.print(cvmLen);
    Serial.print(" Byte): ");
    Tlv::printHex(&data[cvmIdx + 2], cvmLen);
    Serial.println();
  }

  // 12. Tag 0x9F07: Application Usage Control (AUC, 2 Bytes)
  int aucIdx = Tlv::findTag(data, dataLen, 0x9F07);
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
  int verIdx = Tlv::findTag(data, dataLen, 0x9F08);
  if (verIdx != -1) {
    Serial.printf(">> APP VERSION (Tag 0x9F08): %02X%02X\n", data[verIdx + 3], data[verIdx + 4]);
  }

  return parsedAnything;
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

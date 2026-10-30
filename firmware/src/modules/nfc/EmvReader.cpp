#include "EmvReader.h"

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

bool EmvReader::readCard(CardData& out, EmvCapture& cap) {
  // out.uid wurde bereits in update() gesetzt - NICHT per out.clear()
  // ueberschreiben (Original-Verhalten liess _lastUid hier unangetastet).
  out.readSuccess = false;
  cap.reset();  // Mitschnitt fuer die Emulation frisch beginnen
  out.aid = "";
  out.cardNumber = "";
  out.expiry = "";
  out.cardType = "";
  out.cardholder = "";
  out.countryCode = "";
  out.currencyCode = "";
  out.effectiveDate = "";
  out.psn = "";
  out.serviceCode = "";
  out.aip = "";

  std::vector<EmvAid> aidList;

  // 1. PPSE Directory (2PAY.SYS.DDC01) abfragen
  bool ppseOk = selectPpse(aidList, cap);

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
    out.aid = String(aidHex);
    if (strlen(item.label) > 0) {
      out.cardType = String(item.label);
    }

    // Fuer die Emulation: AID + rohes FCI dieser Applikation mitschneiden.
    cap.aidLen = (item.len <= sizeof(cap.aid)) ? item.len : sizeof(cap.aid);
    memcpy(cap.aid, item.aid, cap.aidLen);
    cap.putBlob(cap.fci, cap.fciLen, fci, fciLen);

    // 3. GET PROCESSING OPTIONS (GPO) senden
    uint8_t afl[64] = {0};
    uint8_t aflLen = 0;
    uint8_t gpo[MaxBufferLength];
    uint8_t gpoLen = 0;
    bool aflFound = tryGetProcessingOptions(fci, fciLen, afl, aflLen, gpo, gpoLen);

    // Fuer die Emulation: rohe GPO-Antwort mitschneiden (enthaelt bei qVSDC
    // bereits Track 2 / PAN, sonst AIP + AFL).
    if (gpoLen > 0) cap.putBlob(cap.gpo, cap.gpoLen, gpo, gpoLen);

    // 4. Kartendaten gewinnen.
    //    a) Viele kontaktlose Karten (qVSDC, z. B. Visa) liefern Track 2 / PAN
    //       direkt in der GPO-Antwort (Tag 57), ohne AFL. Das ist die
    //       massgebliche Quelle - zuerst dort suchen.
    bool gotData = false;
    if (gpoLen > 0) {
      gotData = parseRecordData(out, gpo, gpoLen);
    }
    //    b) Sonst klassisch ueber das AFL die Records lesen.
    if (!gotData) {
      gotData = readCardRecords(aflFound ? afl : nullptr, aflLen, out, cap);
    }

    if (gotData) {
      out.readSuccess = true;
      cap.valid = true;  // Mitschnitt ist brauchbar -> Emulation moeglich
      Serial.println("[SUCCESS] Kartendaten erfolgreich gelesen!");
      // Vollstaendiger Dump aller ueber NFC lesbaren Daten dieser Applikation.
      _dump.dumpAll(fci, fciLen, gpo, gpoLen, afl, aflLen, aflFound);
      break;
    } else {
      Serial.println("[HINWEIS] Keine lesbaren Record-Daten unter dieser AID.");
    }
  }

  if (!out.readSuccess) {
    Serial.println("[FEHLER] Auslesen der Kartendaten auf allen verfuegbaren AIDs fehlgeschlagen.");
  }
  return out.readSuccess;
}

bool EmvReader::selectPpse(std::vector<EmvAid>& aidList, EmvCapture& cap) {
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
  cap.putBlob(cap.ppse, cap.ppseLen, response, responseLength - 2);
  Tlv::extractAllAids(response, responseLength - 2, aidList);
  return !aidList.empty();
}

bool EmvReader::selectAid(const uint8_t* aid, uint8_t aidLen,
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

uint8_t EmvReader::buildPdolData(const uint8_t* pdol, uint8_t pdolLen,
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

bool EmvReader::tryGetProcessingOptions(const uint8_t* fci, uint8_t fciLen,
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

bool EmvReader::readCardRecords(const uint8_t* afl, uint8_t aflLen, CardData& card, EmvCapture& cap) {
  uint8_t response[MaxBufferLength];
  bool foundData = false;

  // Wenn AFL vorhanden ist, genau die Records anfragen
  if (afl != nullptr && aflLen >= 4) {
    for (int i = 0; i < aflLen; i += 4) {
      uint8_t sfi = (afl[i] >> 3) & 0x1F;
      uint8_t firstRec = afl[i + 1];
      uint8_t lastRec = afl[i + 2];

      for (int rec = firstRec; rec <= lastRec; ++rec) {
        uint8_t readRecordCmd[] = {
          0x00, 0xB2, static_cast<uint8_t>(rec), static_cast<uint8_t>((sfi << 3) | 0x04), 0x00
        };
        uint8_t responseLength = sizeof(response);
        bool success = _reader.inDataExchange(readRecordCmd, sizeof(readRecordCmd), response, &responseLength);
        if (success && responseLength >= 2 && response[responseLength - 2] == 0x90 && response[responseLength - 1] == 0x00) {
          cap.putRecord(sfi, rec, response, responseLength - 2);  // fuer Emulation
          if (parseRecordData(card, response, responseLength - 2)) {
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
        cap.putRecord(sfi, rec, response, responseLength - 2);  // fuer Emulation
        if (parseRecordData(card, response, responseLength - 2)) {
          return true;
        }
      }
    }
  }

  return foundData;
}

bool EmvReader::parseRecordData(CardData& out, const uint8_t* data, uint8_t dataLen) {
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

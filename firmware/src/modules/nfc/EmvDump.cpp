#include "EmvDump.h"

void EmvDump::decodeHint(uint32_t tag, const uint8_t* val, int len, int depth) {
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
    Tlv::parseTrack2(val, static_cast<uint8_t>(len), pan, exp);
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

void EmvDump::dumpTlv(const uint8_t* data, int dataLen, int depth) {
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
    EmvTags::printTag(tag);
    Serial.print(" [");
    Serial.print(EmvTags::name(tag));
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

bool EmvDump::getDataTag(uint32_t tag, uint8_t* out, uint8_t& outLen) {
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

void EmvDump::readAndDumpRecords(const uint8_t* afl, uint8_t aflLen) {
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

void EmvDump::dumpGetDataTags() {
  const uint32_t tags[] = { 0x9F36, 0x9F17, 0x9F13, 0x9F4F };
  for (uint32_t t : tags) {
    uint8_t buf[MaxBufferLength];
    uint8_t len = sizeof(buf);
    if (getDataTag(t, buf, len) && len > 0) {
      dumpTlv(buf, len, 0);
    } else {
      Serial.print("  (");
      EmvTags::printTag(t);
      Serial.print(" ");
      Serial.print(EmvTags::name(t));
      Serial.println("): nicht verfuegbar");
    }
  }
}

void EmvDump::dumpAll(const uint8_t* fci, uint8_t fciLen,
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

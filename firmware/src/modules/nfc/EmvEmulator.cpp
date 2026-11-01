#include "EmvEmulator.h"

uint8_t EmvEmulator::buildSynthPpse(const EmvCapture& cap, uint8_t* out, uint8_t outCap) {
  // Minimale PPSE-FCI, die genau unsere eine AID anpreist:
  //   6F L1  84 0E <"2PAY.SYS.DDF01">  A5 L2  BF0C L3  61 L4  4F <len> <AID>  87 01 01
  if (cap.aidLen == 0 || cap.aidLen > 16) return 0;
  static const uint8_t ppseName[14] = {
      '2','P','A','Y','.','S','Y','S','.','D','D','C','0','1'};

  uint8_t rec61[32];  // 61-Template-Inhalt: 4F <len> <AID> 87 01 01
  uint8_t n = 0;
  rec61[n++] = 0x4F;
  rec61[n++] = cap.aidLen;
  memcpy(&rec61[n], cap.aid, cap.aidLen); n += cap.aidLen;
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

int EmvEmulator::buildResponse(const EmvCapture& cap, const uint8_t* c, int clen, uint8_t* out, int outCap) {
  // Haengt an die (ggf. gekappten) Nutzdaten die beiden Statusbytes an.
  auto emit = [&](const uint8_t* data, int len, uint8_t sw1, uint8_t sw2) -> int {
    if (len < 0) len = 0;
    if (len > outCap - 2) {
      len = outCap - 2;  // Platz fuer SW1 SW2 sichern
      Serial.println("[emu] Antwort gekappt (Record > 251 B)");
    }
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
    static const char* kPpse = "2PAY.SYS.DDF01";
    if (lc == 14 && clen >= 5 + 14 && memcmp(df, kPpse, 14) == 0) {
      if (cap.ppseLen > 0) return emit(cap.ppse, cap.ppseLen, 0x90, 0x00);
      uint8_t synth[96];
      uint8_t sl = buildSynthPpse(cap, synth, sizeof(synth));
      if (sl > 0) return emit(synth, sl, 0x90, 0x00);
      return emit(nullptr, 0, 0x6A, 0x82);  // Datei nicht gefunden
    }
    // SELECT AID -> wir emulieren genau eine Applikation: FCI zurueckgeben.
    if (cap.fciLen > 0) return emit(cap.fci, cap.fciLen, 0x90, 0x00);
    return emit(nullptr, 0, 0x6A, 0x82);
  }

  // GET PROCESSING OPTIONS (80 A8 ..)
  if (c[0] == 0x80 && ins == 0xA8) {
    if (cap.gpoLen > 0) return emit(cap.gpo, cap.gpoLen, 0x90, 0x00);
    return emit(nullptr, 0, 0x6A, 0x81);  // Funktion nicht unterstuetzt
  }

  // READ RECORD (00 B2 rec (sfi<<3|4) ..)
  if (ins == 0xB2) {
    const uint8_t rec = p1;
    const uint8_t sfi = (p2 >> 3) & 0x1F;
    for (uint8_t i = 0; i < cap.recordCount; ++i) {
      if (cap.records[i].sfi == sfi && cap.records[i].rec == rec) {
        return emit(cap.records[i].data, cap.records[i].len, 0x90, 0x00);
      }
    }
    return emit(nullptr, 0, 0x6A, 0x83);  // Record nicht gefunden
  }

  // GET DATA (80 CA ..) - nicht mitgeschnitten
  if (c[0] == 0x80 && ins == 0xCA) return emit(nullptr, 0, 0x6A, 0x88);

  return emit(nullptr, 0, 0x6D, 0x00);  // INS nicht unterstuetzt
}

bool EmvEmulator::runOnce(const EmvCapture& cap, const uint8_t uid[4], uint16_t initTimeoutMs) {
  // PN532 als ISO14443-4-PICC initialisieren (gleiche bewaehrten Init-Bytes wie
  // der Type-4-Emulationspfad: MODE 0x05 = PICC only/passive, SAK 0x20).
  uint8_t cmd[] = {
      PN532_COMMAND_TGINITASTARGET,
      0x05,                     // MODE: PICC only, passive only
      0x04, 0x00,               // SENS_RES (ATQA)
      uid[1], uid[2], uid[3],   // NFCID1 (3 Byte)
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

  const unsigned long tInit = millis();
  int8_t st = _reader.tgInitAsTarget(cmd, sizeof(cmd), initTimeoutMs);
  const unsigned long initWaited = millis() - tInit;

  // DIAGNOSE (temporaer): zeigt, was tgInitAsTarget liefert.
  Serial.print("[emu][diag] tgInitAsTarget st=");
  Serial.print(st);
  Serial.print(" nach ");
  Serial.print(initWaited);
  Serial.println(" ms");

  // Wichtig: Der I2C-Transport meldet einen Timeout als st=-2 (getResponseLength
  // liefert -1, nicht PN532_TIMEOUT). Ein Timeout ist KEIN Fehler - der PN532
  // wartet nur auf einen Leser. Nur ein SCHNELLES st<0 (deutlich vor Ablauf des
  // Timeouts) ist ein echter Fehler (Kommando abgelehnt / Bus-Problem).
  if (st <= 0) {
    if (st < 0 && initWaited < (unsigned long)initTimeoutMs / 2) {
      if (!_initErrLogged) {
        Serial.println("[emu] tgInitAsTarget-Fehler (Kommando abgelehnt / I2C).");
        _initErrLogged = true;
      }
      return true;
    }
    return false;  // Timeout: kein Leser im Zeitfenster -> naechster Zyklus
  }
  _initErrLogged = false;

  Serial.println("[emu] Leser aktiv - beantworte APDUs.");
  _emuWaitLogged = false;  // naechste Runde darf wieder "warte" melden

  uint8_t capdu[255];
  uint8_t resp[255];
  // Schleifenlimit gegen Haengenbleiben bei fehlerhaftem Leser.
  const unsigned long emuStart = millis();
  for (int guard = 0; guard < 64; ++guard) {
    if (millis() - emuStart > 8000UL) break;  // Sitzung zeitlich begrenzen
    int16_t n = _reader.tgGetData(capdu, sizeof(capdu));
    if (n < 0) break;  // Leser weg / Fehler -> Sitzung beenden
    if (n == 0) continue;

    Serial.print("[emu] <- APDU (");
    Serial.print(n);
    Serial.print(" B): ");
    Tlv::printHex(capdu, (n > 16) ? 16 : static_cast<uint8_t>(n));
    Serial.println();

    int outLen = buildResponse(cap, capdu, n, resp, 253);  // LEN in tgSetData ist uint8_t: Daten+SW <= 253
    if (outLen <= 0) break;
    if (!_reader.tgSetData(resp, static_cast<uint8_t>(outLen))) {
      Serial.println("[emu] tgSetData fehlgeschlagen - Sitzung beendet.");
      break;
    }
  }

  _reader.inRelease();
  return true;
}

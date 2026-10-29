#pragma once

#include <Arduino.h>

struct EmvAid {
  uint8_t aid[16];
  uint8_t len;
  char label[24];
  uint8_t priority;
};

// Zuletzt ausgelesene Kartendaten (Anzeige).
struct CardData {
  String uid, aid, cardType, cardNumber, expiry, cardholder;
  String countryCode, currencyCode, effectiveDate, psn, serviceCode, aip;
  bool   readSuccess = false;

  void clear();
};

// Rohe Mitschnitte fuer die Karten-Emulation (Capture & Replay).
//   Beim echten Lesen werden die rohen Kartenantworten mitgeschnitten und bei
//   "emulate" unveraendert gegenueber einem Leser (Handy-App) zurueckgespielt.
//   WICHTIG: nur statische Anzeigedaten. KEINE Zahlung moeglich - das
//   dynamische EMV-Kryptogramm steckt im Secure Element und ist nicht lesbar.
struct EmvCapture {
  static constexpr uint8_t MaxRecords = 8;
  static constexpr uint8_t BlobCap    = 255;  // tgSetData-Laenge ist uint8_t

  struct Record {
    uint8_t sfi;
    uint8_t rec;
    uint8_t len;
    uint8_t data[BlobCap];
  };

  bool valid = false;          // Liegt ein brauchbarer Mitschnitt vor?
  uint8_t ppse[BlobCap]; uint8_t ppseLen = 0;
  uint8_t aid[16];       uint8_t aidLen  = 0;
  uint8_t fci[BlobCap];  uint8_t fciLen  = 0;
  uint8_t gpo[BlobCap];  uint8_t gpoLen  = 0;
  Record  records[MaxRecords]; uint8_t recordCount = 0;

  void reset();
  void putBlob(uint8_t* dst, uint8_t& dstLen, const uint8_t* src, int srcLen);
  void putRecord(uint8_t sfi, uint8_t rec, const uint8_t* data, int len);
};

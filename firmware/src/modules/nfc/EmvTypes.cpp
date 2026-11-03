#include "EmvTypes.h"

static_assert(EmvCapture::BlobCap == 255, "Replay-Blob muss 255 bleiben");

void CardData::clear() {
  uid = "";
  aid = "";
  cardType = "";
  cardNumber = "";
  expiry = "";
  cardholder = "";
  countryCode = "";
  currencyCode = "";
  effectiveDate = "";
  psn = "";
  serviceCode = "";
  aip = "";
  readSuccess = false;
}

void EmvCapture::reset() {
  valid = false;
  ppseLen = 0;
  aidLen = 0;
  fciLen = 0;
  gpoLen = 0;
  recordCount = 0;
}

void EmvCapture::putBlob(uint8_t* dst, uint8_t& dstLen, const uint8_t* src, int srcLen) {
  if (srcLen < 0) srcLen = 0;
  if (srcLen > BlobCap) srcLen = BlobCap;  // Kartendaten nie ungeprueft
  memcpy(dst, src, srcLen);
  dstLen = static_cast<uint8_t>(srcLen);
}

void EmvCapture::putRecord(uint8_t sfi, uint8_t rec, const uint8_t* data, int len) {
  if (len <= 0) return;
  if (len > BlobCap) len = BlobCap;
  // Schon vorhanden? (z. B. AFL und Fallback-Schleife treffen denselben Record)
  for (uint8_t i = 0; i < recordCount; ++i) {
    if (records[i].sfi == sfi && records[i].rec == rec) return;
  }
  if (recordCount >= MaxRecords) return;  // Puffer voll -> ignorieren
  Record& r = records[recordCount++];
  r.sfi = sfi;
  r.rec = rec;
  r.len = static_cast<uint8_t>(len);
  memcpy(r.data, data, len);
}

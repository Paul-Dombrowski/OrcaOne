#include "EmvTags.h"

namespace EmvTags {

// ============================================================================
//  Vollstaendiger TLV-Dump aller ueber NFC lesbaren Kartendaten
// ----------------------------------------------------------------------------
//  Quelle der Tag-Namen: EMV-Tag-Liste (vgl. eftlab Knowledge Base). Es sind
//  die gaengigen Tags eingetragen, die kontaktlose Karten liefern; unbekannte
//  Tags werden generisch mit "unbekannt" + Hex-Wert ausgegeben. Erweiterbar:
//  einfach weitere {Tag, "Name"}-Zeilen in kTags ergaenzen.
// ============================================================================
const char* name(uint32_t tag) {
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

void printTag(uint32_t tag) {
  const int nb = (tag > 0xFFFFFF) ? 4 : (tag > 0xFFFF) ? 3 : (tag > 0xFF) ? 2 : 1;
  for (int k = nb - 1; k >= 0; --k) {
    const uint8_t b = (tag >> (8 * k)) & 0xFF;
    if (b < 0x10) Serial.print('0');
    Serial.print(b, HEX);
  }
}

}  // namespace EmvTags

# NDEF - NDEF-Nachrichten lesen/schreiben (Don Coleman), setzt auf PN532 auf.
idf_component_register(
  SRCS "Ndef.cpp" "NdefRecord.cpp" "NdefMessage.cpp" "NfcTag.cpp"
       "MifareClassic.cpp" "MifareUltralight.cpp" "NfcAdapter.cpp"
  INCLUDE_DIRS "."
  REQUIRES arduino PN532
)

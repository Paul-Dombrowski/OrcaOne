#pragma once

#include <Arduino.h>
#include <PN532.h>
#include "EmvTypes.h"
#include "Tlv.h"

// Karten-Emulation (Capture & Replay): spielt einen mitgeschnittenen EmvCapture
// gegenueber einem Leser (Handy-App) zurueck.
// WICHTIG: nur statische Anzeigedaten. KEINE Zahlung moeglich - das dynamische
// EMV-Kryptogramm steckt im Secure Element und ist nicht lesbar.
class EmvEmulator {
public:
  explicit EmvEmulator(PN532& reader) : _reader(reader) {}

  // ein Zyklus: wartet bis initTimeoutMs auf einen Leser, beantwortet dann APDUs.
  // uid = 4 Byte fuer NFCID1. Rueckgabe true, wenn der PN532 den reinen Timeout
  // verlassen hat (Target-Modus betreten oder Fehler) und der Leser daher neu
  // initialisiert werden muss; false bei reinem Timeout ohne Leser.
  bool runOnce(const EmvCapture& cap, const uint8_t uid[4], uint16_t initTimeoutMs);

  // "warte auf Leser" darf wieder gemeldet werden (bei Start einer Emulation).
  void resetWaitLog() { _emuWaitLogged = false; }

private:
  PN532& _reader;
  bool _emuWaitLogged = false;  // "warte auf Leser" nur einmal melden

  // Baut aus einer empfangenen C-APDU die Antwort (inkl. SW1 SW2) in out.
  // Rueckgabe: Antwortlaenge (<= outCap, immer >= 2) oder 0 bei Fehler.
  int buildResponse(const EmvCapture& cap, const uint8_t* capdu, int clen, uint8_t* out, int outCap);
  // Synthetisiert eine minimale PPSE-FCI aus der gespeicherten AID, falls beim
  // Lesen keine echte PPSE-Antwort anfiel (Fallback-AID-Pfad).
  uint8_t buildSynthPpse(const EmvCapture& cap, uint8_t* out, uint8_t outCap);
};

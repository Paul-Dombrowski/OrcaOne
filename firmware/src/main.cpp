// ============================================================================
//  main.cpp - Einstiegspunkt der Firmware
// ----------------------------------------------------------------------------
//  Diese Datei "verdrahtet" alle Teile miteinander. Sie enthaelt SELBST keine
//  Hardware-Logik - die steckt in den Modulen (firmware/src/modules/).
//
//  Ablauf:
//    setup()  -> Speicher starten, alle Module einmal initialisieren (begin()),
//                Router + Serial-Konsole verdrahten.
//    loop()   -> allen Modulen staendig update() geben + die Serial-Konsole
//                bedienen (Bedienung ohne Display/Drehgeber/Handy).
//
//  Befehle (Serial Monitor, Handy, spaeter Menue) laufen alle ueber EINEN Weg:
//    SerialConsole -> CommandRouter.dispatch() -> Module.handleCommand().
//
//  Fehlt ein Bauteil, laeuft trotzdem alles andere weiter (ARBEITSWEISE.md §6).
// ============================================================================
#include <Arduino.h>
#include <SPI.h>
#include "config.h"
#include "storage/StorageService.h"
#include "core/CommandRouter.h"
#include "console/SerialConsole.h"
#include "modules/nfc/NfcModule.h"
#include "modules/ir/IrModule.h"
#include "modules/jammer/Jammer.h"

// --- Globale Objekte (ein Exemplar pro Modul) ---
NfcModule nfc;
IrModule  ir;
JammerModule jammer;

// Alle Module in EINER Liste, damit setup()/loop() sie einheitlich durchlaufen.
Module* modules[] = { &nfc, &ir, &jammer };
static const size_t MODULE_COUNT = sizeof(modules) / sizeof(modules[0]);

CommandRouter router;  // ein Weg fuer alle Befehle

void setup() {
  Serial.begin(115200);
  Serial.println("\n[OrcaOne] Start...");

  // SPI-Bus ZENTRAL und als ERSTES mit den NRF24-Pins initialisieren.
  // Grund: SPIClass::begin() kehrt beim zweiten Aufruf vorzeitig zurueck
  // (if(_spi) return true;) - der ERSTE Aufrufer legt die Pins fest. Liefe
  // Storage.begin() (SD.begin -> SPI.begin ohne Pins) zuerst, wuerde die
  // NRF24-Pinwahl ignoriert und der Chip antwortet nicht. SD und NRF24 teilen
  // den Bus und trennen sich ueber eigene CS-Pins (PIN_SD_CS / PIN_NRF_CSN).
  SPI.begin(PIN_NRF_SCK, PIN_NRF_MISO, PIN_NRF_MOSI, PIN_NRF_CSN);

  // Zentraler Speicher-Dienst: waehlt SD (falls Karte) sonst RAM.
  // Muss VOR den Modulen laufen, damit sie direkt speichern koennen.
  Storage.begin();

  // Alle Module einmal initialisieren.
  for (Module* m : modules) m->begin();

  // Befehls-Verteiler kennt alle Module; die Konsole nutzt ihn.
  router.begin(modules, MODULE_COUNT);
  Console.begin(&router);

  Serial.println("[OrcaOne] Bereit. 'help' fuer Befehle.");
}

void loop() {
  // Allen Modulen Rechenzeit geben (nicht-blockierend, siehe core/README.md).
  for (Module* m : modules) m->update();

  // Eingaben aus dem Serial Monitor verarbeiten (nicht blockierend).
  Console.update();

  delay(1);  // Gibt dem Idle-Task Zeit und verhindert Watchdog-Ausloesungen.
}

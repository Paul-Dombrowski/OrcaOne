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
#include "config.h"
#include "storage/StorageService.h"
#include "core/CommandRouter.h"
#include "console/SerialConsole.h"
#include "network/NetworkService.h"
#include "modules/nfc/NfcModule.h"
#include "modules/ir/IrModule.h"

// --- Globale Objekte (ein Exemplar pro Modul) ---
NfcModule nfc;
IrModule  ir;

// Alle Module in EINER Liste, damit setup()/loop() sie einheitlich durchlaufen.
Module* modules[] = { &nfc, &ir };
static const size_t MODULE_COUNT = sizeof(modules) / sizeof(modules[0]);

CommandRouter router;  // ein Weg fuer alle Befehle

void setup() {
  Serial.begin(115200);
  Serial.println("\n[OrcaOne] Start...");

  // Zentraler Speicher-Dienst: waehlt SD (falls Karte) sonst RAM.
  // Muss VOR den Modulen laufen, damit sie direkt speichern koennen.
  Storage.begin();

  // Alle Module einmal initialisieren.
  for (Module* m : modules) m->begin();

  // Befehls-Verteiler kennt alle Module; die Konsole nutzt ihn.
  router.begin(modules, MODULE_COUNT);
  Console.begin(&router);

  // WLAN-Hotspot aufspannen (Stufe 1a: nur Verbinden moeglich, noch keine
  // Weboberflaeche). Gibt die AP-IP im Serial-Monitor aus.
  Net.begin();

  Serial.println("[OrcaOne] Bereit. 'help' fuer Befehle.");
}

void loop() {
  // Allen Modulen Rechenzeit geben (nicht-blockierend, siehe core/README.md).
  for (Module* m : modules) m->update();

  // Eingaben aus dem Serial Monitor verarbeiten (nicht blockierend).
  Console.update();

  // Netzwerk-Dienst (Stufe 1a: noch ohne Aufgabe, Platzhalter fuer Stufe 2).
  Net.update();

  delay(1);  // Gibt dem Idle-Task Zeit und verhindert Watchdog-Ausloesungen.
}

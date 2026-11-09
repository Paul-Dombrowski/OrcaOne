// ============================================================================
//  main.cpp - Einstiegspunkt der Firmware
// ----------------------------------------------------------------------------
//  Diese Datei "verdrahtet" alle Teile miteinander. Sie enthaelt SELBST keine
//  Hardware-Logik - die steckt in den Modulen (firmware/src/modules/).
//
//  Ablauf:
//    setup()  -> NRF24/SPI + Speicher starten, alle Module initialisieren,
//                Router + Serial-Konsole + Netzwerk verdrahten.
//    loop()   -> allen Modulen update() geben, Serial-Konsole + Netzwerk
//                bedienen und Eingaben (Drehgeber/Taste) an das Display geben.
//
//  Fehlt ein Bauteil, laeuft trotzdem alles andere weiter (ARBEITSWEISE.md §6).
// ============================================================================
#include <Arduino.h>
#include <SPI.h>

#include "config.h"
#include "storage/StorageService.h"
#include "core/CommandRouter.h"
#include "core/ModeManager.h"
#include "console/SerialConsole.h"
#include "network/NetworkService.h"
#include "modules/display/DisplayModule.h"
#include "modules/input/InputModule.h"
#include "modules/nfc/NfcModule.h"
#include "modules/ir/IrModule.h"
#include "modules/jammer/Jammer.h"

// --- Globale Objekte (ein Exemplar pro Modul) ---
DisplayModule display;
InputModule   input;
NfcModule     nfc;
IrModule      ir;
JammerModule  jammer;

// Alle Module in EINER Liste, damit setup()/loop() sie einheitlich durchlaufen.
Module* modules[] = {&display, &input, &nfc, &ir, &jammer};
static const size_t MODULE_COUNT = sizeof(modules) / sizeof(modules[0]);

CommandRouter router;  // ein Weg fuer alle Befehle
ModeManager  modeManager;  // Geraete-Modus "No-Mercy" (core/), schaltet Aktoren

void setup() {
  Serial.begin(115200);
  Serial.println("\n[OrcaOne] Start...");

  // SPI-Bus zentral mit den NRF24-Pins initialisieren, dann den NRF24 ZUERST
  // in Betrieb nehmen - VOR der SD. Begruendung: NRF24 und SD teilen den
  // SPI-Bus. Ein fehlender/fehlschlagender SD-Mount (SD.begin) stoert den Bus
  // so, dass ein DANACH initialisierter NRF24 nicht mehr antwortet (genau das
  // Symptom: "keine SD" + "Keine SPI-Antwort vom NRF24"). Der aktive Sender
  // muss zuerst stehen; die SD toleriert Fehlstart (RAM-Fallback) und kommt
  // danach. SD und NRF24 trennen sich ueber eigene CS-Pins (PIN_SD_CS/CSN).
  SPI.begin(PIN_NRF_SCK, PIN_NRF_MISO, PIN_NRF_MOSI, PIN_NRF_CSN);
  jammer.begin();

  // Zentraler Speicher-Dienst: waehlt SD (falls Karte) sonst RAM. Laeuft VOR
  // den uebrigen Modulen, damit diese in ihrem begin() direkt speichern koennen.
  Storage.begin();

  // Uebrige Module initialisieren (jammer ist oben bereits gestartet).
  for (Module* module : modules) {
    if (module == &jammer) continue;
    module->begin();
  }

  // Befehls-Verteiler kennt alle Module; die Konsole nutzt ihn.
  router.begin(modules, MODULE_COUNT);

  // Geraete-Modus (No-Mercy): nimmt "system"-Befehle an und schaltet darueber
  // Jammer + IR. Muss NACH router.begin() und VOR den Befehlsquellen stehen.
  modeManager.begin(&router);
  router.setModeManager(&modeManager);

  display.setCommandRouter(&router);
  Console.begin(&router);

  // WLAN-Hotspot aufspannen (Stufe 1a: nur Verbinden moeglich, noch keine
  // Weboberflaeche). Gibt die AP-IP im Serial-Monitor aus.
  Net.begin();

  Serial.println("[OrcaOne] Bereit. 'help' fuer Befehle.");
}

void loop() {
  // Allen Modulen Rechenzeit geben (nicht-blockierend, siehe core/README.md).
  for (Module* module : modules) module->update();

  // Eingaben aus dem Serial Monitor verarbeiten (nicht blockierend).
  Console.update();

  // Netzwerk-Dienst (Stufe 1a: noch ohne Aufgabe, Platzhalter fuer Stufe 2).
  Net.update();

  // Drehgeber/Taste abfragen und an das Menue/Display weitergeben.
  InputEvent event = input.poll();
  while (event != InputEvent::None) {
    display.handleInput(event);
    event = input.poll();
  }

  delay(1);  // Gibt dem Idle-Task Zeit und verhindert Watchdog-Ausloesungen.
}

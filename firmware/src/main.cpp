// ============================================================================
//  main.cpp - Einstiegspunkt der Firmware
// ----------------------------------------------------------------------------
//  Diese Datei "verdrahtet" alle Teile miteinander. Sie enthaelt SELBST keine
//  Hardware-Logik - die steckt in den Modulen (firmware/src/modules/).
//
//  Ablauf:
//    setup()  -> alle Module einmal initialisieren (begin())
//    loop()   -> allen Modulen staendig update() geben + Netzwerk bedienen
//
//  HINWEIS: Dies ist ein GERUEST. Die mit TODO markierten Zeilen werden
//  aktiviert, sobald das jeweilige Modul existiert. Bis dahin laeuft das
//  Programm, tut aber noch nichts Sichtbares.
// ============================================================================
#include <Arduino.h>
#include "config.h"

// --- Module einbinden (aktivieren, sobald sie gebaut sind) ---
// #include "modules/display/DisplayModule.h"
// #include "modules/input/InputModule.h"
// #include "modules/nfc/NfcModule.h"
// #include "modules/ir/IrModule.h"
// #include "modules/jammer/JammerModule.h"
// #include "core/ModeManager.h"
// #include "network/NetworkManager.h"

// --- Globale Objekte (ein Exemplar pro Modul) ---
// DisplayModule display;
// InputModule   input;
// NfcModule     nfc;
// IrModule      ir;
// JammerModule  jammer;
// ModeManager   mode;          // kennt alle Module, fuehrt Automatik aus
// NetworkManager net;          // WLAN-AP + Webserver + WebSocket

// Alle Module in einer Liste, damit loop() sie einheitlich durchlaufen kann.
// Module* modules[] = { &display, &input, &nfc, &ir, &jammer };

void setup() {
  Serial.begin(115200);
  Serial.println("\n[OrcaOne] Start...");

  // TODO: jedes Modul initialisieren
  // for (Module* m : modules) m->begin();

  // TODO: Netzwerk starten und ihm die Modul-Liste geben, damit es Befehle
  //       vom Handy an das richtige Modul weiterreichen kann.
  // net.begin(modules, sizeof(modules)/sizeof(modules[0]), &mode);

  Serial.println("[OrcaOne] Bereit.");
}

void loop() {
  // TODO: jedem Modul Rechenzeit geben (alle update()s sind nicht-blockierend)
  // for (Module* m : modules) m->update();

  // TODO: Automatik-Regeln auswerten (nur aktiv, wenn Modus == auto)
  // mode.update();

  // TODO: Netzwerk bedienen + regelmaessig Status ans Handy schicken
  // net.update();
}

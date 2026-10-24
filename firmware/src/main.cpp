// ============================================================================
//  main.cpp - Einstiegspunkt der Firmware
// ----------------------------------------------------------------------------
//  Diese Datei "verdrahtet" alle Teile miteinander. Sie enthaelt SELBST keine
//  Hardware-Logik - die steckt in den Modulen (firmware/src/modules/).
//
//  Ablauf:
//    setup()  -> alle Module einmal initialisieren (begin())
//    loop()   -> allen Modulen staendig update() geben + Netzwerk bedienen
//                + Serial-Konsole bedienen (Bedienung ohne Display/Drehgeber)
//
//  Fehlt ein Bauteil, laeuft trotzdem alles andere weiter (ARBEITSWEISE.md §6).
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
// #include "core/CommandRouter.h"
// #include "core/MenuController.h"
// #include "network/NetworkManager.h"
// #include "console/SerialConsole.h"   // stellt das globale Objekt "Console" bereit

// --- Globale Objekte (ein Exemplar pro Modul) ---
// DisplayModule display;
// InputModule   input;
// NfcModule     nfc;
// IrModule      ir;
// JammerModule  jammer;
// ModeManager   mode;          // kennt alle Module, fuehrt Automatik aus
// CommandRouter router;        // EIN Weg fuer alle Befehle (Menue, Handy, Serial)
// MenuController menu;         // Menue-Logik; Ausgabe auf Display UND Serial
// NetworkManager net;          // WLAN-AP + Webserver + WebSocket

// Alle Module in einer Liste, damit loop() sie einheitlich durchlaufen kann.
// Module* modules[] = { &display, &input, &nfc, &ir, &jammer };

// --- TEST: On-Board-LED blinken lassen ------------------------------------
// Zeigt, dass Ordnerstruktur, ESP-IDF-Build und Arduino-Komponente
// funktionieren. Nicht blockierend (millis statt delay), damit es spaeter
// neben den Modulen weiterlaufen kann. Entfernen, sobald echte Module laufen.
static unsigned long lastBlink = 0;
static bool ledOn = false;

static void blinkUpdate() {
  if (millis() - lastBlink < LED_BLINK_MS) return;
  lastBlink = millis();
  ledOn = !ledOn;
  digitalWrite(PIN_LED_BUILTIN, ledOn ? HIGH : LOW);
  Serial.println(ledOn ? "[test] LED an" : "[test] LED aus");
}
// --------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  Serial.println("\n[OrcaOne] Start...");

  pinMode(PIN_LED_BUILTIN, OUTPUT);   // TEST: On-Board-LED

  // TODO: jedes Modul initialisieren
  // for (Module* m : modules) m->begin();

  // TODO: Befehls-Verteiler kennt alle Module + ModeManager.
  // router.begin(modules, sizeof(modules)/sizeof(modules[0]), &mode);

  // TODO: Menue, Netzwerk und Serial-Konsole nutzen alle denselben Router.
  // menu.begin(&router, &display);
  // net.begin(&router);
  // Console.begin(&router, &menu);

  Serial.println("[OrcaOne] Bereit.");
}

void loop() {
  blinkUpdate();   // TEST: On-Board-LED

  // TODO: jedem Modul Rechenzeit geben (alle update()s sind nicht-blockierend)
  // for (Module* m : modules) m->update();

  // TODO: Automatik-Regeln auswerten (nur aktiv, wenn Modus == auto)
  // mode.update();

  // TODO: Drehgeber-Ereignisse ans Menue geben (die Konsole tut dasselbe
  //       mit up/down/ok/back - beide Wege landen in menu.handleInput()).
  // InputEvent e = input.poll();
  // if (e != InputEvent::None) menu.handleInput(e);

  // TODO: Netzwerk bedienen + regelmaessig Status ans Handy schicken
  // net.update();

  // TODO: Eingaben aus dem Serial Monitor verarbeiten (nicht blockierend)
  // Console.update();
}

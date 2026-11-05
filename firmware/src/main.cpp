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
<<<<<<< HEAD
//
//  HINWEIS: Dies ist ein GERUEST. NFC ist als erster Grundtest direkt
//  eingebunden; Display und Input sind die ersten weiteren aktiven Module.
=======
>>>>>>> b12e9f3a41e323e7adc7b4e113026ec3c637e755
// ============================================================================
#include <Arduino.h>
#include "config.h"
<<<<<<< HEAD
#include "core/InputEvent.h"
#include "modules/display/DisplayModule.h"
#include "modules/input/InputModule.h"
#include "modules/nfc/NfcModule.h"

// --- Weitere Module ---
// #include "modules/ir/IrModule.h"
// #include "modules/jammer/JammerModule.h"
// #include "core/ModeManager.h"
// #include "core/CommandRouter.h"
// #include "core/MenuController.h"
// #include "network/NetworkManager.h"
// #include "console/SerialConsole.h"   // stellt das globale Objekt "Console" bereit

// --- Globale Objekte (ein Exemplar pro Modul) ---
DisplayModule display;
InputModule input;
=======
#include "storage/StorageService.h"
#include "core/CommandRouter.h"
#include "console/SerialConsole.h"
#include "modules/nfc/NfcModule.h"
#include "modules/ir/IrModule.h"

// --- Globale Objekte (ein Exemplar pro Modul) ---
>>>>>>> b12e9f3a41e323e7adc7b4e113026ec3c637e755
NfcModule nfc;
IrModule  ir;

// Alle Module in EINER Liste, damit setup()/loop() sie einheitlich durchlaufen.
Module* modules[] = { &nfc, &ir };
static const size_t MODULE_COUNT = sizeof(modules) / sizeof(modules[0]);

<<<<<<< HEAD
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

// --- Vorlaeufige Serial-Befehle (bis die echte Konsole steht) --------------
// Im Serial Monitor einen Befehl + Enter tippen: Menue up/down/ok/back oder
// NFC read/emulate/stop/clear. Nicht blockierend.
static void pollSerialCommands() {
  static String line;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      line.trim();
      if (line.length() > 0) {
        JsonDocument params;  // leer: read/emulate/stop/clear brauchen keine
        JsonObject commandParams = params.to<JsonObject>();
        bool ok = display.handleCommand(line, commandParams) ||
                  nfc.handleCommand(line, commandParams);
        Serial.print(ok ? "[cmd] angenommen: " : "[cmd] unbekannt/abgelehnt: ");
        Serial.println(line);
      }
      line = "";
    } else if (line.length() < 32) {
      line += c;
    }
  }
}
=======
CommandRouter router;  // ein Weg fuer alle Befehle
>>>>>>> b12e9f3a41e323e7adc7b4e113026ec3c637e755

void setup() {
  Serial.begin(115200);
  Serial.println("\n[OrcaOne] Start...");

  // Zentraler Speicher-Dienst: waehlt SD (falls Karte) sonst RAM.
  // Muss VOR den Modulen laufen, damit sie direkt speichern koennen.
  Storage.begin();

<<<<<<< HEAD
  display.begin();
  input.begin();
  nfc.begin();  // Vorlaeufiger NFC-Grundtest, bis die gemeinsame Modul-Liste steht.
=======
  // Alle Module einmal initialisieren.
  for (Module* m : modules) m->begin();
>>>>>>> b12e9f3a41e323e7adc7b4e113026ec3c637e755

  // Befehls-Verteiler kennt alle Module; die Konsole nutzt ihn.
  router.begin(modules, MODULE_COUNT);
  Console.begin(&router);

  Serial.println("[OrcaOne] Bereit. 'help' fuer Befehle.");
}

void loop() {
  // Allen Modulen Rechenzeit geben (nicht-blockierend, siehe core/README.md).
  for (Module* m : modules) m->update();

<<<<<<< HEAD
  pollSerialCommands();  // Vorlaeufig: read/emulate/stop/clear ueber Serial Monitor.
  input.update();
  const InputEvent inputEvent = input.poll();
  if (inputEvent != InputEvent::None) display.handleInput(inputEvent);
  display.update();
  nfc.update();  // Vorlaeufiger NFC-Grundtest, bis die gemeinsame Modul-Liste steht.

  // TODO: uebrigen Modulen Rechenzeit geben und danach auch NFC ueber die Liste updaten.
  // for (Module* m : modules) m->update();

  // TODO: Automatik-Regeln auswerten (nur aktiv, wenn Modus == auto)
  // mode.update();

  // Das Input-Modul liefert Eingaben an die aktuelle Anzeige. Sobald der
  // MenuController bereit ist, wird die Weitergabe dorthin verlegt.

  // TODO: Netzwerk bedienen + regelmaessig Status ans Handy schicken
  // net.update();

  // TODO: Eingaben aus dem Serial Monitor verarbeiten (nicht blockierend)
  // Console.update();
=======
  // Eingaben aus dem Serial Monitor verarbeiten (nicht blockierend).
  Console.update();
>>>>>>> b12e9f3a41e323e7adc7b4e113026ec3c637e755

  delay(1);  // Gibt dem Idle-Task Zeit und verhindert Watchdog-Ausloesungen.
}

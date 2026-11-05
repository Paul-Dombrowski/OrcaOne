#pragma once
#include <Arduino.h>
#include "../core/CommandRouter.h"

// ============================================================================
//  SerialConsole - Bedienung ueber den Serial Monitor
// ----------------------------------------------------------------------------
//  Dritte Befehlsquelle (neben Handy und Menue). Liest NICHT-BLOCKIEREND Zeilen
//  vom Serial Monitor, zerlegt sie und gibt sie an den CommandRouter. Ruft NIE
//  direkt ein Modul auf (console/README.md).
//
//  Zeilenformat:  <modul> <aktion> [schluessel=wert ...]
//  Eine Zeile, die mit '{' beginnt, wird direkt als Web-API-JSON verarbeitet.
//  Eingebaut:     help | status
//
//  Globales Objekt "Console" (Muster wie "Storage"): in setup() Console.begin(),
//  in loop() Console.update().
// ============================================================================
class SerialConsole {
public:
  void begin(CommandRouter* router);
  void update();  // nicht-blockierend; eine fertige Zeile wird sofort bearbeitet

private:
  void handleLine(const String& line);
  void dispatchText(const String& line);
  void dispatchJson(const String& line);
  void printHelp();
  void printStatus();

  CommandRouter* _router = nullptr;
  String _buf;
  bool _overflow = false;  // aktuelle Zeile ueberschritt die Laenge -> verwerfen
};

extern SerialConsole Console;

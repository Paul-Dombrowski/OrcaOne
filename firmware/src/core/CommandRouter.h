#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "Module.h"

// ============================================================================
//  CommandRouter - EIN Weg fuer alle Befehle
// ----------------------------------------------------------------------------
//  Verteilt einen Befehl (module, action, params) an das Modul mit passendem
//  name() und baut den Gesamtstatus. Genutzt von der Serial-Konsole (console/)
//  und spaeter von network/ und dem Menue. Darf NIE haengen, auch wenn ein
//  Ziel-Modul inaktiv ist.
//
//  "system" ist fuer Geraete-Einstellungen (ModeManager) reserviert - der ist
//  noch nicht gebaut, daher wird "system" vorerst abgelehnt.
// ============================================================================
class CommandRouter {
public:
  // Bekommt die Modul-Liste aus main.cpp (Lebensdauer: ganzes Programm).
  void begin(Module** modules, size_t count);

  // Befehl ausfuehren. Rueckgabe: false bei unbekanntem/inaktivem Modul oder
  // unbekanntem Befehl (das Modul entscheidet ueber seinen eigenen Befehl).
  bool dispatch(const String& module, const String& action,
                JsonObjectConst params, String* errorMessage = nullptr);

  // Gesamtstatus { "mode": "manual", "modules": { <name>: statusJson(), ... } }.
  // Dasselbe JSON fuer den Status-Push ans Handy und "status" in der Konsole.
  void buildStatus(JsonObject root);

  // Fuer die "help"-Ausgabe der Konsole (Liste der Modulnamen).
  size_t count() const { return _count; }
  const char* nameAt(size_t i) const { return _modules[i]->name(); }

private:
  Module** _modules = nullptr;
  size_t _count = 0;
};

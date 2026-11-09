#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

class CommandRouter;

// ============================================================================
//  ModeManager - Geraete-Modus "No-Mercy" (core/)
// ----------------------------------------------------------------------------
//  Der EINZIGE Ort, an dem mehrere Module gemeinsam geschaltet werden duerfen.
//  Module selbst reden nie miteinander (ARBEITSWEISE.md §6 Regel 2); diese
//  modulübergreifende Logik gehoert in core/. Der ModeManager bekommt
//  "system"-Befehle vom CommandRouter (Menue, Web-UI, Serial - alle gleich).
//
//  No-Mercy ist ein einfacher Umschalter (keine Sensor-Automatik, bewusst so
//  entschieden):
//    EIN  -> jammer.start + ir.shutoff   (2,4-GHz-Jammer + IR-Ausschaltcodes)
//    AUS  -> jammer.stop  + ir.stop
//  Geschaltet wird ueber denselben CommandRouter, den auch alle anderen
//  Befehlsquellen nutzen.
// ============================================================================
class ModeManager {
public:
  // Bekommt den gemeinsamen Router (Lebensdauer: ganzes Programm).
  void begin(CommandRouter* router);

  // Behandelt "system"-Aktionen. Derzeit nur "setMode" mit params {"mode": ...}.
  // mode: "nomercy"/"no-mercy"/"auto" = EIN, "off"/"manual" = AUS.
  // Rueckgabe: true, wenn der Befehl verstanden und angenommen wurde.
  bool handleSystem(const String& action, JsonObjectConst params,
                    String* errorMessage);

  // Fuer den Status-Push ans Handy (CommandRouter::buildStatus).
  const char* modeName() const { return _noMercy ? "no-mercy" : "manual"; }
  bool isNoMercy() const { return _noMercy; }

private:
  // Schaltet die Aktoren. Idempotent: gleicher Zustand -> kein erneutes Feuern
  // (verhindert Spam von jammer.start/ir.shutoff).
  void setNoMercy(bool on);

  CommandRouter* _router = nullptr;
  bool _noMercy = false;
};

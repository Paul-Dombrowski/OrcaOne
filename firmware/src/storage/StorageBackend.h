#pragma once
#include <Arduino.h>

// ============================================================================
//  StorageBackend - Vertrag fuer austauschbare Speicher-Backends
// ----------------------------------------------------------------------------
//  Der StorageService (Orchestrator) kennt nur diese abstrakte Basis, nicht die
//  konkreten Backends. Dadurch laesst sich ein Backend (RAM, SD, spaeter z. B.
//  Flash/LittleFS) austauschen oder ergaenzen, ohne den Orchestrator oder den
//  oeffentlichen Vertrag (StorageService.h) anzufassen (Strategy-Pattern,
//  ARBEITSWEISE.md §6.4 "additiv bauen").
//
//  WICHTIG: Backends kennen KEINE Pins und KEINEN Schluessel-Guard - beides
//  liegt im Orchestrator. Ein Backend bekommt nur gueltige (ns, key) und fuehrt
//  die reine Speicher-Operation aus.
// ============================================================================
class StorageBackend {
public:
  virtual ~StorageBackend() {}

  // Datensatz ablegen. true bei Erfolg.
  virtual bool save(const String& ns, const String& key, const String& data) = 0;

  // Datensatz lesen. Leerer String, wenn nicht vorhanden.
  virtual String load(const String& ns, const String& key) = 0;

  // Schluessel eines Namensraums als JSON-Array-Text (z. B. ["123","456"]).
  // Leerer Namensraum -> "[]".
  virtual String list(const String& ns) = 0;

  // Datensatz loeschen. true, wenn er existierte und entfernt wurde.
  virtual bool remove(const String& ns, const String& key) = 0;
};

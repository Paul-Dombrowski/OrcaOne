#include "StorageService.h"
#include "RamBackend.h"

// ============================================================================
//  StorageService - Orchestrator des zentralen Speicher-Dienstes
// ----------------------------------------------------------------------------
//  Erfuellt den fixen Vertrag aus StorageService.h. begin() erkennt beim Start
//  einmalig die SD und waehlt ein Backend; die uebrigen Methoden dispatchen an
//  das aktive Backend. Der Schluessel-Guard (gueltiger Dateiname) liegt hier -
//  backenduebergreifend -, damit kein Backend ihn doppelt umsetzen muss.
//
//  Da StorageService.h keine Member deklariert (fixe Spec), liegt der aktive-
//  Backend-Zustand datei-statisch. Die Backends sind statische Objekte
//  (kein Heap - gut fuer den ESP32).
// ============================================================================

// Global erreichbares Objekt (in genau dieser .cpp definiert).
StorageService Storage;

namespace {
RamBackend      ramBackend;
StorageBackend* active = &ramBackend;  // Default: RAM (Task 2 waehlt ggf. SD)
bool            useSd  = false;

// Ein Schluessel wird als Dateiname verwendet -> leer oder mit '/' ist unzulaessig.
bool keyIsValid(const String& key) {
  return key.length() > 0 && key.indexOf('/') < 0;
}
}  // namespace

void StorageService::begin() {
  // Task-1-Stand: nur RAM. Die SD-Erkennung kommt in StorageService (Task 2).
  active = &ramBackend;
  useSd  = false;
  Serial.println("[storage] RAM-Backend aktiv (SD-Erkennung folgt)");
}

bool StorageService::available() {
  return useSd;
}

bool StorageService::save(const String& ns, const String& key, const String& data) {
  if (!keyIsValid(key)) {
    Serial.println("[storage] ungueltiger Schluessel (leer oder '/') - nicht gespeichert");
    return false;
  }
  return active->save(ns, key, data);
}

String StorageService::load(const String& ns, const String& key) {
  return active->load(ns, key);
}

String StorageService::list(const String& ns) {
  return active->list(ns);
}

bool StorageService::remove(const String& ns, const String& key) {
  return active->remove(ns, key);
}

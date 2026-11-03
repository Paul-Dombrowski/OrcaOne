#include "StorageService.h"
#include "RamBackend.h"
#include "SdBackend.h"
#include "../config.h"

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
SdBackend       sdBackend;
StorageBackend* active = &ramBackend;  // Default: RAM; begin() waehlt ggf. SD
bool            useSd  = false;

// Ein Schluessel wird als Dateiname verwendet -> leer oder mit '/' ist unzulaessig.
bool keyIsValid(const String& key) {
  return key.length() > 0 && key.indexOf('/') < 0;
}
}  // namespace

void StorageService::begin() {
  // Einmalige Erkennung beim Start (kein Hot-Plug). SD erkannt -> dauerhaft,
  // sonst fluechtiges RAM. Niemals blockieren, egal was fehlt (§6.5).
  if (sdBackend.begin(PIN_SD_CS)) {
    active = &sdBackend;
    useSd  = true;
    Serial.println("[storage] SD erkannt - dauerhafter Speicher aktiv");
  } else {
    active = &ramBackend;
    useSd  = false;
    Serial.println("[storage] keine SD - Speicher nur im RAM");
  }
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

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

// Namensraum und Schluessel werden als Ordner-/Dateiname auf der SD verwendet.
// Deshalb hier zentral absichern (gilt backenduebergreifend): nicht leer, keine
// Pfad-Trenner ('/' '\'), kein Pfad-Traversal ('.' / '..'), sinnvolle Laenge.
// Bewusst KEINE Ziffern-nur-Regel: der Dienst ist generisch, andere Module
// nutzen sprechende Schluessel (z. B. "registered"), siehe storage/README.md.
bool nameIsValid(const String& s) {
  if (s.length() == 0 || s.length() > 64) return false;
  if (s == "." || s == "..") return false;
  if (s.indexOf('/') >= 0 || s.indexOf('\\') >= 0) return false;
  return true;
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
  if (!nameIsValid(ns) || !nameIsValid(key)) {
    Serial.println("[storage] ungueltiger Namensraum/Schluessel - nicht gespeichert");
    return false;
  }
  return active->save(ns, key, data);
}

String StorageService::load(const String& ns, const String& key) {
  if (!nameIsValid(ns) || !nameIsValid(key)) return String();
  return active->load(ns, key);
}

String StorageService::list(const String& ns) {
  if (!nameIsValid(ns)) return "[]";
  return active->list(ns);
}

bool StorageService::remove(const String& ns, const String& key) {
  if (!nameIsValid(ns) || !nameIsValid(key)) return false;
  return active->remove(ns, key);
}

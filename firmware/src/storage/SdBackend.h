#pragma once
#include "StorageBackend.h"

// ============================================================================
//  SdBackend - dauerhaftes Speicher-Backend auf der Micro-SD (SPI)
// ----------------------------------------------------------------------------
//  Die EINZIGE Stelle im Projekt, die die SD-Karte kennt (ARBEITSWEISE.md §4).
//  Ablage: eine Datei je Datensatz unter /<ns>/<key>.json. Der Ordner ist der
//  Namensraum, der Dateiname der Schluessel.
//
//  SPI-Bus wird mit dem NRF24 geteilt; der CS-Pin kommt vom Orchestrator aus
//  config.h (PIN_SD_CS) - das Backend kennt keine Pins (§5).
// ============================================================================
class SdBackend : public StorageBackend {
public:
  // Karte initialisieren. true, wenn eine Karte erkannt wurde.
  bool begin(uint8_t csPin);

  bool save(const String& ns, const String& key, const String& data) override;
  String load(const String& ns, const String& key) override;
  String list(const String& ns) override;
  bool remove(const String& ns, const String& key) override;

private:
  String pathFor(const String& ns, const String& key);  // "/" + ns + "/" + key + ".json"
  String dirFor(const String& ns);                       // "/" + ns
};

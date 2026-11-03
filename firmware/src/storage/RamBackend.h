#pragma once
#include "StorageBackend.h"

// ============================================================================
//  RamBackend - fluechtiges Speicher-Backend (kein Dauerspeicher)
// ----------------------------------------------------------------------------
//  Faellt ein, wenn keine SD-Karte erkannt wurde. Haelt eine Tabelle fester
//  Groesse im RAM; der Inhalt geht beim Neustart verloren. Dadurch funktioniert
//  Storage.save()/load() fuer jedes Modul auch ohne SD - nichts bricht ab
//  (storage/README.md, ARBEITSWEISE.md §6.5).
// ============================================================================
class RamBackend : public StorageBackend {
public:
  bool save(const String& ns, const String& key, const String& data) override;
  String load(const String& ns, const String& key) override;
  String list(const String& ns) override;
  bool remove(const String& ns, const String& key) override;

private:
  // Feste Obergrenze: begrenzt den RAM-Verbrauch. Mehr Eintraege -> save() false.
  static constexpr size_t Cap = 16;

  struct Entry {
    bool   used = false;
    String ns;
    String key;
    String data;
  };

  Entry _entries[Cap];

  // Index eines vorhandenen (ns,key) oder -1.
  int findIndex(const String& ns, const String& key) const;
};

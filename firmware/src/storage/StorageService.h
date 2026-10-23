#pragma once
#include <Arduino.h>

// ============================================================================
//  StorageService - zentraler Speicher-Dienst (SPEZIFIKATION, kein Code-Rumpf)
// ----------------------------------------------------------------------------
//  Dies ist der verbindliche Schnittstellen-Kopf. Die Umsetzung (.cpp) kommt
//  SPAETER - erst RAM-Backend (laeuft ohne SD), dann SD-Backend als Add-on.
//
//  Benutzung in einem Modul (immer gleich):
//      Storage.save("nfc", "slot1", jsonText);
//      String data = Storage.load("nfc", "slot1");
//
//  Regeln (siehe docs/ARBEITSWEISE.md):
//   - Namensraum (ns) = der Modulname, z. B. "nfc", "ir".
//   - data = JSON-Text; das Modul serialisiert/deserialisiert selbst.
//   - Ohne SD funktioniert alles (RAM, fluechtig); available() == false.
//     Kein Modul darf abbrechen, nur weil keine Karte steckt.
// ============================================================================
class StorageService {
public:
  // Einmaliger Start in setup(): waehlt SD-Backend (falls Karte) sonst RAM.
  void begin();

  // Ist dauerhafter Speicher (SD) vorhanden? false = nur fluechtiges RAM.
  bool available();

  // Datensatz ablegen. Rueckgabe: true bei Erfolg (bei SD-Schreibfehler false).
  bool save(const String& ns, const String& key, const String& data);

  // Datensatz lesen. Leerer String, wenn nicht vorhanden.
  String load(const String& ns, const String& key);

  // Vorhandene Schluessel eines Namensraums auflisten (z. B. als JSON-Array-Text).
  String list(const String& ns);

  // Datensatz loeschen. Rueckgabe: true bei Erfolg.
  bool remove(const String& ns, const String& key);
};

// Global erreichbares Objekt (in genau einer .cpp definiert, hier nur bekannt
// gemacht). Jedes Modul benutzt dieses eine Storage.
extern StorageService Storage;

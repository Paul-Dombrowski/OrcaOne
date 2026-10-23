# modules/ — Die einzelnen Funktionen

Jede Funktion des Geräts ist ein eigenes Modul in einem eigenen Unterordner.
Alle Module erben von der Basisklasse `Module` (siehe
[`../core/Module.h`](../core/Module.h)) und setzen denselben Vertrag um.

| Ordner | Funktion | Rolle | Status |
|---|---|---|---|
| `display/` | OLED-Ausgabe 1,3" | Aktor | ⬜ |
| `input/` | Drehgeber (Menü-Navigation) | Sensor | ⬜ |
| `nfc/` | NFC/RFID lesen, schreiben, klonen | Sensor + Aktor | ⬜ |
| `ir/` | IR aufnehmen & senden | Sensor + Aktor | ⬜ |
| `jammer/` | 2,4-GHz-Störsender | Aktor | ⬜ |
| `sdcard/` | Micro-SD (Add-on, liefert SD-Backend für `storage/`) | — | ⬜ |

## Ein neues Modul anlegen (Bauanleitung)

Jedes Modul besteht aus zwei Dateien, z. B. `nfc/NfcModule.h` und
`nfc/NfcModule.cpp`. Grundgerüst:

```cpp
// NfcModule.h
#pragma once
#include "../../core/Module.h"

class NfcModule : public Module {
public:
  const char* name() const override { return "nfc"; }
  void begin() override;
  void update() override;
  void statusJson(JsonObject obj) override;
  bool handleCommand(const String& action, JsonObjectConst params) override;

private:
  // eigene Variablen des Moduls (Sensor-Objekt, Zustand, letzter Messwert ...)
};
```

```cpp
// NfcModule.cpp
#include "NfcModule.h"
#include "../../config.h"

void NfcModule::begin() {
  // Hardware initialisieren (Pins, Bus, Sensor-Objekt)
}

void NfcModule::update() {
  // NICHT blockierend! Kurz prüfen, ob es etwas Neues gibt, dann zurückkehren.
  // Lange Vorgänge als Zustandsautomat mit millis() bauen - kein delay().
}

void NfcModule::statusJson(JsonObject obj) {
  // aktuellen Zustand nach außen geben (Handy + Display)
  // z. B. obj["lastUid"] = _lastUid;
}

bool NfcModule::handleCommand(const String& action, JsonObjectConst params) {
  // Befehle ausführen, z. B. if (action == "read") { ...; return true; }
  return false; // unbekannter Befehl
}
```

Danach das Modul in [`../main.cpp`](../main.cpp) einbinden, ein Objekt anlegen
und in die `modules[]`-Liste eintragen. Fertig — Menü und Handy können es dann
automatisch ansprechen.

## Regeln (für alle Module)

1. **Niemals `delay()` in `update()`.** (Warum: siehe [`../core/README.md`](../core/README.md).)
2. **Pins nur aus `config.h`** verwenden, keine Zahlen fest eintippen.
3. **`name()`** muss eindeutig sein und wird als Schlüssel in der Web-API benutzt
   (siehe [`../../../docs/kommunikation/web-api.md`](../../../docs/kommunikation/web-api.md)).

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
  bool _ok = false;   // false = Bauteil hat beim Start nicht geantwortet
  // eigene Variablen des Moduls (Sensor-Objekt, Zustand, letzter Messwert ...)
};
```

```cpp
// NfcModule.cpp
#include "NfcModule.h"
#include "../../config.h"

void NfcModule::begin() {
  // Hardware initialisieren (Pins, Bus, Sensor-Objekt)
  // Bauteil kann fehlen -> melden und inaktiv bleiben. NIE while(1);
  // _ok = (nfc.getFirmwareVersion() != 0);
  // if (!_ok) Serial.println("[nfc] PN532 nicht gefunden - Modul inaktiv");
}

void NfcModule::update() {
  if (!_ok) return;   // Bauteil fehlt -> nichts tun, Rest laeuft weiter
  // NICHT blockierend! Kurz prüfen, ob es etwas Neues gibt, dann zurückkehren.
  // Lange Vorgänge als Zustandsautomat mit millis() bauen - kein delay().
}

void NfcModule::statusJson(JsonObject obj) {
  // aktuellen Zustand nach außen geben (Handy + Display + Serial "status")
  obj["ok"] = _ok;    // Pflichtfeld
  // z. B. obj["lastUid"] = _lastUid;
}

bool NfcModule::handleCommand(const String& action, JsonObjectConst params) {
  if (!_ok) return false;   // Bauteil fehlt -> Befehl ablehnen, nicht haengen
  // Befehle ausführen, z. B. if (action == "read") { ...; return true; }
  return false; // unbekannter Befehl
}
```

Danach das Modul in [`../main.cpp`](../main.cpp) einbinden, ein Objekt anlegen
und in die `modules[]`-Liste eintragen. Fertig — Menü, Handy **und Serial Monitor**
können es dann automatisch ansprechen (z. B. `nfc read` im Monitor).

> Bei Bauteilen, die sich nicht erkennen lassen (Drehgeber, IR, Display), ist
> `_ok` einfach immer `true`. Siehe Abschnitt „Ohne Hardware“ in der Modul-README.

## Regeln (für alle Module)

1. **Niemals `delay()` in `update()`.** (Warum: siehe [`../core/README.md`](../core/README.md).)
2. **Pins nur aus `config.h`** verwenden, keine Zahlen fest eintippen.
3. **Ohne Bauteil lauffähig:** Fehlt die Hardware, bleibt nur dieses Modul
   inaktiv (Muster oben, `docs/ARBEITSWEISE.md` §6 Regel 5).
4. **`name()`** muss eindeutig sein und wird als Schlüssel in der Web-API benutzt
   (siehe [`../../../docs/kommunikation/web-api.md`](../../../docs/kommunikation/web-api.md)).

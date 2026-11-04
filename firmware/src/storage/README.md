# storage/ — Zentraler Speicher-Dienst

Ein **gemeinsamer Dienst**, über den **alle** Module dauerhaft speichern. Er ist
die **einzige** Stelle im Code, die SD-Karte, Ordner und Dateiformat kennt. Kein
Modul spricht die SD direkt an (harte Regel, siehe
[`../../../docs/ARBEITSWEISE.md`](../../../docs/ARBEITSWEISE.md) §6).

## So speicherst du etwas (für jede KI gleich)

Das Modul ruft das **globale `Storage`-Objekt** auf — es wird in `setup()` einmal
gestartet und ist überall verfügbar:

```cpp
Storage.save("nfc", "slot1", jsonText);   // ablegen
String data = Storage.load("nfc", "slot1"); // lesen
```

- **Namensraum** = der Modulname (`name()`), z. B. `"nfc"`, `"ir"`.
- **Schlüssel** = frei, Konvention `"slot1"`..`"slotN"` für die RAM-Slots, sprechende
  Namen für Listen (z. B. `"registered"` für registrierte Automatik-Tags).
- **Inhalt** = **JSON-Text** (String). Das Modul serialisiert selbst (ArduinoJson).

## Schnittstelle

Verbindlich ist die Funktionalität; die genauen Signaturen stehen in
[`StorageService.h`](StorageService.h).

| Methode | Zweck |
|---|---|
| `available()` | Gibt es dauerhaften Speicher (SD erkannt)? |
| `save(ns, key, data)` | Datensatz ablegen |
| `load(ns, key)` | Datensatz lesen |
| `list(ns)` | vorhandene Schlüssel eines Namensraums auflisten |
| `remove(ns, key)` | löschen |

## Optional & ausfallsicher (wichtig)

- **Ohne SD-Karte** funktioniert alles weiter — dann liegt der Dienst im
  **RAM-Backend** (flüchtig, geht bei Neustart verloren). `available()` meldet
  `false`. **Kein Modul darf deshalb abbrechen.**
- **Mit SD-Karte** (vom `SdBackend` beim Start erkannt) wird dasselbe Speichern
  automatisch dauerhaft. Der SD-Zugang liegt direkt hier im Dienst
  (`SdBackend.cpp`), nicht in einem eigenen Modul.
- **Erkennung nur beim Start** (kein Hot-Plug).
- **Schreibfehler** (Karte voll/defekt) werden über den Rückgabewert gemeldet, das
  Gerät stürzt nicht ab.

## Umsetzung (Dateien)

- [x] `StorageService` (Orchestrator) + RAM-Backend — läuft ohne SD.
- [x] SD-Backend (`SdBackend`) — dauerhaft, sobald eine Karte erkannt wird.

Struktur (Strategy-Pattern): `StorageService.cpp` wählt beim Start ein Backend
hinter dem gemeinsamen Vertrag `StorageBackend.h`; `RamBackend` (flüchtig) oder
`SdBackend` (Dateien unter `/<ns>/<key>.json`).

> Reihenfolge der Backends ist Absicht: Erst das RAM-Backend, damit die ersten
> Module sofort ohne SD funktionieren. SD ist ein Add-on.

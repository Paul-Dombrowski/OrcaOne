# core/ — Das „Gehirn" des Geräts

Hier liegt die Logik, die **alle Module zusammenhält**. Dieser Ordner enthält
keine einzelne Hardware-Funktion, sondern die Regeln, nach denen die Module
bedient werden.

## Dateien

| Datei | Zweck | Status |
|---|---|---|
| `Module.h` | **Der Modul-Vertrag.** Basisklasse, von der jedes Modul erbt. Die wichtigste Datei im ganzen Firmware-Teil. | ✅ fertig |
| `ModeManager.*` | Verwaltet den Zustand **Automatik ↔ Manuell** und führt die automatischen Regeln aus. | ⬜ zu bauen |
| `MenuController.*` | Steuert das OLED-Menü anhand der Drehgeber-Eingaben; ruft `handleCommand()` der Module auf. | ⬜ zu bauen |

## Der Modul-Vertrag (`Module.h`)

Jedes Modul bietet vier Funktionen an:

- `begin()` — einmal beim Start: Hardware initialisieren.
- `update()` — ständig in `loop()`: Sensor abfragen / Aktion fortsetzen. **Nie blockierend.**
- `statusJson(obj)` — aktuellen Zustand in JSON schreiben (→ Handy + Display).
- `handleCommand(action, params)` — einen Befehl ausführen (vom Handy oder Menü).

★ **Warum nicht-blockierend?** Der ESP32 arbeitet die `loop()` als Endlosschleife
ab. Ein `delay(2000)` in einem Modul legt für 2 Sekunden *alles* lahm: das Menü
reagiert nicht, das WLAN verliert Pakete, andere Module stehen still. Deshalb
werden lange Vorgänge (z. B. „3 Sekunden auf IR-Signal warten") als kleiner
**Zustandsautomat** gebaut: `update()` prüft nur kurz „bin ich fertig?" und kehrt
sofort zurück. Zeit misst man mit `millis()` statt `delay()`.

## ModeManager — Automatik (Pflicht laut Aufgabe)

Der Projektauftrag verlangt (a) **eine automatische Reaktion auf einen
Sensorwert** und (b) **Automatik an/aus per Handy**. Beides lebt hier.

**Beispiel-Automatikregel** (Vorschlag — mit der Gruppe festlegen):
> Wenn das NFC-Modul einen *registrierten* Tag erkennt, löst der ModeManager
> automatisch eine Aktion aus (z. B. gespeichertes IR-Signal senden oder eine
> Statusmeldung auf dem Display + Handy anzeigen).

- Im **Automatik**-Modus wertet `ModeManager::update()` die Sensor-Module aus und
  ruft bei erfüllter Bedingung selbstständig `handleCommand()` eines Aktor-Moduls.
- Im **Manuell**-Modus passiert das nicht; alles läuft nur über Menü/Handy.
- Umschalten per Handy: Befehl `{ "module":"system", "action":"setMode",
  "params":{"mode":"auto"} }` (siehe [`../../../docs/kommunikation/web-api.md`](../../../docs/kommunikation/web-api.md)).

## To-do

- [ ] `ModeManager` mit `setMode(auto|manual)`, `isAuto()` und einer ersten Regel.
- [ ] `MenuController`: Menüstruktur aus den registrierten Modulen aufbauen,
      Drehgeber-Events (vom `input`-Modul) in Navigation + `handleCommand` umsetzen.

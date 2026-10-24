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
| `CommandRouter.*` | **Befehls-Verteiler.** Einzige Stelle, die einen Befehl `(module, action, params)` an das richtige Modul bzw. an den ModeManager gibt und den Gesamtstatus baut. Genutzt von Menü, `network/` und `console/`. | ⬜ zu bauen |
| `InputEvent.h` | Gemeinsamer Typ für Bedien-Ereignisse (`Up`, `Down`, `Click`, `LongPress`). Liegt hier, weil ihn `input`-Modul **und** Serial-Konsole erzeugen. | ⬜ zu bauen |

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

## CommandRouter — ein Weg für alle Befehle

Befehle kommen aus **drei** Quellen: Drehgeber-Menü, Handy (`network/`) und Serial
Monitor (`console/`). Damit die Verteilung nicht dreimal gebaut wird (und dreimal
unterschiedlich), gibt es genau **eine** Stelle dafür:

```cpp
// Befehl ausfuehren. "system" -> ModeManager, sonst Modul mit passendem name().
// Rueckgabe: false bei unbekanntem Modul, inaktivem Modul oder unbekanntem Befehl.
bool dispatch(const String& module, const String& action, JsonObjectConst params);

// Gesamtstatus { "mode": ..., "modules": { <name>: statusJson(), ... } } bauen.
// Dasselbe JSON fuer Status-Push ans Handy und "status" in der Konsole.
void buildStatus(JsonObject root);
```

- Bekommt beim Start die Modul-Liste und den ModeManager (aus `main.cpp`).
- Darf **nie hängen**, auch wenn das Ziel-Modul inaktiv ist (Bauteil fehlt).

## Menü-Eingabe und -Ausgabe (Teil-Hardware)

- **Eingabe:** Der `MenuController` nimmt `InputEvent`s über **eine** Methode
  entgegen, z. B. `menu.handleInput(InputEvent e)`. Aufgerufen wird sie vom
  `input`-Modul-Polling in `main.cpp` **und** von der Serial-Konsole (`up`, `down`,
  `ok`, `back`). Woher ein Ereignis kommt, spielt für den Menü-Code keine Rolle.
- **Ausgabe:** Bei jeder Menü-Änderung zeichnet der `MenuController` aufs Display
  **und** gibt das Menü per `Console.printMenu(...)` im Serial Monitor aus. Immer
  beides, ohne zu prüfen, ob ein Display angeschlossen ist (keine
  Hardware-Erkennung, siehe `docs/ARBEITSWEISE.md` §6 Regel 5).

Details zur Konsole: [`../console/README.md`](../console/README.md).

## To-do

- [ ] `CommandRouter` mit `dispatch()` und `buildStatus()`.
- [ ] `InputEvent.h` (enum aus `modules/input/README.md` hierher verschieben).
- [ ] `ModeManager` mit `setMode(auto|manual)`, `isAuto()` und einer ersten Regel.
- [ ] `MenuController`: Menüstruktur aus den registrierten Modulen aufbauen,
      `InputEvent`s (vom `input`-Modul und von der Konsole) über `handleInput()`
      in Navigation + `CommandRouter::dispatch()` umsetzen; Menü immer auch per
      `Console.printMenu()` ausgeben.

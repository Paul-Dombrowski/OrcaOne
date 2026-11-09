# Modul: input — Drehgeber (Menü-Navigation)

## Vor dem Start lesen
docs/ARBEITSWEISE.md · diese README · core/Module.h

## Was ändere ich wo / Was speichere ich wie
- Eigene Dateien:   firmware/src/modules/input/   (nur hier Code dieses Moduls)
- Einstellungen:    firmware/src/config.h          (Encoder-Pins)
- Speicher-Namensraum: — (speichert nichts)

---

**Hardware:** Drehgeber KY-040 (CLK = GPIO25, DT = GPIO26, SW = GPIO27).
**Bibliothek:** keine (Quadraturauswertung im Modul). **Rolle:** Sensor
(Benutzereingabe).
`name()` = `"input"`.

## Was es können soll

- **Drehen** erkennen (eine Stufe nach rechts / links) → Auswahl hoch/runter.
- **Kurzer Druck** (SW) → auswählen / in den Modus gehen.
- **Langer Druck** (SW gehalten) → zurück (auch aus einem Status-Screen ins Menü).
- Diese Ereignisse dem Menü bzw. der Anzeige bereitstellen.

## Schnittstelle

`InputModule` erzeugt Ereignisse. `main.cpp` reicht sie an die
`DisplayModule`-Ansicht weiter; wenn später der `MenuController` umgesetzt wird,
kann er dieselbe Schnittstelle verwenden.

```cpp
// Der Typ liegt in core/InputEvent.h (NICHT in diesem Modul), weil die
// Serial-Konsole dieselben Ereignisse erzeugt (up/down/ok/back).
enum class InputEvent { None, Up, Down, Click, LongPress };
//  Click     = kurzer Druck  -> auswaehlen / bestaetigen
//  LongPress = langer Druck   -> zurueck
InputEvent poll();   // gibt das nächste Ereignis zurück (oder None)
```

- `update()`: Encoder-Position und Taster abfragen, Änderungen in Ereignisse
  umwandeln. **Nicht blockierend.**
- Taster **entprellen** (Software-Entprellung mit `millis()`), sonst löst ein
  Druck mehrfach aus. **Kurz vs. lang** über die Druckdauer unterscheiden
  (Schwelle z. B. ~600 ms, ebenfalls mit `millis()` gemessen).

## Ohne Hardware (Teil-Hardware, ARBEITSWEISE.md §6 Regel 5)

- Ein Drehgeber lässt sich nicht erkennen. `ok` ist deshalb immer `true`.
- ⚠️ **Pflicht: interne Pull-ups aktivieren.** Ohne angeschlossenen KY-040 hängen
  die Pins sonst in der Luft und liefern Zufallsereignisse, das Menü springt dann
  von selbst, während jemand z. B. am NFC arbeitet.
  - Drehgeber und Taster: `pinMode(..., INPUT_PULLUP)`. Gedrückt = `LOW`.
- **Ersatz ohne Drehgeber:** Im Serial Monitor `up`, `down`, `ok`, `back`
  eingeben; `main.cpp` reicht sie an die Anzeige weiter (siehe
  [`../../console/README.md`](../../console/README.md)).

## To-do

- [x] `InputModule.h` / `.cpp` anlegen und Encoder/Taster nicht-blockierend lesen.
- [x] Taster entprellen und kurze/lange Drücke als `Click`/`LongPress` ausgeben.

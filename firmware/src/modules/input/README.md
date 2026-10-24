# Modul: input — Drehgeber (Menü-Navigation)

## Vor dem Start lesen
docs/ARBEITSWEISE.md · diese README · core/Module.h

## Was ändere ich wo / Was speichere ich wie
- Eigene Dateien:   firmware/src/modules/input/   (nur hier Code dieses Moduls)
- Einstellungen:    firmware/src/config.h          (Encoder-Pins)
- Speicher-Namensraum: — (speichert nichts)

---

**Hardware:** Drehgeber KY-040 (CLK = GPIO25, DT = GPIO26, SW = GPIO27).
**Bibliothek:** `ESP32Encoder`. **Rolle:** Sensor (Benutzereingabe).
`name()` = `"input"`.

## Was es können soll

- **Drehen** erkennen (eine Stufe nach rechts / links) → Auswahl hoch/runter.
- **Kurzer Druck** (SW) → auswählen / in den Modus gehen.
- **Langer Druck** (SW gehalten) → zurück (auch aus einem Status-Screen ins Menü).
- Diese Ereignisse dem `MenuController` (in `core/`) bereitstellen.

## Schnittstelle

`main.cpp` fragt das Input-Modul nach aufgetretenen Ereignissen und gibt sie an
den `MenuController` (`menu.handleInput(e)`).

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
  - Drehgeber: Pull-ups über `ESP32Encoder` einschalten
    (`ESP32Encoder::useInternalWeakPullResistors`, genaue Schreibweise siehe
    Beispiele der installierten Bibliotheksversion), **vor** dem Anlegen des Encoders.
  - Taster: `pinMode(PIN_ENCODER_SW, INPUT_PULLUP)`. Gedrückt = `LOW`.
- **Ersatz ohne Drehgeber:** Im Serial Monitor `up`, `down`, `ok`, `back`
  eingeben (siehe [`../../console/README.md`](../../console/README.md)).

## To-do

- [ ] `InputModule.h` / `.cpp` anlegen, `ESP32Encoder` auf CLK/DT einrichten
      (mit internen Pull-ups, siehe oben).
- [ ] Drehrichtung im Serial-Monitor ausgeben (Test).
- [ ] Taster mit Entprellung ergänzen; kurzen vs. langen Druck unterscheiden,
      `poll()` mit `Click`/`LongPress` umsetzen.

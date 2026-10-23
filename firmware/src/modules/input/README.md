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

Der `MenuController` fragt das Input-Modul nach aufgetretenen Ereignissen.
Vorschlag:

```cpp
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

## To-do

- [ ] `InputModule.h` / `.cpp` anlegen, `ESP32Encoder` auf CLK/DT einrichten.
- [ ] Drehrichtung im Serial-Monitor ausgeben (Test).
- [ ] Taster mit Entprellung ergänzen; kurzen vs. langen Druck unterscheiden,
      `poll()` mit `Click`/`LongPress` umsetzen.

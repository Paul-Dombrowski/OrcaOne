# Modul: input — Drehgeber (Menü-Navigation)

**Hardware:** Drehgeber KY-040 (CLK = GPIO25, DT = GPIO26, SW = GPIO27).
**Bibliothek:** `ESP32Encoder`. **Rolle:** Sensor (Benutzereingabe).
`name()` = `"input"`.

## Was es können soll

- **Drehen** erkennen (eine Stufe nach rechts / links) → Menü hoch/runter.
- **Drücken** des eingebauten Tasters (SW) erkennen → Auswahl/Bestätigen.
- Diese Ereignisse dem `MenuController` (in `core/`) bereitstellen.

## Schnittstelle

Der `MenuController` fragt das Input-Modul nach aufgetretenen Ereignissen.
Vorschlag:

```cpp
enum class InputEvent { None, Up, Down, Click };
InputEvent poll();   // gibt das nächste Ereignis zurück (oder None)
```

- `update()`: Encoder-Position und Taster abfragen, Änderungen in Ereignisse
  umwandeln. **Nicht blockierend.**
- Taster **entprellen** (Software-Entprellung mit `millis()`), sonst löst ein
  Druck mehrfach aus.

## To-do

- [ ] `InputModule.h` / `.cpp` anlegen, `ESP32Encoder` auf CLK/DT einrichten.
- [ ] Drehrichtung im Serial-Monitor ausgeben (Test).
- [ ] Taster mit Entprellung ergänzen, `poll()` umsetzen.

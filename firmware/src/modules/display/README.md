# Modul: display — OLED-Ausgabe 1,3"

## Vor dem Start lesen
docs/ARBEITSWEISE.md · diese README · core/Module.h

## Was ändere ich wo / Was speichere ich wie
- Eigene Dateien:   firmware/src/modules/display/   (nur hier Code dieses Moduls)
- Einstellungen:    firmware/src/config.h            (Display-Pins/Adresse)
- Speicher-Namensraum: — (speichert nichts)

---

**Hardware:** 1,3" OLED (SH1106-Controller) über I2C (SDA = GPIO21, SCL = GPIO22,
Adresse 0x3C). **Bibliothek:** `U8g2`.
**Rolle:** Aktor (Ausgabe). `name()` = `"display"`.

> Hinweis: 1,3"-OLEDs nutzen meist den **SH1106**-Controller, 0,96"-OLEDs den
> SSD1306. Dieses Projekt verwendet den SH1106-Setup-Aufruf der U8g2-C-API.

## Umsetzung

`DisplayModule` portiert das vorhandene Orca-Menü in die Projektstruktur. Es
zeichnet das invertiert markierte Menü sowie die Schwimm-, Sprung-, Lade- und
Info-Bildschirme. Eingabeereignisse erhält es vom `InputModule`.

- `begin()` initialisiert I²C und U8g2.
- `update()` zeichnet Animationen höchstens alle 30 ms; es enthält kein
  `delay()`.
- `handleInput()` verarbeitet `InputEvent`s aus dem Input-Modul.
- `handleCommand()` unterstützt `up`, `down`, `ok` und `back` für die
  Serial-Konsole.
- U8g2 verwendet einen Wire-Callback, damit OLED und PN532 denselben I²C-Bus
  nutzen und kein zweiter I²C-Treiber installiert wird.
- Nach der neuen Abhängigkeit und den `.cpp`-Dateien einmal `idf.py reconfigure`,
  danach `idf.py build` ausführen.
- Das Projekt verwendet SH1106 und die gemeinsamen Pins aus `config.h`; die
  abweichenden Werte aus dem ursprünglichen Sketch werden nicht fest verdrahtet.
- Der Drehgeber wird ausschließlich vom Modul in `../input/` ausgelesen.

## Ohne Hardware (Teil-Hardware, ARBEITSWEISE.md §6 Regel 5)

- U8g2 erkennt ein fehlendes Display nicht; die Zeichenbefehle laufen ins Leere.
  `ok` bleibt deshalb `true`, solange der I²C-Bus initialisiert werden konnte.
- Die Befehle `up`, `down`, `ok` und `back` können zusätzlich über den
  vorläufigen Serial-Eingang in `main.cpp` gesendet werden.
- Status enthält `ok`, `screen`, `selection` und `swimSpeed`.

## To-do

- [x] `DisplayModule.h` / `.cpp` anlegen und U8g2 initialisieren.
- [x] Menü, Animationen und Drehgeber-Steuerung aus dem Sketch portieren.

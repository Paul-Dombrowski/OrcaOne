# Modul: display — OLED-Ausgabe 1,3"

**Hardware:** 1,3" OLED (SH1106-Controller) über I2C (SDA = GPIO21, SCL = GPIO22,
Adresse 0x3C). **Bibliothek:** `U8g2`.
**Rolle:** Aktor (Ausgabe). `name()` = `"display"`.

> Hinweis: 1,3"-OLEDs nutzen meist den **SH1106**-Controller, 0,96"-OLEDs den
> SSD1306. Bei verschobenem/abgeschnittenem Bild in U8g2 den passenden
> Konstruktor wählen (`U8G2_SH1106_128X64_NONAME_F_HW_I2C`).

## Was es können soll

- Das aktuelle **Menü** zeichnen (wird vom `MenuController` in `core/` gefüttert).
- Statuswerte anzeigen (z. B. zuletzt gelesene NFC-UID, aktueller Modus).
- Einfache Rückmeldungen („IR gespeichert", „Automatik AN").

## Schnittstelle

Das Display ist ein Dienst für die anderen Teile. Sinnvolle Zusatzmethoden
(neben dem Modul-Vertrag), die `MenuController` nutzen kann:

```cpp
void showMenu(const char* title, const char* items[], int count, int selected);
void showMessage(const char* text);
```

- `update()` übernimmt das eigentliche Zeichnen (U8g2 arbeitet mit einem
  Puffer: Inhalt setzen, dann einmal `display.sendBuffer()`), nicht-blockierend.

## To-do

- [ ] `DisplayModule.h` / `.cpp` anlegen, U8g2 initialisieren.
- [ ] „Hallo OrcaOne" anzeigen → bestätigt, dass Display + Verkabelung laufen.
- [ ] `showMenu` / `showMessage` umsetzen.

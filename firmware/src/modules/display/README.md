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
> SSD1306. Bei verschobenem/abgeschnittenem Bild in U8g2 den passenden
> Konstruktor wählen (`U8G2_SH1106_128X64_NONAME_F_HW_I2C`).

## Was es können soll

- Das aktuelle **Menü** zeichnen (wird vom `MenuController` in `core/` gefüttert),
  mit der aktuellen Auswahl **hervorgehoben**. Das OLED ist **einfarbig**, daher
  ist „hervorgehoben" ein **invertierter Balken** (dunkle Schrift auf hellem Grund),
  keine Farbe.
- **Status-Screen:** Ist man in einem Modus, in dem es nichts auszuwählen gibt,
  wird statt eines Menüs nur die Live-Statusinfo angezeigt (z. B. „Warte auf
  Signal…", letzte NFC-UID, aktueller Modus).
- Einfache Rückmeldungen („IR gespeichert", „Automatik AN").

## Schnittstelle

Das Display ist ein Dienst für die anderen Teile. Sinnvolle Zusatzmethoden
(neben dem Modul-Vertrag), die `MenuController` nutzen kann:

```cpp
void showMenu(const char* title, const char* items[], int count, int selected);
void showStatus(const char* title, const char* lines[], int count); // Status-Screen
void showMessage(const char* text);                                  // kurze Rückmeldung
```

`showMenu` hebt die Zeile `selected` invertiert hervor; `showStatus` zeigt reine
Infozeilen ohne Auswahl.

- `update()` übernimmt das eigentliche Zeichnen (U8g2 arbeitet mit einem
  Puffer: Inhalt setzen, dann einmal `display.sendBuffer()`), nicht-blockierend.

## Ohne Hardware (Teil-Hardware, ARBEITSWEISE.md §6 Regel 5)

- U8g2 merkt nicht, ob ein Display angeschlossen ist. Ohne Display laufen die
  Zeichenbefehle einfach ins Leere, das ist harmlos und **soll so bleiben** (keine
  Erkennung einbauen). `ok` ist deshalb immer `true`.
- **Das Menü ist trotzdem bedienbar:** Der `MenuController` gibt jedes Menü
  zusätzlich im Serial Monitor aus (`Console.printMenu`), siehe
  [`../../console/README.md`](../../console/README.md). Das Display-Modul muss dafür
  **nichts** tun und ruft die Konsole auch nicht selbst auf.
- Status: `{ "ok": true }`.

## To-do

- [ ] `DisplayModule.h` / `.cpp` anlegen, U8g2 initialisieren.
- [ ] „Hallo OrcaOne" anzeigen → bestätigt, dass Display + Verkabelung laufen.
- [ ] `showMenu` / `showMessage` umsetzen.

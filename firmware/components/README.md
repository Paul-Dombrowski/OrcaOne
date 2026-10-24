# firmware/components/ — Fremd-Bibliotheken als ESP-IDF-Komponenten

Hier liegt **Code von Drittanbietern**, der direkt im Projekt mitgeliefert wird.
ESP-IDF findet jeden Unterordner hier automatisch als Komponente.

Dieser Ordner ersetzt den früheren PlatformIO-`lib/`-Ordner. Das Projekt wird
**mit ESP-IDF (idf.py) gebaut**, nicht mehr mit PlatformIO.

## Vor dem Start lesen
docs/ARBEITSWEISE.md · diese README · firmware/main/idf_component.yml

---

## Entscheidung: Wohin kommt eine neue Bibliothek?

Gehe die Fragen **der Reihe nach** durch. Die erste, die zutrifft, gilt.

| # | Frage | Dann | Ort |
|---|---|---|---|
| 1 | Gibt es sie in der **ESP-IDF Component Registry** (components.espressif.com)? | Abhängigkeit eintragen | `firmware/main/idf_component.yml` |
| 2 | Gibt es nur eine **Arduino-Bibliothek** (kein IDF-Paket)? | Quellcode hierher kopieren + eigene `CMakeLists.txt` schreiben | `firmware/components/<Name>/` |
| 3 | Ist es **JS/CSS für die Weboberfläche**? | Datei mitliefern, **kein CDN** | `firmware/web/vendor/` (siehe `firmware/web/README.md`) |

Viele der Projekt-Bibliotheken (PN532, IR, RF24, Encoder …) sind reine
**Arduino**-Bibliotheken ohne IDF-Paket → das ist Fall 2 und der Normalfall hier.

### Fall 1 — Registry-Abhängigkeit

In `firmware/main/idf_component.yml` unter `dependencies:` eintragen, z. B.:

```yaml
dependencies:
  espressif/arduino-esp32: "~3.3.0"
  someuser/somelib: "^1.2.0"
```

Oder per Befehl: `idf.py add-dependency "someuser/somelib^1.2.0"`.
Geladen wird beim nächsten Build nach `firmware/managed_components/` (nicht im
Repo). Version **immer angeben** (Reproduzierbarkeit).

---

## Fall 2 — Arduino-Bibliothek als Komponente im Repo

```
firmware/components/
└─ <Name>/                  → ein Ordner pro Bibliothek, Name ohne Leerzeichen
   ├─ CMakeLists.txt        → Pflicht (sagt ESP-IDF, wie gebaut wird)
   ├─ src/ bzw. *.h/*.cpp   → der Bibliothekscode
   ├─ LICENSE               → Lizenzdatei des Originals (Pflicht)
   └─ HERKUNFT.md           → Pflicht, siehe unten
```

Minimale `CMakeLists.txt` für eine Arduino-Bibliothek:

```cmake
idf_component_register(
  SRCS "src/DieLib.cpp"          # alle .cpp der Bibliothek auflisten
  INCLUDE_DIRS "src"             # wo die .h liegen
  REQUIRES arduino               # braucht Arduino.h, Wire, SPI ...
)
```

- `REQUIRES arduino` ist bei fast allen diesen Bibliotheken nötig (sie benutzen
  `Arduino.h`, `Wire`, `SPI`). Nutzt die Lib weitere Komponenten, diese ergänzen.
- Einbinden im Code wie gewohnt: `#include <DieLib.h>`.

### Pflichtinhalt `HERKUNFT.md`

```markdown
- Quelle:      <URL des Originals>
- Version:     <Tag/Commit/Datum des Downloads>
- Lizenz:      <z. B. MIT, BSD, GPL-3.0>
- Geändert:    ja/nein — falls ja: was und warum
- Genutzt von: <welches Modul, z. B. modules/nfc>
```

---

## Regeln

1. **Kein eigener Projektcode hier.** Eigene Module gehören nach
   `firmware/src/modules/<name>/` (siehe `docs/ARBEITSWEISE.md` §2).
   `components/` ist nur für **fremden** Code.
2. **Lizenz prüfen**, bevor Code kopiert wird. Ohne Lizenzdatei nicht übernehmen.
3. **Keine Bibliothek doppelt:** entweder Registry (Fall 1) **oder** als
   Komponente (Fall 2), nie beides.
4. **Pins** werden nie in einer Bibliothek fest eingetragen, sondern vom Modul aus
   `config.h` übergeben (`docs/ARBEITSWEISE.md` §5).
5. Nach dem Hinzufügen: `idf.py build` ausführen und prüfen, dass es kompiliert.

## Aktuell enthaltene Komponenten

| Ordner | Zweck | Genutzt von |
|---|---|---|
| `PN532/` | PN532-Kern-Treiber (Elechouse/Seeed) | `modules/nfc` |
| `PN532_I2C/` | I2C-Transport für den PN532 | `modules/nfc` |
| `NDEF/` | NDEF-Nachrichten lesen/schreiben (Don Coleman) | `modules/nfc` |

Beim Hinzufügen hier eine Zeile ergänzen.

> Noch offen: je ein `HERKUNFT.md` (Quelle/Version/Lizenz) pro Komponente — bitte
> die Download-Quelle eintragen, die bei dir verwendet wurde.

> ⚠️ **Bekannte Schwachstellen** in PN532/NDEF (Upstream, untrusted Tag-Daten):
> OOB-Read beim NDEF-Parsen, VLA-Stack-Overflow beim Mifare-Schreiben,
> Buffer-Overflow in `emulatetag` (ungenutzt). Details und Gegenmaßnahmen:
> [`../src/modules/nfc/README.md`](../src/modules/nfc/README.md) → „Sicherheit".

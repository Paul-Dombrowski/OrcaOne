# firmware/lib/ — Fremd-Bibliotheken, die im Repo liegen

Hier liegt **Code von Drittanbietern**, der direkt im Projekt mitgeliefert wird.
PlatformIO findet diesen Ordner automatisch (Standardordner, keine Einstellung
nötig).

## Vor dem Start lesen
docs/ARBEITSWEISE.md · diese README · firmware/platformio.ini (`lib_deps`)

---

## Entscheidung: Wohin kommt eine neue Bibliothek?

Gehe die Fragen **der Reihe nach** durch. Die erste, die zutrifft, gilt.

| # | Frage | Dann | Ort |
|---|---|---|---|
| 1 | Gibt es sie in der **PlatformIO-Registry** (registry.platformio.org)? | Eintrag in `lib_deps` | `firmware/platformio.ini` |
| 2 | Gibt es sie nur auf **GitHub** (unverändert nutzbar)? | Git-URL **mit Tag/Commit** in `lib_deps` | `firmware/platformio.ini` |
| 3 | Muss sie **verändert** werden oder gibt es sie nirgends als Paket? | Code hierher kopieren | `firmware/lib/<Name>/` |
| 4 | Ist es **JS/CSS für die Weboberfläche**? | Datei mitliefern, **kein CDN** | `firmware/web/vendor/` (siehe `firmware/web/README.md`) |

**Normalfall ist #1.** Dieser Ordner (`lib/`) ist nur für #3 gedacht.

### Beispiele für `lib_deps` (Fall 1 und 2)

```ini
lib_deps =
    ...
    adafruit/Adafruit NeoPixel@^1.12.0                 ; Fall 1: Registry
    https://github.com/user/repo.git#v1.2.3            ; Fall 2: GitHub, Version fest
```

- Jede Zeile bekommt einen **kurzen Kommentar** (`;`), wofür sie gebraucht wird,
  wie die bestehenden Einträge.
- GitHub-URLs **immer** mit `#<tag>` oder `#<commit>`. Ohne das wird bei jedem Build
  der aktuelle Stand geladen, und der Build kann sich unbemerkt ändern.
- Heruntergeladene Bibliotheken landen in `firmware/.pio/libdeps/`. Dieser Ordner
  steht in `.gitignore` und wird **nie** committet oder von Hand bearbeitet.

---

## Aufbau einer Bibliothek in diesem Ordner (Fall 3)

```
firmware/lib/
└─ <Name>/                  → ein Ordner pro Bibliothek, Name ohne Leerzeichen
   ├─ src/                  → .h und .cpp der Bibliothek
   │  ├─ <Name>.h
   │  └─ <Name>.cpp
   ├─ LICENSE               → Lizenzdatei des Originals (Pflicht)
   └─ HERKUNFT.md           → Pflicht, siehe unten
```

Einbinden im Code: `#include <Name.h>`. Kein relativer Pfad nötig, PlatformIO
kennt den Ordner.

### Pflichtinhalt `HERKUNFT.md`

```markdown
- Quelle:     <URL des Originals>
- Version:    <Tag/Commit/Datum des Downloads>
- Lizenz:     <z. B. MIT, BSD, GPL-3.0>
- Geändert:   ja/nein — falls ja: was und warum
- Genutzt von: <welches Modul, z. B. modules/nfc>
```

So weiß jede Person (und jede KI) später, woher der Code stammt und ob man ihn
durch eine neue Version ersetzen darf.

---

## Regeln

1. **Kein eigener Projektcode hier.** Eigene Module gehören nach
   `firmware/src/modules/<name>/`, eigene Dienste nach `firmware/src/<dienst>/`
   (siehe `docs/ARBEITSWEISE.md` §2). `lib/` ist nur für **fremden** Code.
2. **Lizenz prüfen**, bevor Code kopiert wird. Ohne Lizenzdatei nicht übernehmen
   und nachfragen.
3. **Keine Bibliothek doppelt:** Steht sie schon in `lib_deps`, nicht zusätzlich
   hierher kopieren (sonst gibt es Konflikte beim Kompilieren).
4. **Pins** werden nie in einer Bibliothek fest eingetragen, sondern vom Modul aus
   `config.h` übergeben (`docs/ARBEITSWEISE.md` §5).
5. **Vorher prüfen, ob sie gebraucht wird:** Für Kleinigkeiten keine neue
   Bibliothek. Jede Bibliothek kostet Flash-Speicher auf dem ESP32.
6. Nach dem Hinzufügen: **Build ausführen** (Haken in PlatformIO) und prüfen, dass
   alles kompiliert.

## Aktuell enthaltene Bibliotheken

| Ordner | Zweck | Genutzt von |
|---|---|---|
| *(noch keine)* | | |

Beim Hinzufügen hier eine Zeile ergänzen.

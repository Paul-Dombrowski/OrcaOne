# Arbeitsweise — verbindlicher Standard für alle (Menschen & KIs)

**Zweck:** Dieses Dokument legt fest, *wie* an OrcaOne gearbeitet wird — damit
**jedes KI-System und jede Person auf exakt dieselbe Weise** arbeitet, egal wer
oder welches Modell gerade dran ist. Wer an diesem Projekt etwas baut oder ändert,
liest **zuerst diese Datei**.

> Grundsatz: Lieber eine Regel zu viel befolgen als raten. Wenn etwas unklar ist,
> gilt die Platzierungsregel (§2) und im Zweifel wird **nachgefragt**, nicht
> geraten.

---

## §1 Vor dem Start IMMER lesen (Kontext-Regel)

Bevor du mit einer Aufgabe beginnst, lies in dieser Reihenfolge:

1. **Diese Datei** (`docs/ARBEITSWEISE.md`) — die Spielregeln.
2. Die **`README.md` des Ordners**, in dem du arbeitest.
3. Bei Firmware zusätzlich: **`firmware/src/core/Module.h`** (der Modul-Vertrag).
4. Bei Speicherung: **`firmware/src/storage/README.md`** (wie man speichert).
5. Bei Hardware/Pins: **`docs/hardware/pinbelegung.md`**.
6. Bei Handy ↔ ESP32: **`docs/kommunikation/web-api.md`**.
7. Bei neuen Bibliotheken (Firmware oder Web): **`firmware/lib/README.md`**.
8. Beim Testen ohne Display/Drehgeber/Handy: **`firmware/src/console/README.md`**.

Wenn du als KI diese Dateien nicht vorliegen hast, **fordere sie an** — arbeite
nie aus dem Gedächtnis oder aus Vermutungen über die Schnittstellen.

---

## §2 Platzierungsregeln — wohin kommt was?

Auch für Dinge, die noch niemand angelegt hat: lege sie **nach dieser Tabelle** am
richtigen Ort an.

| Was | Pfad | Beispiel |
|---|---|---|
| Neue Gerätefunktion (Firmware-Modul) | `firmware/src/modules/<name>/` | `modules/buzzer/` |
| Gemeinsamer Firmware-Dienst (von mehreren Modulen genutzt) | `firmware/src/<dienst>/` | `storage/` |
| Dokumentation / Planung | `docs/<thema>/` | `docs/hardware/` |
| Quellcode der Weboberfläche | `firmware/web/` | `firmware/web/js/app.js` |
| Fremd-Bibliothek aus Registry/GitHub (Normalfall) | Eintrag in `lib_deps` in `firmware/platformio.ini` | `adafruit/Adafruit NeoPixel@^1.12.0` |
| Fremd-Bibliothek als Code im Repo (verändert / kein Paket) | `firmware/lib/<Name>/` | `firmware/lib/MyLib/` |
| Fremd-JS/CSS für die Weboberfläche | `firmware/web/vendor/` | `firmware/web/vendor/chart.min.js` |

Bei Bibliotheken entscheidet die Reihenfolge in `firmware/lib/README.md`.

**Fallback-Regel (wichtig):** Fehlt für etwas eine Kategorie, entscheide nach Art:
- Es ist eine **Gerätefunktion** → unter `firmware/src/modules/<name>/`.
- Es ist **Doku/Planung** → unter `docs/<thema>/`.
- Passt es in keine Kategorie → **nicht raten, nachfragen.**

Lege dafür einen **neuen Ordner mit eigener `README.md`** an (§3-Prinzip gilt
sinngemäß). **Niemals** eine Datei „irgendwohin" oder ins Projekt-Stammverzeichnis
werfen.

---

## §3 Der Modul-Standard (gleich für jedes Modul)

Jede `modules/<name>/README.md` **beginnt** mit diesem Block — identisch aufgebaut,
damit jede KI ihn sofort findet:

```markdown
## Vor dem Start lesen
docs/ARBEITSWEISE.md · diese README · core/Module.h · (bei Speichern: storage/README.md)

## Was ändere ich wo / Was speichere ich wie
- Eigene Dateien:   firmware/src/modules/<name>/   (nur hier Code dieses Moduls)
- Einstellungen:    firmware/src/config.h          (Pins/Konstanten dieses Moduls)
- Speicher-Namensraum: "<name>"                    (über Storage, siehe storage/README.md)
```

Danach folgen: *Was es können soll* → *Befehle (`handleCommand`)* → *Status
(`statusJson`, immer mit `ok`)* → *Ohne Hardware* (was passiert, wenn das Bauteil
fehlt + wie man per Serial Monitor testet) → *Umsetzungshinweise* → *To-do*. Diese Reihenfolge ist für **alle**
Module gleich.

---

## §4 Speichern — immer über den zentralen Dienst

- Module speichern **nie** direkt auf SD oder ins Dateisystem, sondern nur über das
  globale `Storage`-Objekt. Details & Methoden: `firmware/src/storage/README.md`.
- Jedes Modul hat seinen **Namensraum** (= sein `name()`, z. B. `"nfc"`).
- Gespeichert wird als **JSON-Text**. Das Modul serialisiert selbst.
- Speichern ist **optional**: ohne SD-Karte läuft alles (flüchtig, im RAM). Prüfe
  mit `Storage.available()`, ob Dauerspeicher da ist — **brich nie ab**, wenn nicht.

---

## §5 Hardware & Pins

- **Alle** Pin-Nummern und board-spezifischen Werte stehen **ausschließlich** in
  `firmware/src/config.h`. Nirgendwo sonst Pins fest eintippen.
- Aktuelles Board: **ESP32 D1 R32**. Die Pins dort sind **noch zu verifizieren**
  (siehe `docs/hardware/pinbelegung.md`). Board-Wechsel = nur `config.h` ändern.

---

## §6 Harte Regeln (nicht verhandelbar)

1. **Kein `delay()` in `update()`** — macht das ganze Gerät unbedienbar. Zeit mit
   `millis()` messen, lange Abläufe als Zustandsautomat.
2. **Modul ↔ Modul ist verboten.** Ein Modul redet nur mit `core/` und `storage/`.
   Verknüpfungen (z. B. „NFC → IR") laufen über den `ModeManager` im `core/`.
3. **`Module.h` bleibt bei genau 4 Methoden** (`begin/update/statusJson/handleCommand`).
   Neue Fähigkeiten kommen als neues Modul, nicht durch Aufbohren des Vertrags.
4. **Additiv bauen:** neue Funktion = neue Datei(en), den Kern nicht umbauen.
5. **Fehlende Hardware darf nie den Rest blockieren.** Niemand im Team hat alle
   Bauteile zu Hause. Ist ein Bauteil nicht angeschlossen, funktioniert nur dieses
   eine Modul nicht. Alles andere muss normal weiterlaufen. Konkret:
   - **Kein `while(1);`**, kein `ESP.restart()`, kein Warten ohne Zeitlimit, wenn
     ein Bauteil nicht antwortet. Achtung: Die Beispiel-Sketches vieler
     Bibliotheken machen genau das (`if (!found) while (1);`), also **nicht
     übernehmen**.
   - Meldet die Bibliothek beim Start einen Fehler: **einmal** im Serial Monitor
     ausgeben (z. B. `[nfc] PN532 nicht gefunden - Modul inaktiv`), dann
     `update()` nichts tun lassen und in `handleCommand()` `false` zurückgeben.
   - Eingänge ohne angeschlossenes Bauteil dürfen keine Zufallsereignisse
     erzeugen: **interne Pull-ups** aktivieren (`INPUT_PULLUP`, beim Drehgeber
     über die Pull-up-Einstellung von `ESP32Encoder`).
   - Es gibt **keine** Hardware-Erkennung und **keine** An/Aus-Schalter für
     Bauteile. Bau keine ein.
6. **Alles ist über den Serial Monitor bedienbar.** Jeder Befehl, den das Handy
   oder das Menü auslösen kann, muss auch über die Serial-Konsole gehen. Ein Modul
   erreicht das automatisch, wenn es seine Befehle in `handleCommand()` umsetzt
   (siehe `firmware/src/console/`).

---

## §7 git / Zusammenarbeit

- Nie direkt auf `main`. Eigener Branch pro Aufgabe (`feature/<thema>`), dann Pull
  Request. Vor dem Start `git pull`, nach fertigen Schritten `git commit`.
- Weil jedes Modul einen eigenen Ordner hat, arbeitet ihr an getrennten Dateien →
  kaum Merge-Konflikte.

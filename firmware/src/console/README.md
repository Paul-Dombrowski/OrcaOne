# console/ — Serial-Konsole (Bedienung über den Serial Monitor)

Mit der Serial-Konsole lässt sich das ganze Gerät **ohne Display, ohne Drehgeber und
ohne Handy** bedienen, nur über den Serial Monitor von PlatformIO.

**Warum es sie gibt:** Niemand im Team hat alle Bauteile zu Hause. Wer z. B. nur
das NFC-Modul hat, muss es trotzdem vollständig testen und bedienen können
(Regel: `docs/ARBEITSWEISE.md` §6 Regel 5 + 6, Design: Spec Abschnitt 5.7).

## Vor dem Start lesen
docs/ARBEITSWEISE.md · diese README · core/README.md (CommandRouter, MenuController) ·
docs/kommunikation/web-api.md (Befehlsliste)

## Was ändere ich wo
- Eigene Dateien: `firmware/src/console/` (`SerialConsole.h` / `.cpp`)
- Einstellungen:  keine eigenen (Baudrate 115200 steht in `platformio.ini` + `main.cpp`)
- Speicher:       — (speichert nichts)

---

## Einordnung

Die Konsole ist **kein Modul**, sondern ein gemeinsamer Dienst wie `network/`.
Sie ist die **dritte Befehlsquelle**. Alle drei landen am selben Punkt:

```
   Drehgeber-Menü        Handy (WebSocket)        Serial Monitor
        │                       │                       │
   MenuController          network/                console/
        └───────────────┬───────┴───────────────────────┘
                        ▼
             core/CommandRouter  →  handleCommand() des Moduls
                                    bzw. ModeManager ("system")
```

Folge für Modul-Entwickler: **Ein Modul muss für die Konsole nichts extra tun.**
Was es in `handleCommand()` umsetzt, ist automatisch auch per Serial bedienbar.

Die Konsole ist ein **global erreichbares Objekt** `Console` (gleiches Muster wie
`Storage`): einmal in `setup()` mit `Console.begin(...)` starten, in `loop()`
`Console.update()` aufrufen.

---

## 1. Modul-Befehle

Format, eine Zeile pro Befehl, mit Enter abschicken:

```
<modul> <aktion> [schluessel=wert ...]
```

Das ist **dieselbe** Struktur wie das JSON der Web-API, nur als Text:

| Eingabe im Monitor | entspricht Web-API-JSON |
|---|---|
| `nfc read` | `{ "module":"nfc", "action":"read" }` |
| `ir send slot=1` | `{ "module":"ir", "action":"send", "params":{"slot":1} }` |
| `system setMode mode=auto` | `{ "module":"system", "action":"setMode", "params":{"mode":"auto"} }` |
| `{"module":"ir","action":"send","params":{"slot":1}}` | Eine Zeile, die mit `{` beginnt, wird **direkt als JSON** verarbeitet (zum Testen der Web-API) |

Regeln für den Parser:
- Werte, die nur aus Ziffern bestehen, werden als **Zahl** übergeben (`slot=1` → `1`),
  alles andere als Text (`mode=auto` → `"auto"`).
- Groß-/Kleinschreibung der Modulnamen und Aktionen wie in der Web-API (klein).
- Die gültigen Aktionen pro Modul stehen in der Tabelle in
  [`docs/kommunikation/web-api.md`](../../../docs/kommunikation/web-api.md). Die
  Konsole hat **keine eigene** Befehlsliste.

### Eingebaute Befehle

| Eingabe | Wirkung |
|---|---|
| `help` | Syntax, Liste der Modulnamen, Hinweis auf `web-api.md` |
| `status` | Gesamtstatus ausgeben (dasselbe JSON, das ans Handy geht) |

### Antworten

Jede Ausgabe beginnt mit einem Präfix in eckigen Klammern:

```
> ir send slot=1
[ir] send: OK
> nfc read
[nfc] read: abgelehnt (Modul inaktiv oder Befehl unbekannt)
> foo bar
[console] unbekanntes Modul "foo" - "help" eingeben
```

---

## 2. Menü-Emulation (Ersatz für Drehgeber + Display)

| Eingabe | ersetzt am Gerät |
|---|---|
| `up` | Drehgeber eine Stufe hoch |
| `down` | Drehgeber eine Stufe runter |
| `ok` | kurzer Druck (auswählen) |
| `back` | langer Druck (zurück) |

- Die Konsole erzeugt dabei **dieselben `InputEvent`s** wie das `input`-Modul und
  gibt sie an den `MenuController` (`core/`). Der Menü-Code unterscheidet nicht,
  woher ein Ereignis kommt.
- **Ausgabe:** Der `MenuController` gibt das Menü bei **jeder Änderung immer auch**
  im Monitor aus, egal ob ein Display angeschlossen ist (es gibt bewusst keine
  Hardware-Erkennung). Rückmeldungen wie „IR Slot 1 gespeichert“ ebenso.
  Dafür bietet die Konsole:

```cpp
void printMenu(const char* title, const char* items[], int count, int selected);
void printMessage(const char* text);
```

Beispiel-Ausgabe:

```
== Infrarot ==
   Aufnehmen
 > Senden
```

---

## Umsetzungshinweise (auch für KI)

- **Nicht blockierend lesen:** In `update()` nur die Zeichen holen, die gerade da
  sind (`while (Serial.available())`), in einem Puffer sammeln, bei `'\n'` die
  Zeile verarbeiten. `'\r'` ignorieren. **Kein** `Serial.readString()` oder
  `readStringUntil()`, beide warten bis zu 1 s und frieren das Gerät ein.
- Puffer begrenzen (z. B. 128 Zeichen). Zu lange Zeilen verwerfen und melden.
- Befehle **nicht selbst** an Module verteilen, sondern an `CommandRouter`
  übergeben (siehe `core/README.md`). Für `status` ebenfalls den `CommandRouter`
  nutzen, damit die Ausgabe exakt dem Handy-Status entspricht.
- Die Konsole ruft nie direkt ein Modul auf (§6 Regel 2 sinngemäß).

## Serial Monitor richtig einstellen

In `platformio.ini` ist bereits gesetzt:
- `monitor_echo = yes`: Man sieht, was man tippt.
- `monitor_eol = LF`: Enter schickt `\n`.

Starten: PlatformIO → **Monitor** (Stecker-Symbol), Text eingeben, Enter.

## To-do

- [ ] `SerialConsole.h` / `.cpp` anlegen, globales Objekt `Console`.
- [ ] Zeilen nicht blockierend einlesen.
- [ ] Text-Befehle in `module` / `action` / `params` zerlegen, an `CommandRouter`.
- [ ] JSON-Zeilen (beginnen mit `{`) direkt an `CommandRouter`.
- [ ] `help`, `status`.
- [ ] `up` / `down` / `ok` / `back` als `InputEvent` an den `MenuController`.
- [ ] `printMenu` / `printMessage`.

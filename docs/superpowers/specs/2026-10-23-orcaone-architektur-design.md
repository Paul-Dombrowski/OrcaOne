# Design: OrcaOne — upgradebare Architektur, zentraler Speicher, Board-Portabilität

Stand: 2026-10-23 · Projekt TGI 13 „Nacht der Technik" · Team: 3 Personen + kleinere
KI-Modelle als Helfer

Dieses Dokument legt die Architektur fest, nach der OrcaOne weitergebaut wird. Es
beschreibt **was** entschieden ist und **warum** — nicht den fertigen Code. Ein
Entwickler (oder eine KI) soll danach bauen können, ohne nachzufragen.

---

## 1. Absicht (Intent) & Warum

OrcaOne ist kein Einzweck-Gerät, sondern eine **Plattform**: ein ESP32-Multitool
(NFC/RFID, Infrarot, 2,4 GHz), lokal per Drehgeber + OLED bedienbar und per
WLAN-Weboberfläche fernsteuerbar.

Der Auftraggeber (Projektgruppe) will drei Eigenschaften dauerhaft sichern:

1. **Upgradebar durch Hinzufügen, nie durch Umbauen.** Eine neue Funktion = neue
   Datei(en) dazu. Den Kern (Hauptschleife, Menü, WLAN/Web) fundamental umbauen zu
   müssen, wäre „katastrophal". → additive Erweiterung ist das oberste Prinzip.
2. **Dauerhafter Speicher über eine zentrale Stelle.** Aufgenommene IR-Signale und
   NFC-Tags (und weitere Listen) sollen Neustarts überleben. Die SD-Karte ist
   aber ein **Add-on**, kein Fundament.
3. **Austauschbare Hardware.** Aktuelles Board (ESP32 D1 R32) kann später gegen ein
   anderes (z. B. WROOM-32E/-U mit Antennenbuchse) getauscht werden, ohne den Code
   umzuschreiben.

Zusätzliche Querschnitts-Anforderung: **Kleinere KI-Modelle** müssen jederzeit
verstehen, *was sie wo ändern bzw. speichern müssen*. Die Vorbereitung soll
umfänglich (gründlich) sein, aber **ohne** lauffähiges Code-Skelett — reine
Spezifikations-/Doku-Ebene.

---

## 2. Ziele (Goals)

- **G1 – Additiv erweiterbar:** Ein neues Feature wird als `modules/<name>/`
  angelegt und mit *einer* Zeile in `main.cpp` registriert. Kein Eingriff in `core/`
  oder andere Module nötig. (Basis existiert: Modul-Vertrag `core/Module.h`.)
- **G2 – Zentraler Speicher-Dienst:** Genau *eine* Stelle im Code kennt Dateien,
  Ordner und SD-Zugriff. Module speichern über eine kleine, immer gleiche
  Schnittstelle, ohne das Speicher-Innenleben zu kennen.
- **G3 – SD optional (graceful degradation):** Module funktionieren **ohne**
  SD-Modul (flüchtig im RAM). Steckt eine Karte, wird dasselbe Speichern
  automatisch dauerhaft. Kein Modul bricht ab, weil keine Karte da ist.
- **G4 – Board-portabel:** Alles Board-/Pin-Spezifische steht ausschließlich in
  `config.h`. Board wechseln = nur dort Werte ändern.
- **G5 – KI-/Einsteiger-tauglich:** Jede Stelle, an der gearbeitet wird, sagt von
  selbst, *was wohin gehört*. Eine KI braucht nur die README ihres Ordners plus
  eine kurze Schnittstellenbeschreibung — nie das Gesamtsystem.

---

## 3. Nicht-Ziele (Non-Goals) & Anti-Ziele

**Non-Goals (bewusst jetzt nicht):**
- Kein lauffähiges Code-Skelett, keine Modul-Implementierung in diesem Schritt —
  nur Vorbereitung auf Doku-/Spezifikationsebene.
- Keine Board-Umstellung jetzt. Das D1 R32 bleibt; der Wechsel ist Zukunft.
- Keine Ausarbeitung der Jammer-Sendelogik (bleibt bei der Gruppe, siehe
  `modules/jammer/README.md`).
- Kein Zeitplan/Meilensteinplan (vom Auftraggeber nicht gewünscht).

**Anti-Goals (gilt als gescheitert, selbst wenn es „funktioniert"):**
- **A1:** Ein Modul greift direkt auf ein anderes Modul zu. → zerstört
  Austauschbarkeit und Additivität.
- **A2:** Ein Modul spricht direkt die SD-Karte / das Dateisystem an, statt über
  den Speicher-Dienst. → zerstört G2/G3.
- **A3:** Board-/Pin-spezifische Werte irgendwo außer in `config.h`. → zerstört G4.
- **A4:** Ein Modul hängt oder stürzt ab, weil keine SD-Karte steckt. → zerstört G3.
- **A5:** Eine Funktion lässt sich nur durch Änderungen im Kern hinzufügen. →
  zerstört G1.

---

## 4. Rahmenbedingungen (Constraints)

- **Hardware jetzt:** ESP32 **D1 R32** (Uno-Formfaktor, ESP32-WROOM-32).
  ⚠️ Dessen Pinbelegung weicht vom 30-Pin-DevKit ab, für das die aktuellen Pins in
  `config.h` eingetragen sind. Die Pins **müssen** gegen das D1 R32 geprüft und
  korrigiert werden (Hardware-Phase).
- **Bus-Teilung:** NRF24L01+ und das SD-Modul nutzen **beide SPI**. Sie teilen sich
  denselben SPI-Bus, brauchen aber **getrennte CS-Pins**. Das ist im Pin-Layout zu
  berücksichtigen.
- **Software:** PlatformIO + Arduino-Framework. Nicht-blockierend: kein `delay()` in
  `update()` (begründet in `core/README.md`).
- **Team:** 3 Personen, git-basiert, Arbeit über Feature-Branches + PRs; kleinere
  KI-Modelle als Helfer.
- **Termin:** Abgabe/Präsentation 11.11.2026.

---

## 5. Festgelegte Architektur (das entschiedene „Wie")

> **Ergebnis dieses Schritts ist reine Vorbereitung: Spezifikationen, READMEs und
> Schnittstellen-Köpfe — KEIN lauffähiger Code.** `StorageService.h`, `Module.h`
> usw. beschreiben *welche* Funktionen es gibt; die Rümpfe (`.cpp`) füllen die
> Teammitglieder / kleineren KIs später. Die Architektur unten ist der Bauplan,
> nach dem das dann geschieht.

### 5.1 Schichten und wer mit wem redet

```
   Handy (Web)        Drehgeber + OLED
        │                   │
   network/  ────────  core/ (ModeManager, MenuController)
                            │
                     ┌──────┴───────┐
                 modules/*       storage/   ← zentraler Speicher-Dienst
                 (nfc, ir, …)        │
                                 Backend: SD  ODER  RAM (Fallback)
```

**Erlaubte Abhängigkeiten (verbindlich):**
- Ein Modul darf nur mit `core/` (Menü/Modus) und mit `storage/` reden.
- **Modul ↔ Modul ist verboten** (Anti-Goal A1). Verknüpfungen zwischen Modulen
  (z. B. „NFC-Tag erkannt → IR senden" für die Automatik) laufen **über den
  `ModeManager` im `core/`**, nicht direkt zwischen den Modulen.

**`ModeManager` und `MenuController` existieren noch nicht** — ihre READMEs
beschreiben sie, der Code wird gebaut. Für die Automatik gilt die **Poll-Regel**
(Festlegung, hält `Module.h` bei genau 4 Methoden, kein Event-/Callback-Mechanismus
nötig): `ModeManager::update()` liest im Auto-Modus den Status der Sensor-Module
(über `statusJson()`), prüft die Bedingung und ruft bei Treffer `handleCommand()`
des Aktor-Moduls auf. So bleibt der Modul-Vertrag unverändert und Anti-Goal A5
gewahrt.

### 5.2 Zentraler Speicher-Dienst (`storage/`)

Neuer **Kern-Baustein** neben `core/` (nicht unter `modules/`, weil kein normales
Feature, sondern ein von mehreren Modulen genutzter Dienst).

Kleine, stabile Schnittstelle (konzeptionell — genaue Signaturen beim Bauen):
- `bool available()` — ist dauerhafter Speicher (SD) vorhanden?
- `save(namespace, key, data)` — Datensatz ablegen (z. B. `("ir", "slot1", …)`).
- `load(namespace, key) -> data` — Datensatz lesen.
- `list(namespace) -> keys` — vorhandene Einträge auflisten.
- `remove(namespace, key)` — löschen.

Jedes Modul bekommt einen eigenen **Namensraum** (`"ir"`, `"nfc"`, …). Das Modul
weiß **nicht**, ob dahinter eine SD-Karte, RAM oder etwas anderes steckt, und
kennt weder Ordner noch Dateiformat.

**Wie ein Modul den Dienst erreicht (Festlegung):** Der Speicher-Dienst ist ein
**global erreichbares Dienst-Objekt** (z. B. `Storage.save(...)`), das in `setup()`
einmal initialisiert wird. Ein Modul ruft es direkt auf — es wird **nicht** über
den Konstruktor oder `begin()` hereingereicht. Damit bleibt `Module.h` unverändert
bei genau vier Methoden (G1/A5), und eine KI, die ein Modul schreibt, lernt nur
einen Satz: „zum Speichern `Storage.save(namespace, key, data)` aufrufen".

**Datentyp (Festlegung):** `data` ist ein **JSON-Text (String)**; ArduinoJson ist
bereits eingebunden. Jedes Modul serialisiert seinen Datensatz selbst nach JSON
(IR: Protokoll/Code bzw. Raw-Timings; NFC: UID + Inhalt). Der Speicher-Dienst
behandelt den Inhalt als undurchsichtigen Text — dadurch funktioniert dieselbe
Schnittstelle für alle Modultypen.

**Schlüssel-Schema (Festlegung):** Die bestehenden RAM-Slots werden als Schlüssel
`"slot1"`..`"slotN"` abgebildet (N aus `IR_SLOT_COUNT`/`NFC_SLOT_COUNT`). Darüber
hinausgehende Listen (z. B. registrierte Automatik-Tags) sind ein eigener
Namensraum/Schlüssel. Obergrenze im RAM-Fallback: an den Slot-Zählern orientiert,
damit der Speicher nicht unbegrenzt wächst.

### 5.3 SD als Add-on + Fallback (G3)

- Der Speicher-Dienst hat austauschbare **Backends**:
  - **SD-Backend:** aktiv, wenn das SD-Modul vorhanden und eine Karte erkannt ist →
    persistent.
  - **RAM-Backend (Fallback):** sonst → flüchtig, Daten gehen bei Neustart
    verloren, aber alles funktioniert.
- Beim Start prüft `storage/`, ob eine SD verfügbar ist, und wählt das Backend.
  `available()` meldet das Ergebnis; die Module verhalten sich gleich, nur die
  Haltbarkeit unterscheidet sich.
- `modules/sdcard/` ist ein **dünnes Hardware-Modul**, das dem Speicher-Dienst nur
  den SD-Zugang bereitstellt. Fehlt es, läuft das Gerät mit RAM-Backend.

**Weitere Festlegungen zur SD:**
- **Erkennung nur beim Start** (wie in 5.3). Hot-Plug (Karte im Betrieb
  einstecken) ist bewusst *nicht* gefordert — es vermeidet Datenmigration RAM→SD
  zur Laufzeit und hält das Backend einfach. Optionale Zukunft.
- **SD-`CS`-Pin:** eigener Chip-Select, getrennt von NRF24, auf demselben SPI-Bus.
  Konkrete Pin-Nummer ist in `config.h` zunächst **`TBD`-Platzhalter** und wird in
  der Hardware-Phase am D1 R32 festgelegt (siehe Constraints).
- **Schreibfehler zur Laufzeit** (Karte defekt/voll): Der Dienst meldet den Fehler
  über den Rückgabewert und im Status zurück und **stürzt nicht ab** (A4). Ob
  danach automatisch auf RAM zurückgefallen wird, ist Entwickler-Entscheidung.
- **Weboberfläche liest gespeicherte Daten** nicht direkt aus `storage/`, sondern
  über das jeweilige Modul (dessen `statusJson()` fragt bei Bedarf den Dienst). So
  bleibt der Pfad `network → core/modules → storage` erhalten (A2).

### 5.4 Board-Portabilität (G4)

- Einzige board-spezifische Datei: `config.h` (Pins, Busse). Bereits so angelegt;
  Regel wird im Dokument und in den READMEs verankert.
- Offene Aufgabe: Pin-Werte für das **D1 R32** eintragen/prüfen (inkl. I2C-, SPI-
  und freie Pins für künftige Module).

### 5.5 Vorbereitung für kleinere KI-Modelle (G5)

- Jede Modul-README erhält oben einen kurzen, immer gleich aufgebauten Block
  **„Was ändere ich wo / Was speichere ich wie"**: welche Dateien dieses Modul
  besitzt, welche `config.h`-Werte es nutzt, welchen Speicher-Namensraum.
- Eigene README + „So speicherst du etwas"-Kurzanleitung für `storage/`.
- In `STRUCTURE.md` bleibt die Leitregel: einer KI immer nur die README ihres
  Ordners (+ ggf. `Module.h` und die Speicher-Schnittstelle) mitgeben.

### 5.6 Konkrete Struktur-Änderungen (additiv, keine Umbenennung)

Neu:
```
firmware/src/storage/        → README.md, StorageService.h (Schnittstelle, kein Code-Body)
firmware/src/modules/sdcard/ → README.md (Add-on-Hardware-Modul)
docs/superpowers/specs/      → dieses Design-Dokument
```
Aktualisiert (nur Text/Doku):
- `docs/STRUCTURE.md` — `storage/` + `sdcard/` + Abhängigkeitsregeln aufnehmen.
- `docs/hardware/pinbelegung.md` + `bauteile.md` — SD-Modul (SPI, eigener CS) +
  D1-R32-Hinweis.
- `docs/planning/io-uebersicht.md` — SD als optionaler persistenter Speicher,
  „weitere Listen" (registrierte Tags für die Automatik).
- Modul-READMEs `nfc/`, `ir/` — Speicher-Namensraum + „Was speichere ich wie"-Block;
  klarstellen, dass Persistenz optional ist.
- `config.h` — Platz/Hinweis für SD-Pins; Board-Regel als Kommentar.

---

## 6. Dem Entwickler überlassen (Left to builder)

- **Dateiformat auf der SD** (JSON, CSV oder binär). Empfehlung: einfaches,
  menschenlesbares Format (z. B. je Namensraum ein Ordner, je Eintrag eine Datei).
- **Zusätzlicher Fallback über LittleFS** (interner Flash) als Mittelweg zwischen
  SD und reinem RAM — optional; nur wenn es den Code nicht verkompliziert.
- **Welche „weiteren Listen"** über die registrierten Automatik-Tags hinaus
  tatsächlich gebraucht werden (z. B. Logs) — bei Bedarf ergänzen.
- **Genaue SD- und Gesamt-Pinbelegung** auf dem D1 R32 (Hardware-Phase).
- **Exakte Methodensignaturen** des Speicher-Dienstes (die Liste in 5.2 ist die
  verbindliche Funktionalität, nicht die finale Syntax).

---

## 7. Entscheidungen & offene Punkte

Ein Reviewer hat das Dokument gegengelesen. Die aufgedeckten Lücken habe ich wie
folgt behandelt.

### 7a. Von mir getroffene Festlegungen (bitte beim Lesen prüfen)

Diese habe ich selbst entschieden, weil ein Entwickler sie vernünftig entscheiden
kann — ihr könnt jede davon kippen:

1. **Speicher-Zugriff = globales Dienst-Objekt** (`Storage.save(...)`), nicht über
   Konstruktor/`begin()`. Grund: hält `Module.h` bei 4 Methoden und ist für kleine
   KIs am einfachsten. (wichtigste Festlegung)
2. **Datentyp beim Speichern = JSON-Text**; jedes Modul serialisiert selbst.
3. **Automatik per Polling** im `ModeManager`, kein Event-Mechanismus.
4. **SD-Erkennung nur beim Start**, kein Hot-Plug.
5. **SD-`CS`-Pin = TBD-Platzhalter** in `config.h`, finale Pins in der
   Hardware-Phase am D1 R32.
6. **`AUTO_TRIGGER_UID` bleibt** als fester Default in `config.h` (garantiert die
   Automatik-Demo auch ohne SD); eine echte „registrierte Tags"-Liste kommt
   zusätzlich als Speicher-Namensraum, sobald Persistenz verfügbar ist.
7. **Web liest Speicher-Daten über die Module**, nicht direkt aus `storage/`.

### 7b. Das solltet ihr als Gruppe entscheiden

- **Welche „weiteren Listen" braucht ihr wirklich?** Gesichert ist: gespeicherte
  IR-Signale, gespeicherte NFC-Tags, registrierte Automatik-Tags. Alles darüber
  hinaus (z. B. ein Ereignis-/Nutzungs-Log für die Demo) lege ich nur an, wenn ihr
  es wollt — sonst bleibt es weg, um nichts Überflüssiges zu bauen.

### 7c. Scope (klargestellt)

Dieser Schritt liefert **nur Vorbereitung** (Spezifikationen, READMEs,
Schnittstellen-Köpfe ohne Code-Rümpfe) — gemäß eurer Vorgabe „kein lauffähiges
Sketch". Die eigentliche Implementierung von `StorageService`, SD-/RAM-Backend und
den Modulen erfolgt später durch das Team/die KIs.

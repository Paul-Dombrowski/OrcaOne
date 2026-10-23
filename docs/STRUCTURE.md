# Projektstruktur OrcaOne

Diese Datei beschreibt den **gesamten Aufbau** des Projekts: welcher Ordner wofür
da ist, wie die Teile zusammenspielen und wo jede Person arbeitet. Wenn du neu
dazukommst (Mensch oder KI): **diese Datei zuerst lesen.**

---

## 1. Verzeichnisbaum

```
OrcaOne/
├─ README.md                 → Projektüberblick, Einstieg für alle
├─ docs/                     → gesamte Dokumentation & Planung (kein Code)
│  ├─ STRUCTURE.md           → DIESE Datei: Aufbau des Projekts
│  ├─ planning/              → Aufgabe 1: Idee, Ein-/Ausgänge, Blockschaltbild
│  ├─ hardware/              → Aufgabe 2: Bauteile, Pinbelegung, Verkabelung
│  └─ kommunikation/         → Aufgabe 3: WLAN, Web-API (REST/WebSocket)
│
├─ firmware/                 → der Code, der auf dem ESP32 läuft (C++/Arduino)
│  ├─ platformio.ini         → Build-Konfiguration + Bibliotheken
│  └─ src/
│     ├─ main.cpp            → Einstiegspunkt: setup() + loop(), verdrahtet alles
│     ├─ config.h            → zentrale Einstellungen (Pins, WLAN-Name, Timings)
│     ├─ core/               → „Gehirn": Modul-Vertrag, Menü, Auto/Manuell-Modus
│     ├─ modules/            → die einzelnen Funktionen, je ein Unterordner:
│     │  ├─ nfc/             → NFC/RFID lesen, schreiben, klonen (PN532)
│     │  ├─ ir/              → IR aufnehmen & senden (KY-022 / KY-005)
│     │  ├─ jammer/          → 2,4-GHz-Modul (NRF24L01+)
│     │  ├─ display/         → OLED-Ausgabe (1,3")
│     │  └─ input/           → Drehgeber (Menü-Navigation)
│     └─ network/            → WLAN-Access-Point + Webserver + WebSocket
│
├─ web/                      → Aufgabe 4: Weboberfläche fürs Handy
│  ├─ index.html             → Bedienoberfläche
│  ├─ css/                   → Styling
│  └─ js/                    → Logik + WebSocket-Verbindung zum ESP32
│
└─ task/                     → Original-Projektauftrag der Schule (nicht ändern)
```

---

## 2. Die drei Schichten (so hängt alles zusammen)

```
   [ Handy / Browser ]                 [ Gerät selbst ]
          │                             Drehgeber + OLED
          │  WLAN (WebSocket/HTTP)            │
          ▼                                   ▼
   ┌─────────────────────────────────────────────────────┐
   │  network/   (Webserver, verteilt Befehle & Status)   │
   └───────────────┬─────────────────────────────────────┘
                   │  einheitlicher Modul-Vertrag
   ┌───────────────▼─────────────────────────────────────┐
   │  core/   ModeManager (Auto/Manuell) + MenuController │
   └───────────────┬─────────────────────────────────────┘
                   │  begin() / update() / statusJson() / handleCommand()
   ┌───────────────▼─────────────────────────────────────┐
   │  modules/   nfc · ir · jammer · display · input      │
   └───────────────┬─────────────────────────────────────┘
                   │
              [ Hardware: PN532, IR, NRF24, OLED, Encoder ]
```

**Kernidee:** Jedes Modul bietet die *gleichen vier Funktionen* an (den
„Modul-Vertrag", siehe [`firmware/src/core/`](../firmware/src/core/)). Egal ob
der Befehl vom Drehgeber-Menü oder vom Handy kommt — er landet über die gleiche
Schnittstelle beim richtigen Modul. Das macht das System erweiterbar und
teamfreundlich.

---

## 3. Wer arbeitet wo? (Vorschlag zur Aufteilung)

Jede Person „besitzt" bestimmte Ordner. Dadurch bearbeitet ihr getrennte Dateien
und blockiert euch nicht gegenseitig.

| Bereich | Ordner | Aufgaben-Bezug |
|---|---|---|
| **Hardware + Firmware-Module** | `firmware/src/modules/` | Aufgabe 2 (20 P.) |
| **IoT-Kommunikation** | `firmware/src/network/`, `docs/kommunikation/` | Aufgabe 3 (25 P.) |
| **Weboberfläche** | `web/` | Aufgabe 4 (15 P.) |
| gemeinsam | `core/`, `docs/planning/`, `docs/` | Aufgabe 1 (10 P.) + Doku (5 P.) |

> Das ist nur ein Vorschlag — teilt es so auf, wie es für euch passt. Wichtig ist,
> dass die Verantwortlichkeiten klar sind.

---

## 4. Wie man mit einer KI an einem Teil arbeitet

Die anderen KIs sind schwächer und verstehen das große Ganze nicht von selbst.
Deshalb gilt:

1. **Immer die `README.md` des Ordners** mitgeben, an dem gearbeitet wird. Sie
   enthält Zweck, Schnittstelle und ein konkretes To-do.
2. Zusätzlich bei Firmware-Arbeit die Datei
   [`firmware/src/core/Module.h`](../firmware/src/core/Module.h) mitgeben — das ist
   der verbindliche Vertrag, an den sich jedes Modul halten muss.
3. Bei Pin-Fragen die [`docs/hardware/pinbelegung.md`](hardware/pinbelegung.md)
   mitgeben.
4. Bei Handy ↔ ESP32-Fragen die
   [`docs/kommunikation/web-api.md`](kommunikation/web-api.md) mitgeben.

So bekommt eine KI genug Kontext, ohne das ganze Projekt zu „kennen".

---

## 5. Konventionen

- **Sprache im Code:** Kommentare und Doku auf Deutsch; Bezeichner (Variablen,
  Funktionen) auf Englisch (üblich und KI-freundlich).
- **Ein Modul = ein Ordner** unter `firmware/src/modules/`.
- **Keine blockierenden Wartezeiten** (`delay()`) in `update()` — sonst hängt das
  ganze Gerät. Begründung steht in [`firmware/src/core/README.md`](../firmware/src/core/README.md).
- **Geheimnisse** (echte WLAN-Passwörter o. Ä.) gehören nicht ins Repo.

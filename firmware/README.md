# firmware/ — Der Code für den ESP32

Das ist das Programm, das auf dem Mikrocontroller läuft. Geschrieben in C++ mit
dem **Arduino-Framework**, gebaut und geflasht mit **ESP-IDF** (`idf.py`).

> Arduino **und** ESP-IDF? ESP-IDF ist das Build-/Flash-System; der Arduino-Core
> wird als Komponente geladen (`main/idf_component.yml`). Dadurch bleibt der Code
> Arduino-Code (`setup()`/`loop()`, `Arduino.h`), aber gebaut wird mit `idf.py`.

## Aufbau

```
firmware/
├─ CMakeLists.txt      → ESP-IDF-Projektdefinition (nicht löschen)
├─ sdkconfig.defaults  → ESP-IDF-Grundeinstellungen (u. a. Arduino-Autostart)
├─ main/               → ESP-IDF-Einstiegskomponente: bindet src/ ein, lädt Arduino
│  ├─ CMakeLists.txt   → registriert den Code aus ../src automatisch
│  └─ idf_component.yml→ Fremd-Abhängigkeiten (ersetzt das alte lib_deps)
├─ components/         → Fremd-Bibliotheken als Komponenten (siehe components/README.md)
├─ web/               → wird ins ESP32-Dateisystem geladen (Weboberfläche)
└─ src/
   ├─ main.cpp      → setup() + loop(), verbindet alle Teile
   ├─ config.h      → Pins, WLAN, Timings (zentrale Einstellungen)
   ├─ core/         → Modul-Vertrag, Auto/Manuell-Modus, Menü
   ├─ storage/      → zentraler Speicher-Dienst
   ├─ modules/      → die einzelnen Funktionen (nfc, ir, jammer, display, input, sdcard)
   ├─ network/      → WLAN-Hotspot + Webserver + WebSocket
   └─ console/      → Serial-Konsole (Bedienung über den Serial Monitor)
```

## Einrichten (einmalig pro Rechner)

1. **ESP-IDF v5.5.x** installieren (Espressif-Anleitung „ESP-IDF Get Started").
   Wichtig: genau die 5.5-Reihe — der stabile Arduino-Core 3.3.x ist daran
   gebunden. Eine andere IDF-Version lässt den Build bewusst abbrechen.
   (Mehrere IDF-Versionen dürfen parallel installiert sein.)
2. ESP-IDF-Umgebung aktivieren (unter Windows: „ESP-IDF PowerShell"/„ESP-IDF CMD",
   oder `export.ps1`/`export.bat` aus dem IDF-Ordner).
3. In diesen `firmware/`-Ordner wechseln.

## Bauen & Hochladen

Alle Befehle im `firmware/`-Ordner, in der aktivierten ESP-IDF-Umgebung:

```
idf.py set-target esp32      # einmalig: Ziel-Chip (ESP32 D1 R32 = klassischer ESP32)
idf.py build                 # kompilieren; beim 1. Mal wird der Arduino-Core geladen
idf.py -p <PORT> flash       # auf den ESP32 flashen (<PORT> z. B. COM5)
idf.py -p <PORT> monitor     # Serial Monitor; beenden mit Strg+]
```

Kurzform für Flashen + Monitor: `idf.py -p <PORT> flash monitor`.

## Bibliotheken hinzufügen

Registry-Bibliothek → Zeile in `main/idf_component.yml`. Arduino-Bibliothek ohne
IDF-Paket → nach `components/`. Genaue Reihenfolge und das Komponenten-Muster:
[`components/README.md`](components/README.md).

## Nicht alle Bauteile da?

Kein Problem, das ist der Normalfall. Was nicht angeschlossen ist, funktioniert
nicht, alles andere läuft normal weiter. Regel für den Code:
[`../docs/ARBEITSWEISE.md`](../docs/ARBEITSWEISE.md) §6 Regel 5. Bedienen und
testen lässt sich alles über den Serial Monitor (`idf.py monitor`), siehe
[`src/console/README.md`](src/console/README.md).

## Wie die Weboberfläche auf den ESP32 kommt

Die Dateien, die der Browser lädt, liegen in [`web/`](web/) und müssen in ein
eigenes Flash-Dateisystem (LittleFS) des ESP32.

> ⚠️ **Noch offen (Schritt 3):** Unter PlatformIO erledigte das „Upload Filesystem
> Image". Unter ESP-IDF braucht es eine LittleFS-Partition + einen Image-Schritt
> (Partitionstabelle + Komponente zum Erzeugen/Flashen des Images). Das wird beim
> Anbinden der Weboberfläche eingerichtet und hier ergänzt.

## Reihenfolge der Umsetzung (empfohlen)

1. `display` + `input` → man sieht etwas und kann navigieren (Grundgerüst).
2. `network` → Handy verbindet sich, Status wird angezeigt (Aufgabe 3).
3. `nfc`, `ir` → die eigentlichen Funktionen (Aufgabe 2).
4. `core/ModeManager` → Automatik (Pflicht).
5. `jammer` → zuletzt, separat.

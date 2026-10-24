# firmware/ — Der Code für den ESP32

Das ist das Programm, das auf dem Mikrocontroller läuft. Geschrieben in C++ mit
dem Arduino-Framework, gebaut mit **PlatformIO**.

## Aufbau

```
firmware/
├─ platformio.ini   → Board + Bibliotheken (lib_deps) (nicht löschen)
├─ lib/             → Fremd-Bibliotheken als Code im Repo (Ausnahmefall, siehe lib/README.md)
├─ web/             → wird ins ESP32-Dateisystem geladen (Weboberfläche)
└─ src/
   ├─ main.cpp      → setup() + loop(), verbindet alle Teile
   ├─ config.h      → Pins, WLAN, Timings (zentrale Einstellungen)
   ├─ core/         → Modul-Vertrag, Auto/Manuell-Modus, Menü
   ├─ storage/      → zentraler Speicher-Dienst
   ├─ modules/      → die einzelnen Funktionen (nfc, ir, jammer, display, input, sdcard)
   └─ network/      → WLAN-Hotspot + Webserver + WebSocket
```

## Bibliotheken hinzufügen

Normalfall: eine Zeile in `lib_deps` in `platformio.ini`. Nur wenn das nicht
geht, kommt der Code nach `lib/`. Die genaue Entscheidungsreihenfolge steht in
[`lib/README.md`](lib/README.md).

## Einrichten (einmalig pro Rechner)

1. **VS Code** installieren, darin die Erweiterung **PlatformIO IDE**.
2. Dieses `firmware/`-Verzeichnis in VS Code öffnen.
3. PlatformIO lädt die Bibliotheken aus `platformio.ini` automatisch.

## Bauen & Hochladen

- **Build** (Haken-Symbol unten) → kompiliert den Code.
- **Upload** (Pfeil-Symbol) → flasht die Firmware auf den angeschlossenen ESP32.
- **Upload Filesystem Image** → lädt die Weboberfläche (`web/`) auf den ESP32.
- **Monitor** (Stecker-Symbol) → zeigt die `Serial.println`-Ausgaben (115200 Baud).

## Wie die Weboberfläche auf den ESP32 kommt

Die Dateien, die der Browser lädt, müssen im Flash des ESP32 liegen. Sie liegen
direkt in [`web/`](web/); `platformio.ini` ist so eingestellt, dass **genau dieser
Ordner** beim „Upload Filesystem Image" aufs Gerät geladen wird — **kein
Kopierschritt** nötig. (Siehe [`web/README.md`](web/README.md).)

## Reihenfolge der Umsetzung (empfohlen)

1. `display` + `input` → man sieht etwas und kann navigieren (Grundgerüst).
2. `network` → Handy verbindet sich, Status wird angezeigt (Aufgabe 3).
3. `nfc`, `ir` → die eigentlichen Funktionen (Aufgabe 2).
4. `core/ModeManager` → Automatik (Pflicht).
5. `jammer` → zuletzt, separat.

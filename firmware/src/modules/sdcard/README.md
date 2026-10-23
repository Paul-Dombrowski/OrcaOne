# Modul: sdcard — Micro-SD (Add-on, dünnes Hardware-Modul)

## Vor dem Start lesen
docs/ARBEITSWEISE.md · diese README · core/Module.h · storage/README.md

## Was ändere ich wo / Was speichere ich wie
- Eigene Dateien:   firmware/src/modules/sdcard/   (nur hier Code dieses Moduls)
- Einstellungen:    firmware/src/config.h          (SD-SPI-Pins, v. a. CS)
- Speicher-Namensraum: — (speichert selbst nichts; liefert nur das SD-Backend)

---

**Rolle:** Add-on. Dieses Modul speichert **nicht** selbst, sondern stellt dem
zentralen Speicher-Dienst ([`../../storage/`](../../storage/)) den **SD-Zugang**
bereit. Fehlt dieses Modul oder steckt keine Karte, läuft OrcaOne mit dem
RAM-Backend weiter (siehe `storage/README.md`).

**Hardware:** Micro-SD-Modul über **SPI**, gemeinsamer Bus mit dem NRF24L01+, aber
**eigener Chip-Select (CS)**. Bibliothek: `SD` (bzw. `SdFat`).

## Was es können soll
- Beim Start prüfen, ob eine Karte steckt und lesbar ist.
- Dem Speicher-Dienst Datei-Lesen/-Schreiben/-Auflisten/-Löschen bereitstellen.
- Den Zustand melden (Karte vorhanden? freier Platz?).

## Wichtig
- **Kein Hot-Plug:** Erkennung nur beim Start.
- Teilt sich SPI mit dem NRF24 → der **CS-Pin** muss ein anderer sein als der des
  NRF24. Konkreter Pin: `config.h` (aktuell `TBD`, in der Hardware-Phase am D1 R32
  festlegen).

## To-do (später, nicht jetzt)
- [ ] Kartenerkennung beim Start + Statusmeldung.
- [ ] SD-Backend an den Speicher-Dienst anbinden.

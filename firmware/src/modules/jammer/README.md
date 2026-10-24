# Modul: jammer — 2,4-GHz-Modul (NRF24L01+)

## Vor dem Start lesen
docs/ARBEITSWEISE.md · diese README · core/Module.h

## Was ändere ich wo / Was speichere ich wie
- Eigene Dateien:   firmware/src/modules/jammer/   (nur hier Code dieses Moduls)
- Einstellungen:    firmware/src/config.h           (NRF24-SPI-Pins)
- Speicher-Namensraum: — (speichert nichts)

---

**Hardware:** NRF24L01+ über SPI (SCK = GPIO18, MISO = GPIO19, MOSI = GPIO23,
CE = GPIO17, CSN = GPIO5). **Bibliothek:** `RF24`.
**Rolle:** Aktor. `name()` = `"jammer"`.

## Status dieses Moduls

Die **eigentliche Störfunktion implementiert die Gruppe selbst** — dieses
README beschreibt nur die Einbindung ins Gerät (Vertrag, Menüpunkt, Status),
nicht die Sendelogik.

## Vertrag (wie bei jedem Modul)

| Methode | Zweck |
|---|---|
| `handleCommand("start"/"stop", …)` | Modul aktivieren/deaktivieren |
| `statusJson(obj)` | z. B. `{ "ok": true, "active": false }` zurückgeben |
| `update()` | nicht-blockierend; reagiert auf den aktiven Zustand |

Damit das Gerät bedienbar bleibt, muss auch dieses Modul die
Nicht-Blockieren-Regel aus [`../../core/README.md`](../../core/README.md) einhalten
und jederzeit sauber über `stop` abschaltbar sein.

## Ohne Hardware (Teil-Hardware, ARBEITSWEISE.md §6 Regel 5)

- **NRF24 fehlt:** `radio.begin()` gibt `false` zurück (alternativ
  `radio.isChipConnected()`). Dann einmal `[jammer] NRF24 nicht gefunden - Modul
  inaktiv` ausgeben, `ok = false`, `start` gibt `false` zurück.
- Kein `while (1);` aus den RF24-Beispielen übernehmen.
- Der SPI-Bus wird mit der SD-Karte geteilt. Fehlt eines der beiden Module,
  funktioniert das andere trotzdem.
- **Testen ohne Display/Handy:** im Serial Monitor `jammer start`, `status`,
  `jammer stop`.

## To-do

- [ ] `JammerModule.h` / `.cpp` nach dem Modul-Muster anlegen (Vertrag + sicherer
      Aus-Zustand als Standard beim Start).
- [ ] Sendelogik gemäß dem abgestimmten Rahmen durch die Gruppe ergänzen.

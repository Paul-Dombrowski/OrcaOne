# Modul: jammer — 2,4-GHz-Modul (NRF24L01+)

**Hardware:** NRF24L01+ über SPI (SCK = GPIO18, MISO = GPIO19, MOSI = GPIO23,
CE = GPIO17, CSN = GPIO5). **Bibliothek:** `RF24`.
**Rolle:** Aktor. `name()` = `"jammer"`.

## Status dieses Moduls

Die **eigentliche Störfunktion implementiert die Gruppe selbst** — dieses
README beschreibt nur die Einbindung ins Gerät (Vertrag, Menüpunkt, Status),
nicht die Sendelogik.

> ⚠️ **Rechtlich & betrieblich:** Der Betrieb von Störsendern ist stark reguliert.
> Dieses Modul nur in dem Rahmen aktivieren, der für das Projekt **ausdrücklich
> abgestimmt** wurde. **Nicht während der Live-Demo laufen lassen**, solange die
> WLAN-Fernsteuerung (Aufgabe 3) vorgeführt wird: ein aktives 2,4-GHz-Modul stört
> genau das eigene WLAN und das der Nachbarstände. Als Sicherheitsnetz einen
> klaren Aus-Schalter/Not-Aus im Menü und in der Weboberfläche vorsehen.

## Vertrag (wie bei jedem Modul)

| Methode | Zweck |
|---|---|
| `handleCommand("start"/"stop", …)` | Modul aktivieren/deaktivieren |
| `statusJson(obj)` | z. B. `{ "active": false }` zurückgeben |
| `update()` | nicht-blockierend; reagiert auf den aktiven Zustand |

Damit das Gerät bedienbar bleibt, muss auch dieses Modul die
Nicht-Blockieren-Regel aus [`../../core/README.md`](../../core/README.md) einhalten
und jederzeit sauber über `stop` abschaltbar sein.

## To-do

- [ ] `JammerModule.h` / `.cpp` nach dem Modul-Muster anlegen (Vertrag + sicherer
      Aus-Zustand als Standard beim Start).
- [ ] Sendelogik gemäß dem abgestimmten Rahmen durch die Gruppe ergänzen.

# Ein-/Ausgangsübersicht

Alle Signale des Systems, getrennt nach **Eingängen (Sensoren)** und
**Ausgängen (Aktoren)**, mit Kennzeichnung automatisch (A) / manuell (M).

## Eingänge (Sensoren)

| Signal / Bauteil | Modul | Anschluss | A/M | Beschreibung |
|---|---|---|---|---|
| NFC/RFID-Tag (PN532) | `nfc` | I2C (21/22) | A + M | UID/Daten eines Tags einlesen |
| IR-Signal (KY-022) | `ir` | GPIO34 | M | Signal einer Fernbedienung aufnehmen |
| Drehgeber (KY-040) | `input` | GPIO25/26/27 | M | Menü navigieren + bestätigen |

## Ausgänge (Aktoren)

| Signal / Bauteil | Modul | Anschluss | A/M | Beschreibung |
|---|---|---|---|---|
| IR-LED (KY-005) | `ir` | GPIO4 | A + M | gespeichertes IR-Signal senden |
| OLED-Display (SH1106) | `display` | I2C (21/22) | A + M | Menü & Status anzeigen |
| NFC-Writer (PN532) | `nfc` | I2C (21/22) | M | Daten auf einen Tag schreiben |
| 2,4-GHz-Modul (NRF24L01+) | `jammer` | SPI (18/19/23/17/5) | M | Störsender (abgestimmter Rahmen) |

## Automatische Funktion (Pflicht)

**Regel (Vorschlag):**
> *Wenn* im Automatik-Modus ein **registrierter NFC-Tag** erkannt wird (Eingang,
> Sensor `nfc`), *dann* sendet das Gerät automatisch ein hinterlegtes IR-Signal
> (Ausgang, Aktor `ir`) **und** zeigt „Zugang erkannt" auf Display + Handy.

- Realisiert im `ModeManager` ([`../../firmware/src/core/`](../../firmware/src/core/)).
- Der **Automatik-Modus** wird per Handy (`system/setMode`) oder im Menü ein-/
  ausgeschaltet. Im Manuell-Modus greift die Regel nicht.

> Die konkrete Regel ist mit der Gruppe final abzustimmen — wichtig ist nur, dass
> ein **Sensorwert automatisch einen Aktor auslöst** und sich der Automatik-Modus
> umschalten lässt.

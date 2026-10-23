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

## Automatische Funktion (Pflicht) — festgelegt

**Hauptregel (benotete Automatik):**
> *Wenn* im Automatik-Modus am NFC-Leser (Sensor `nfc`) ein **registrierter Tag**
> erkannt wird — d. h. die gelesene UID == `AUTO_TRIGGER_UID` aus `config.h` —,
> *dann* sendet OrcaOne automatisch das IR-Signal aus Slot `AUTO_ACTION_IR_SLOT`
> (Aktor `ir`) **und** zeigt „Zugang erkannt" auf OLED (Aktor `display`) und
> Handy.

Damit löst ein **Sensoreingang (NFC)** selbstständig einen **Aktor (IR-LED)** aus
— genau die geforderte „automatische Reaktion auf einen Sensorwert".

- Realisiert im `ModeManager` ([`../../firmware/src/core/`](../../firmware/src/core/)).
- Der **Automatik-Modus** wird per Handy (`system/setMode`) oder im Menü ein-/
  ausgeschaltet. Im **Manuell**-Modus greift die Regel nicht; dann läuft alles nur
  über Menü/Handy.
- Die konkrete UID wird beim Aufbau eingetragen (per „nfc read" im Serial-Monitor
  ablesen, in `config.h` → `AUTO_TRIGGER_UID` eintragen).

**Optionale zweite Regel (Erweiterung, nicht Pflicht):** Empfängt der IR-Empfänger
(Sensor `ir`) im Auto-Modus einen hinterlegten Fernbedienungs-Code, sendet das
Gerät eine gespeicherte Antwort. Nur umsetzen, wenn Zeit bleibt.

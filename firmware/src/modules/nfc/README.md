# Modul: nfc — NFC/RFID lesen, schreiben, klonen

**Hardware:** PN532-Modul über I2C (SDA = GPIO21, SCL = GPIO22, Adresse 0x24).
**Bibliothek:** `Adafruit PN532`.
**Rolle:** Sensor (Tag lesen) **und** Aktor (Tag beschreiben/klonen).
`name()` = `"nfc"`.

## Was es können soll

- Einen in die Nähe gehaltenen NFC/RFID-Tag **lesen** (UID + Dateninhalt).
- Gelesene Daten zwischenspeichern und auf einen anderen (beschreibbaren) Tag
  **schreiben** → „klonen".
- Gelesene UID an Display und Handy melden.
- Bis zu **3 Tags** speichern (`NFC_SLOT_COUNT` in [`../../config.h`](../../config.h)).

## Befehle (`handleCommand`)

| action | params | Wirkung |
|---|---|---|
| `read` | – | nächsten Tag einlesen, UID + Daten merken |
| `write` | `{ "slot": <n> }` | gemerkte Daten auf aktuellen Tag schreiben |
| `clone` | – | lesen und direkt zum Schreiben vormerken |

## Status (`statusJson`)

```json
{ "ready": true, "lastUid": "04A1B2C3", "tagPresent": false, "stored": 1 }
```

## Umsetzungshinweise (auch für KI)

- In `begin()`: `nfc.begin()` und `nfc.SAMConfig()` aufrufen, bei Fehler per
  `Serial.println` melden.
- In `update()`: **nicht blockierend** nach einem Tag suchen. Die Adafruit-
  Funktion `readPassiveTargetID(...)` kann mit kurzem Timeout aufgerufen werden;
  pro Durchlauf nur ein kurzer Versuch, dann zurückkehren.
- Pins/Adresse ausschließlich aus [`../../config.h`](../../config.h) nehmen.

## Idee zur Handy-Integration

Das Smartphone hat selbst NFC. Perspektivisch könnte das Handy gelesene/kopierte
Tag-Daten vom ESP32 abrufen (über die Web-API) und selbst weiterverwenden. Erst
die Grundfunktion (lesen/schreiben am Gerät) bauen, diese Erweiterung später.

## To-do

- [ ] `NfcModule.h` / `.cpp` nach dem Muster aus [`../README.md`](../README.md) anlegen.
- [ ] `read` umsetzen und UID im Serial-Monitor ausgeben.
- [ ] `write`/`clone` ergänzen.

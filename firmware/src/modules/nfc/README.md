# Modul: nfc — NFC/RFID lesen, schreiben, klonen

## Vor dem Start lesen
docs/ARBEITSWEISE.md · diese README · core/Module.h · storage/README.md

## Was ändere ich wo / Was speichere ich wie
- Eigene Dateien:   firmware/src/modules/nfc/   (nur hier Code dieses Moduls)
- Einstellungen:    firmware/src/config.h        (Pins/Konstanten dieses Moduls)
- Speicher-Namensraum: "nfc"                      (über Storage, siehe storage/README.md)

---

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
- Speichern über `Storage.save("nfc", "slot1", jsonText)`; vorher `Storage.available()`
  prüfen, aber **nie abbrechen**, wenn keine SD da ist (dann nur flüchtig im RAM).

## Idee zur Handy-Integration

Das Smartphone hat selbst NFC. Perspektivisch könnte das Handy gelesene/kopierte
Tag-Daten vom ESP32 abrufen (über die Web-API) und selbst weiterverwenden. Erst
die Grundfunktion (lesen/schreiben am Gerät) bauen, diese Erweiterung später.

## To-do

- [ ] `NfcModule.h` / `.cpp` nach dem Muster aus [`../README.md`](../README.md) anlegen.
- [ ] `read` umsetzen und UID im Serial-Monitor ausgeben.
- [ ] `write`/`clone` ergänzen.

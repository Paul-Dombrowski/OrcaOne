# Modul: nfc — NFC/RFID lesen, schreiben, klonen

## Vor dem Start lesen
docs/ARBEITSWEISE.md · diese README · core/Module.h · storage/README.md

## Was ändere ich wo / Was speichere ich wie
- Eigene Dateien:   firmware/src/modules/nfc/   (nur hier Code dieses Moduls)
- Einstellungen:    firmware/src/config.h        (Pins/Konstanten dieses Moduls)
- Speicher-Namensraum: "nfc"                      (über Storage, siehe storage/README.md)

---

**Hardware:** PN532-Modul über I2C (SDA = GPIO21, SCL = GPIO22, Adresse 0x24).
**Bibliothek:** Elechouse/Seeed-`PN532` + `PN532_I2C` (Transport) + `NDEF`
(NDEF-Inhalte). Liegen als Komponenten in `firmware/components/`
(siehe [`../../../components/README.md`](../../../components/README.md)) —
**nicht** Adafruit PN532.
**Rolle:** Sensor (Tag lesen) **und** Aktor (Tag beschreiben/klonen).
`name()` = `"nfc"`.

**Reichweite/Technik:** PN532 arbeitet auf **13,56 MHz** und deckt damit sowohl
klassische **RFID-Tags** (MIFARE Classic/Ultralight, Hotelkarten) als auch **NFC**
ab. **Nicht** unterstützt: 125-kHz-Karten/Fobs (EM4100 u. Ä.) — dafür bräuchte es
einen anderen Leser.

## Was es aktuell kann

- Beim Start einmalig nach einem NFC/RFID-Tag suchen und dessen UID lesen.
- Mit dem Befehl `read` einen weiteren Lesevorgang anfordern.
- Die zuletzt gelesene UID im Status bereitstellen.
- Die erste Umsetzung unterstützt UIDs bis 7 Bytes, passend zur Signatur des
  vorhandenen PN532-Treibers.

## Was es später können soll

- Gelesene Daten zwischenspeichern und auf einen anderen (beschreibbaren) Tag
  **schreiben** → „klonen". *(Hinweis: Eine 1:1-Kopie inklusive der fest
  eingebrannten UID geht nur mit speziellen „magic"-Karten; normal lässt sich nur
  der beschreibbare Inhalt kopieren.)*
- Gelesene UID an Display und Handy melden.
- Bis zu **3 Tags** speichern (`NFC_SLOT_COUNT` in [`../../config.h`](../../config.h)).

## Befehle (`handleCommand`)

| action | params | Wirkung |
|---|---|---|
| `read` | – | einen weiteren Tag einlesen und die UID merken |
| `write` | `{ "slot": <n> }` | gemerkte Daten auf aktuellen Tag schreiben |
| `clone` | – | lesen und direkt zum Schreiben vormerken |

## Status (`statusJson`)

```json
{ "ok": true, "ready": true, "lastUid": "04A1B2C3", "readPending": false }
```

## Ohne Hardware (Teil-Hardware, ARBEITSWEISE.md §6 Regel 5)

- **PN532 fehlt:** `nfc.getFirmwareVersion()` liefert `0`. Dann einmal
  `[nfc] PN532 nicht gefunden - Modul inaktiv` ausgeben, `ok = false`, `update()`
  tut nichts, `handleCommand()` gibt `false` zurück.
- Kann `Wire.begin()` den Bus nicht starten, meldet das Modul den I²C-Fehler
  einmalig und bleibt inaktiv. Antwortet der PN532 nicht auf die I²C-Adresse,
  wird der fehlgeschlagene Schreibversuch beendet, ohne wiederholt ACK-Leseversuche
  auszuführen.
- ⚠️ Die Beispiel-Sketche dieser Bibliothek machen an dieser Stelle `while (1);`.
  **Nicht übernehmen**, sonst hängt das ganze Gerät, sobald jemand ohne PN532
  testet.
- Das OLED-Display teilt sich den I2C-Bus mit dem PN532. Fehlt eines der beiden,
  funktioniert das andere trotzdem.
- **Aktuell testen:** im Serial Monitor nach dem Start die ausgegebene
  PN532-Meldung abwarten und einen Tag an den Leser halten. Bis Serial-Konsole
  und CommandRouter eingebunden sind, startet das Modul beim Einschalten
  selbstständig einen Lesevorgang. Danach wird `nfc read` zum erneuten Auslesen
  verfügbar sein.

## Umsetzungshinweise (auch für KI)

- Einbinden (I2C):
  ```cpp
  #include <Wire.h>
  #include <PN532_I2C.h>
  #include <PN532.h>
  // optional fuer NDEF-Inhalte:
  #include <NfcAdapter.h>

  PN532_I2C pn532_i2c(Wire, PIN_I2C_SDA, PIN_I2C_SCL, PN532_I2C_ADDR);
  PN532     nfc(pn532_i2c);          // oder: NfcAdapter nfc(pn532_i2c);
  ```
- `nfc.begin()` initialisiert den I²C-Bus und die Adresse über die zentralen
  Werte aus `config.h`.
  Danach Firmware-Version prüfen und `SAMConfig()` aufrufen.
- `update()` fragt mit Intervall und begrenztem Timeout ab. Der PN532-Treiber
  wartet intern synchron auf I²C-Antworten; jeder Versuch ist daher zeitlich
  begrenzt, aber nicht vollständig asynchron.
- Vor dem Scannen wird `setPassiveActivationRetries()` auf einen endlichen Wert
  gesetzt. Der PN532 muss einen Suchbefehl beenden können, bevor `update()` den
  nächsten sendet; der Host-Timeout ist dafür größer als ein einzelnes Retry.
- Die PN532-Antwort auf `SAMConfig()` kann ohne Nutzdaten kommen. Der
  Antwortparser akzeptiert deshalb einen gültigen Antwortframe mit einer
  Frame-Länge von 2 Bytes (TFI + Antwortcode).
- NDEF-Inhalte (optional): über `NfcAdapter` — `tagPresent()`, `read()` →
  `NfcTag`, `write(message)`.
- Pins/Adresse ausschließlich aus [`../../config.h`](../../config.h) nehmen.
- Speichern über `Storage.save("nfc", "slot1", jsonText)`; vorher `Storage.available()`
  prüfen, aber **nie abbrechen**, wenn keine SD da ist (dann nur flüchtig im RAM).

## Sicherheit: NFC-Tags sind untrusted Eingaben

Ein Tag kann **beliebige Bytes** enthalten — jeder kann einen Tag frei
beschreiben. Die eingebundenen Bibliotheken haben in der Upstream-Fassung bekannte
Speicherfehler beim Verarbeiten solcher Daten (von der automatischen
Sicherheitsprüfung gemeldet, 2026-10-24):

- **NDEF-Parsing** (`components/NDEF/NdefMessage.cpp`): Längenfelder aus dem Tag
  wurden ungeprüft als Index/Größe benutzt. Der Parser ist jetzt lokal gepatcht:
  Feldlängen werden gegen die verbleibende Nachricht geprüft, bevor Daten kopiert
  werden. Beschädigte oder abgeschnittene Records werden verworfen.
- **Mifare-Classic-Schreiben** (`components/NDEF/MifareClassic.cpp`): Puffergröße
  aus `messageLength` als VLA auf dem Stack → Stack-Overflow bei großer Länge.
- **Tag-Emulation** (`components/PN532/emulatetag.cpp`): Buffer-Overflow bei
  APDU-Daten. Nur relevant, falls Emulation genutzt wird (**nicht** vorgesehen).

Auf dem ESP32 (kein Speicherschutz) ist die wahrscheinliche Folge ein
Absturz/Reboot. **Beim Umsetzen dieses Moduls deshalb:**
- Tag-Länge vor dem Parsen gegen eine feste Obergrenze prüfen; Längen aus dem Tag
  nie ungeprüft als Index/Größe verwenden.
- `emulatetag` nicht verwenden.
- Alternativ die Bibliotheken patchen (Bounds-Checks) — dann im jeweiligen
  `components/<Lib>/HERKUNFT.md` als Änderung vermerken.

## Idee zur Handy-Integration

Das Smartphone hat selbst NFC. Perspektivisch könnte das Handy gelesene/kopierte
Tag-Daten vom ESP32 abrufen (über die Web-API) und selbst weiterverwenden. Erst
die Grundfunktion (lesen/schreiben am Gerät) bauen, diese Erweiterung später.

## To-do

- [x] `NfcModule.h` / `.cpp` nach dem Muster aus [`../README.md`](../README.md) anlegen.
- [x] `read` umsetzen und UID im Serial-Monitor ausgeben.
- [ ] `write`/`clone` ergänzen.

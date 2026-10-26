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
**Rolle:** Rohdaten-Leser für ISO14443A- und Type-2-Tags.
`name()` = `"nfc"`.

**Reichweite/Technik:** PN532 arbeitet auf **13,56 MHz** und deckt damit sowohl
klassische **RFID-Tags** (MIFARE Classic/Ultralight, Hotelkarten) als auch **NFC**
ab. **Nicht** unterstützt: 125-kHz-Karten/Fobs (EM4100 u. Ä.) — dafür bräuchte es
einen anderen Leser.

## Was es aktuell kann

- Beim Start fortlaufend nach ISO14443A-Tags suchen und die vom PN532 gelieferten
  Erkennungsdaten sowie rohe Antworten auf Type-2-Leseanforderungen als Hex
  ausgeben.
- Keine UID-Deduplizierung, NDEF-Interpretation oder textuelle Einordnung von
  Seiteninhalten und Kartenstatus vornehmen.
- Die Rohdatenansicht ist absichtlich auf Pages 4-39 begrenzt: Sie umfasst den
  üblichen Nutzspeicherbereich gängiger Type-2-Testtags und liest weder
  Hersteller-/UID-Pages 0-3 noch darüberliegende Konfigurationspages. Nicht
  unterstützte, kleinere oder geschützte Tags können vor Page 39 abbrechen.

## Befehle (`handleCommand`)

| action | params | Wirkung |
|---|---|---|
| `read` | – | einen neuen Scan-Zyklus sofort anstoßen |

## Status (`statusJson`)

```json
{ "ok": true, "ready": true }
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
- **Aktuell testen:** im Serial Monitor nach dem Start einen Tag an den Leser
  halten. Das Modul sucht fortlaufend nach Tags und gibt empfangene Bytes als Hex
  aus.

## Umsetzungshinweise (auch für KI)

- Einbinden (I2C):
  ```cpp
  #include <Wire.h>
  #include <PN532_I2C.h>
  #include <PN532.h>
  PN532_I2C pn532_i2c(Wire, PIN_I2C_SDA, PIN_I2C_SCL, PN532_I2C_ADDR);
  PN532     nfc(pn532_i2c);
  ```
- `nfc.begin()` initialisiert den I²C-Bus und die Adresse über die zentralen
  Werte aus `config.h`.
  Danach Firmware-Version prüfen und `SAMConfig()` aufrufen.
- `update()` fragt mit Intervall und begrenztem Timeout ab. Der PN532-Treiber
  wartet intern synchron auf I²C-Antworten; jeder Versuch ist daher zeitlich
  begrenzt, aber nicht vollständig asynchron.
- Der PN532 erhält am bereits von `Wire` initialisierten Bus einen eigenen
  ESP-IDF-I²C-Gerätehandle. Dessen SCL-Clock-Stretch-Timeout ist auf 20 ms
  gesetzt (statt des ESP32-Arduino-Defaults von 2 ms), weil der PN532 die
  I²C-Taktleitung während interner Verarbeitung strecken kann.
- Während des Wartens fragt der Treiber nur das einzelne PN532-Statusbyte ab;
  den Antwortframe liest er erst, sobald der PN532 „bereit“ meldet.
- Die Rohdatenansicht nutzt `mifareultralight_ReadPagesRaw()` und gibt die
  empfangenen Antwortbytes pro Vier-Page-Leseanforderung aus. Sie liest nicht
  universell alle Tag-Technologien.
- Jede Zeile enthält ausschließlich Bytes als Hex. Der erste Hex-Datensatz ist
  eine empfangene Erkennungsantwort des PN532 (ohne PN532-/I²C-Transportrahmen);
  danach folgen rohe Antwortbytes zu Leseanforderungen für Pages 4-39. Es gibt
  keine textuelle Sortierung nach UID, leerem Inhalt oder Kartenstatus.
- Vor dem Scannen wird `setPassiveActivationRetries()` auf einen endlichen Wert
  gesetzt. Der PN532 muss einen Suchbefehl beenden können, bevor `update()` den
  nächsten sendet; der Host-Timeout ist dafür größer als ein einzelnes Retry.
- Die PN532-Antwort auf `SAMConfig()` kann ohne Nutzdaten kommen. Der
  Antwortparser akzeptiert deshalb einen gültigen Antwortframe mit einer
  Frame-Länge von 2 Bytes (TFI + Antwortcode).
- Pins/Adresse ausschließlich aus [`../../config.h`](../../config.h) nehmen.

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

## To-do

- [x] `NfcModule.h` / `.cpp` nach dem Muster aus [`../README.md`](../README.md) anlegen.
- [x] Empfangene Daten als Hex im Serial-Monitor ausgeben.

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
  Erkennungsdaten sowie Antworten einer mehrstufigen Type-2-Leseabfolge als Hex
  ausgeben.
- Leere Suchzyklen bleiben still. Bei einem erkannten Tag sind Erkennung,
  gesendete Type-2-READ-Page und Antwortlaenge in der seriellen Ausgabe
  beschriftet.
- Keine UID-Deduplizierung, NDEF-Interpretation oder textuelle Einordnung von
  Seiteninhalten und Kartenstatus vornehmen.
- Als Beispiel werden Pages 4 und 8 gelesen. Die Folge ist in `ReadPages` in
  `NfcModule.cpp` konfiguriert; jeder Eintrag löst einen Type-2-READ für vier
  aufeinanderfolgende Seiten aus.

## Befehle (`handleCommand`)

| action | params | Wirkung |
|---|---|---|
| `read` | – | einen neuen Scan-Zyklus sofort anstoßen (beendet ggf. Emulation) |
| `emulate` | – | Karten-Emulation starten: zuletzt gelesene Karte gegenüber einem Leser (Handy-App) zurückspielen. Setzt einen vorherigen erfolgreichen `read` voraus. |
| `stop` | – | Emulation beenden, zurück in den Lesebetrieb |
| `clear` | – | zuletzt gelesene Kartendaten verwerfen |

## Status (`statusJson`)

```json
{ "ok": true, "ready": true, "emulating": false, "emuReady": false }
```

`emuReady` = ein Mitschnitt zum Zurückspielen liegt vor; `emulating` = Emulation
gerade aktiv.

## Karten-Emulation (Capture & Replay)

Der PN532 kann die zuletzt gelesenen Kartendaten so zurückspielen, dass eine
NFC-Scanner-/EMV-Reader-App am Handy **dieselben Werte** anzeigt wie bei der echten
Karte.

- **Prinzip:** Beim `read` werden die **rohen** Kartenantworten (PPSE, FCI, GPO,
  AFL-Records) mitgeschnitten. Bei `emulate` gibt sich der PN532 als
  ISO14443-4-Karte (`tgInitAsTarget`, SAK 0x20) aus und beantwortet die
  Standard-APDU-Folge `SELECT PPSE → SELECT AID → GPO → READ RECORD` mit genau
  diesen Bytes. Dadurch ist die Anzeige in der App identisch.
- **Ablauf:** echte Karte an den Leser → `read` → Karte entfernen → `emulate` →
  Handy an den PN532 halten. `stop` kehrt in den Lesebetrieb zurück.
- **Grenze (wichtig):** Nur **statische Anzeigedaten**. Eine **Zahlung ist nicht
  möglich** — das pro Transaktion dynamisch erzeugte EMV-Kryptogramm (ARQC) steckt
  im Secure Element der Karte und ist über NFC nicht lesbar, also auch nicht
  klonbar. Ein zahlungsprüfendes Terminal lehnt die Emulation ab.
- **Timing:** `emulate` lässt `update()` je Zyklus bis zu `NFC_EMU_INIT_TIMEOUT_MS`
  (config.h) auf einen Leser warten — in dieser Zeit blockiert der Loop. Nur im
  Emulationsmodus relevant.
- ⚠️ **Hardware-Status:** Die Logik ist mit der echten Toolchain übersetzt, aber
  die ISO-DEP-Target-Emulation des PN532 ist in der Praxis empfindlich
  (Timing, Firmware-Abhängigkeiten) und **noch nicht auf Hardware verifiziert**.
  Es wird der eigene APDU-Responder in `NfcModule.cpp` genutzt, **nicht**
  `components/PN532/emulatetag.cpp` (dort dokumentierter Overflow, nur NDEF).

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
- Die Type-2-Leseabfolge nutzt `mifareultralight_ReadPagesRaw()`. Sie liest
  nicht universell alle Tag-Technologien und sendet keine frei wählbaren
  APDUs oder Schreibbefehle.
- Jede Antwort bleibt in `NfcModule::_lastCommandResponse` gespeichert; die
  Byteanzahl steht in `_lastCommandResponseLength`. `processCommandResponse()`
  verarbeitet die Antwort vor dem nächsten Lesezugriff und ist der vorgesehene
  Erweiterungspunkt für eigene Auswertungen.
- Jede Zeile enthält ausschließlich Bytes als Hex. Der erste Hex-Datensatz ist
  eine empfangene Erkennungsantwort des PN532 (ohne PN532-/I²C-Transportrahmen);
  danach folgen rohe Antworten für die Seiten-Leseanforderungen. Es gibt keine
  textuelle Sortierung nach UID, leerem Inhalt oder Kartenstatus.
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

## Dateien

| Datei | Verantwortlichkeit |
|---|---|
| `EmvTypes.h/.cpp` | Datentypen EmvAid, CardData, EmvCapture (+ Methoden) |
| `Tlv.h/.cpp` | zustandslose TLV-/Byte-Helfer |
| `EmvTags.h/.cpp` | EMV-Tag-Namen + Tag-Hex-Ausgabe |
| `EmvDump.h/.cpp` | annotierter Diagnose-Dump der Kartendaten |
| `EmvReader.h/.cpp` | EMV-Lesefluss (PPSE→AID→GPO→Records), füllt CardData+EmvCapture |
| `EmvEmulator.h/.cpp` | Karten-Emulation (Capture-Replay, ISO14443-4-PICC) |
| `NfcModule.h/.cpp` | Module-Vertrag, Orchestrierung, Befehls-Routing |

## To-do

- [x] `NfcModule.h` / `.cpp` nach dem Muster aus [`../README.md`](../README.md) anlegen.
- [x] Empfangene Daten als Hex im Serial-Monitor ausgeben.
- [x] Karten-Emulation (Capture & Replay) implementiert — **auf Hardware testen.**

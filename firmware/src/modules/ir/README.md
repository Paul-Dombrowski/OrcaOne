# Modul: ir — Infrarot aufnehmen & senden

## Vor dem Start lesen
docs/ARBEITSWEISE.md · diese README · core/Module.h · storage/README.md

## Was ändere ich wo / Was speichere ich wie
- Eigene Dateien:   firmware/src/modules/ir/   (nur hier Code dieses Moduls)
- Einstellungen:    firmware/src/config.h       (Pins/Konstanten dieses Moduls)
- Speicher-Namensraum: "ir"                      (über Storage, siehe storage/README.md)

---

**Hardware:** KY-022 Empfänger an GPIO34 (nur Eingang), KY-005 / IR-LED Sender an
GPIO4. **Bibliothek:** `IRremoteESP8266`.
**Rolle:** Sensor (Signal empfangen) **und** Aktor (Signal senden).
`name()` = `"ir"`.

## Was es können soll

Zweck ist **Störung** („IR-Jammer"): ein IR-gesteuertes Gerät unbenutzbar machen.
Zwei Betriebsarten, beide laufen bis `stop`:

- **Aufnehmen/Senden:** Signale eines Zeitfensters mitschneiden (`record` →
  `stop`) und blind, zufällig in Dauerschleife zurückspielen (`send`). Es kommt
  bewusst nicht auf das „richtige" Signal an.
- **[Erweiterung] Ausschalt-Sequenz:** kuratierte Off-Code-Liste von der SD-Karte
  in Dauerschleife senden (Prinzip „TV-B-Gone").

**Slots (Ringpuffer):** `record` legt automatisch einen **neuen** Slot an; ist der
Puffer voll, wird der **älteste** überschrieben. Anzahl backend-abhängig:
`IR_SD_SLOT_COUNT` (mit SD) bzw. `IR_RAM_SLOT_COUNT` (ohne SD), beide in
[`../../config.h`](../../config.h). Format: rohe Timing-Arrays als JSON, je Slot
bis `IR_MAX_SIGNALS_PER_SLOT` Signale.

## Befehle (`handleCommand`)

| action | params | Wirkung |
|---|---|---|
| `record` | – | Aufnahme starten: alle Signale bis `stop` in einen **neuen** Slot |
| `send` | `{ "slot": <n> }` *(optional)* | Slot zufällig in Dauerschleife senden; ohne `slot` den **neuesten** |
| `shutoff` | – | **[E]** Ausschalt-Codeliste von SD in Dauerschleife senden |
| `stop` | – | laufende Aufnahme / Senden / Ausschalt-Sequenz beenden |

## Status (`statusJson`)

```json
{ "ok": true, "hardwareDetectable": false, "state": "idle", "slots": 0, "newest": 0, "hasShutoff": false }
```

`state` = `idle` | `recording` | `sending` | `shutoff`; `slots` = Anzahl belegter
Slots; `newest` = zuletzt beschriebener Slot (0 = keiner); `hasShutoff` = Ausschalt-
liste auf SD vorhanden.

## Ohne Hardware (Teil-Hardware, ARBEITSWEISE.md §6 Regel 5)

- Empfänger und Sender lassen sich nicht erkennen. `ok` ist deshalb immer `true`
  und `hardwareDetectable` ist `false`. Die Display-Meldung weist darauf hin,
  dass der Sensor nicht geprüft werden kann; eine fehlende IR-Hardware kann nicht
  als Fehler diagnostiziert werden. Ohne Sender wird eben „ins Leere“ gesendet,
  das ist harmlos.
- ⚠️ **Rauschen ohne Empfänger:** GPIO34 hat keinen internen Pull-up. Ist kein
  KY-022 angeschlossen, hängt der Pin in der Luft und kann Zufallssignale liefern,
  die als „unbekanntes IR-Signal“ erkannt werden. Deshalb:
  - kurze Störsignale ausfiltern: `irrecv.setUnknownThreshold(12)` (Wert wie im
    Beispiel `IRrecvDumpV2` der Bibliothek),
  - außerhalb von `record` wird **gar nicht dekodiert** (`decode()` wird nur im
    Aufnahme-Zustand aufgerufen) — so erzeugt ein offener GPIO34 keine Slots.
- **Testen ohne Display/Handy:** im Serial Monitor `ir record`, Taste(n) auf der
  Fernbedienung drücken, `ir stop`, dann `ir send` (neuester Slot). `ir stop`
  beendet auch das Senden.

## Umsetzungshinweise (auch für KI)

- Empfang: `IRrecv` auf `PIN_IR_RX`, `decode(&results)` in `update()` (kehrt sofort
  zurück → nicht blockierend). Gespeichert werden **rohe Timing-Arrays**
  (`resultToRawArray` / `getCorrectedRawLength`), nicht protokoll-dekodiert —
  robust auch für unbekannte Signale (Jammer-Zweck).
- Senden: `IRsend` auf `PIN_IR_TX` mit `sendRaw(raw, len, khz)`.
- GPIO34 ist **input-only** (kein interner Pull-up). Mit Empfänger passt das,
  weil das KY-022-Modul den Pegel selbst festlegt. Ohne Empfänger siehe oben.
- Pins aus [`../../config.h`](../../config.h).
- Speichern über `Storage.save("ir", "slot1", jsonText)`; vorher `Storage.available()`
  prüfen, aber **nie abbrechen**, wenn keine SD da ist (dann nur flüchtig im RAM).

## To-do

- [x] `IrModule.h` / `.cpp` anlegen.
- [x] `record`/`stop` → Signale eines Fensters in einen Ringpuffer-Slot aufnehmen.
- [x] `send` → Slot (neuester/`slot=n`) zufällig in Dauerschleife senden — **an echtem Gerät testen.**
- [x] **[E]** `shutoff`: Off-Code-Liste von SD in Dauerschleife senden (nicht-blockierend, `millis()`).

> **Ausschaltliste anlegen:** Datei im Namensraum `ir`, Schlüssel `shutoff`, gleiches
> Format wie ein Slot: `{ "v":1, "signals":[ { "khz":38, "raw":[ ... ] }, ... ] }`
> (Schlüssel `codes` wird als Alias akzeptiert). Auf SD unter `/ir/shutoff.json`.

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

- Ein IR-Signal einer Fernbedienung **aufnehmen** (empfangen und speichern).
- Ein gespeichertes Signal auf Befehl wieder **senden** (Replay) → OrcaOne wird
  zur Universalfernbedienung.
- Mehrere Signale in Speicherplätzen (Slots) ablegen — **3 Slots**
  (`IR_SLOT_COUNT` in [`../../config.h`](../../config.h)).
- **[Erweiterung] Shut-Off-Modus:** eine eingebaute Liste von Ausschalt-Codes
  verschiedener Hersteller nacheinander senden (Prinzip „TV-B-Gone"). Ein Befehl →
  das Gerät feuert die Power-Off-Codes der Reihe nach ab.

## Befehle (`handleCommand`)

| action | params | Wirkung |
|---|---|---|
| `record` | `{ "slot": <n> }` | nächstes empfangenes Signal in Slot `n` speichern |
| `send` | `{ "slot": <n> }` | Signal aus Slot `n` senden |
| `shutoff` | – | **[E]** eingebaute Hersteller-Ausschaltcodes nacheinander senden |
| `stop` | – | laufende Aufnahme oder Shut-Off-Sequenz abbrechen |

## Status (`statusJson`)

```json
{ "ok": true, "recording": false, "lastProtocol": "NEC", "slots": [true,false,false] }
```

## Ohne Hardware (Teil-Hardware, ARBEITSWEISE.md §6 Regel 5)

- Empfänger und Sender lassen sich nicht erkennen. `ok` ist deshalb immer `true`.
  Ohne Sender wird eben „ins Leere“ gesendet, das ist harmlos.
- ⚠️ **Rauschen ohne Empfänger:** GPIO34 hat keinen internen Pull-up. Ist kein
  KY-022 angeschlossen, hängt der Pin in der Luft und kann Zufallssignale liefern,
  die als „unbekanntes IR-Signal“ erkannt werden. Deshalb:
  - kurze Störsignale ausfiltern: `irrecv.setUnknownThreshold(12)` (Wert wie im
    Beispiel `IRrecvDumpV2` der Bibliothek),
  - empfangene Signale **nur während `record`** in einen Slot übernehmen, sonst
    höchstens `lastProtocol` aktualisieren.
- **Testen ohne Display/Handy:** im Serial Monitor `ir record slot=1`, Taste auf
  der Fernbedienung drücken, dann `ir send slot=1`.

## Umsetzungshinweise (auch für KI)

- Empfang: `IRrecv` auf `PIN_IR_RX`. In `update()` mit `irrecv.decode(&results)`
  prüfen (kehrt sofort zurück → nicht blockierend) und bei Treffer die Rohdaten
  bzw. Protokoll+Code im aktiven Slot speichern.
- Senden: `IRsend` auf `PIN_IR_TX`; passende `send...()`-Funktion zum gespeicherten
  Protokoll aufrufen.
- GPIO34 ist **input-only** (kein interner Pull-up). Mit Empfänger passt das,
  weil das KY-022-Modul den Pegel selbst festlegt. Ohne Empfänger siehe oben.
- Pins aus [`../../config.h`](../../config.h).
- Speichern über `Storage.save("ir", "slot1", jsonText)`; vorher `Storage.available()`
  prüfen, aber **nie abbrechen**, wenn keine SD da ist (dann nur flüchtig im RAM).

## To-do

- [ ] `IrModule.h` / `.cpp` anlegen.
- [ ] `record` → empfangenes Signal im Serial-Monitor ausgeben.
- [ ] `send` → gespeichertes Signal zurückspielen und an echtem Gerät testen.
- [ ] **[E]** `shutoff`: Tabelle bekannter Power-Off-Codes anlegen und nacheinander
      senden — **nicht-blockierend** als Zustandsautomat (ein Code pro `update()`,
      kleine Pause über `millis()`), damit das Gerät bedienbar bleibt.

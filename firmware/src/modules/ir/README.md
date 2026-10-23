# Modul: ir — Infrarot aufnehmen & senden

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

## Befehle (`handleCommand`)

| action | params | Wirkung |
|---|---|---|
| `record` | `{ "slot": <n> }` | nächstes empfangenes Signal in Slot `n` speichern |
| `send` | `{ "slot": <n> }` | Signal aus Slot `n` senden |
| `stop` | – | Aufnahme-Bereitschaft abbrechen |

## Status (`statusJson`)

```json
{ "recording": false, "lastProtocol": "NEC", "slots": [true,false,false] }
```

## Umsetzungshinweise (auch für KI)

- Empfang: `IRrecv` auf `PIN_IR_RX`. In `update()` mit `irrecv.decode(&results)`
  prüfen (kehrt sofort zurück → nicht blockierend) und bei Treffer die Rohdaten
  bzw. Protokoll+Code im aktiven Slot speichern.
- Senden: `IRsend` auf `PIN_IR_TX`; passende `send...()`-Funktion zum gespeicherten
  Protokoll aufrufen.
- GPIO34 ist **input-only** (kein interner Pull-up) — passt für den Empfänger.
- Pins aus [`../../config.h`](../../config.h).

## To-do

- [ ] `IrModule.h` / `.cpp` anlegen.
- [ ] `record` → empfangenes Signal im Serial-Monitor ausgeben.
- [ ] `send` → gespeichertes Signal zurückspielen und an echtem Gerät testen.

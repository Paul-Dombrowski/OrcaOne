# Menüstruktur (OLED + Drehgeber) — festgelegt

Bedienung lokal am Gerät: **Drehen** = Auswahl bewegen, **Drücken** = bestätigen /
in Untermenü, langer Druck = zurück. Umgesetzt im `MenuController`
([`../../firmware/src/core/`](../../firmware/src/core/)); jeder Menüpunkt löst
einen `handleCommand()`-Aufruf am passenden Modul aus — exakt dieselben Befehle
wie über die Weboberfläche (siehe [`../kommunikation/web-api.md`](../kommunikation/web-api.md)).

```
Hauptmenü
├─ NFC / RFID
│   ├─ Lesen                 → nfc.read
│   ├─ Schreiben ▸ Slot 1-3  → nfc.write { slot }
│   └─ Klonen                → nfc.clone
├─ Infrarot
│   ├─ Aufnehmen ▸ Slot 1-3  → ir.record { slot }
│   └─ Senden    ▸ Slot 1-3  → ir.send   { slot }
├─ 2,4 GHz
│   ├─ Start                 → jammer.start
│   └─ Stop                  → jammer.stop
├─ Modus
│   ├─ Automatik AN          → system.setMode { auto }
│   └─ Automatik AUS         → system.setMode { manual }
└─ Info
    ├─ WLAN / IP-Adresse     (Anzeige)
    └─ Verbundene Geräte     (Anzeige)
```

## Festlegungen

- **IR-Speicher:** 3 Slots (`IR_SLOT_COUNT` in `config.h`).
- **NFC-Speicher:** 3 Slots (`NFC_SLOT_COUNT` in `config.h`).
- **Startbildschirm:** Gerätename + WLAN-Status + aktueller Modus (auto/manuell).
- **Rückmeldungen:** Nach jeder Aktion kurze Statusmeldung im Display
  (z. B. „IR Slot 1 gespeichert"), damit Bedienung nachvollziehbar ist.

## Konsistenzregel

Jeder Menüpunkt hat ein Gegenstück in der Web-API und umgekehrt. Wird ein Befehl
neu aufgenommen, muss er an **drei** Stellen gepflegt werden: hier, in
[`../kommunikation/web-api.md`](../kommunikation/web-api.md) und im
`handleCommand()` des Moduls.

Die **Serial-Konsole** braucht keine eigene Pflege: Sie nutzt dieselben Befehle
(`ir send slot=1`) und bedient das Menü mit `up`/`down`/`ok`/`back`. Siehe
[`../../firmware/src/console/README.md`](../../firmware/src/console/README.md).

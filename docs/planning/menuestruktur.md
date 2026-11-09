# Menüstruktur (OLED + Drehgeber) — festgelegt

Bedienung lokal am Gerät: **Drehen** = Auswahl bewegen, **Drücken** = bestätigen /
in Untermenü, langer Druck = zurück. Die Navigation ist derzeit im
`DisplayModule` umgesetzt; die geplante Auslagerung in einen `MenuController`
steht noch aus. Menüaktionen gehen über den `CommandRouter` an
`handleCommand()` des passenden Moduls — dieselben Befehle wie über die
Weboberfläche (siehe [`../kommunikation/web-api.md`](../kommunikation/web-api.md)).

```
Hauptmenü
├─ NFC / RFID
│   └─ Lesen                 → nfc.read
├─ Infrarot
│   ├─ Aufnehmen              → ir.record
│   └─ Stop                   → ir.stop
├─ 2,4 GHz
│   └─ Hinweis: Modul nicht eingebunden
├─ No-Mercy
│   ├─ No-Mercy AN           → system.setMode { nomercy }  (Jammer + IR-Shutoff an)
│   └─ No-Mercy AUS          → system.setMode { off }      (Jammer + IR stop)
├─ Info
│   ├─ WLAN / IP-Adresse     (Anzeige)
│   └─ Verbundene Geräte     (Anzeige)
```

## Festlegungen

- **Ladeanimation:** Vor Menüaktionen der eingebundenen NFC- und IR-Funktionen
  wird die Orca-Ladeanimation angezeigt.
- **2,4 GHz:** Das Jammer-Modul ist noch nicht implementiert und deshalb nicht
  startbar.
- **Startbildschirm:** Gerätename + WLAN-Status + aktueller Modus (auto/manuell).
- **Rückmeldungen:** Nach jeder Aktion kurze Statusmeldung im Display
  (z. B. „NFC-Suche gestartet"), damit Bedienung nachvollziehbar ist.
- **Fehler:** Abgelehnte Befehle zeigen den konkreten Grund, etwa einen
  fehlenden PN532 oder einen nicht eingebundenen Modul-Router.
- **IR-Erkennung:** IR-Empfänger und -Sender melden sich nicht am ESP32 an.
  Deshalb wird die Aufnahme als gestartet gemeldet, aber ein fehlender
  IR-Sensor oder -Sender kann nicht automatisch diagnostiziert werden.
- **Orca-Animationen:** Der separate Orca-Menüpunkt entfällt; die Ladeanimation
  bleibt als Übergang vor den unterstützten Menüaktionen erhalten.

## Konsistenzregel

Jeder Menüpunkt hat ein Gegenstück in der Web-API und umgekehrt. Wird ein Befehl
neu aufgenommen, muss er an **drei** Stellen gepflegt werden: hier, in
[`../kommunikation/web-api.md`](../kommunikation/web-api.md) und im
`handleCommand()` des Moduls.

Die **Serial-Konsole** braucht keine eigene Pflege: Sie nutzt dieselben Befehle
(`ir send slot=1`) und bedient das Menü mit `up`/`down`/`ok`/`back`. Siehe
[`../../firmware/src/console/README.md`](../../firmware/src/console/README.md).

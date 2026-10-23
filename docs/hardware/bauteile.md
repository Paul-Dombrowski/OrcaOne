# Bauteilliste

| Bauteil | Typ | Funktion im Projekt | Modul |
|---|---|---|---|
| ESP32 DevKit | Mikrocontroller | zentrale Steuerung + WLAN-Access-Point | — |
| PN532 | NFC/RFID-Modul (13,56 MHz) | Tags lesen, schreiben, klonen | `nfc` |
| KY-022 | IR-Empfänger | IR-Signale aufnehmen | `ir` |
| KY-005 / IR-LED | IR-Sender | IR-Signale senden | `ir` |
| NRF24L01+ | 2,4-GHz-Funkmodul | Störsender (abgestimmter Rahmen) | `jammer` |
| OLED 1,3" | Display (SH1106, I2C) | Menü- und Statusanzeige | `display` |
| KY-040 | Drehgeber mit Taster | Menü-Navigation & Auswahl | `input` |

## Verbrauchsmaterial / sonstiges

- Steckbrett oder Lochrasterplatine, Jumperkabel
- gemeinsame Stromversorgung 3,3 V / stabile USB-Versorgung des ESP32
- Stützkondensator ~10 µF für den NRF24L01+ (Stabilität)
- NFC-Testtags (lesbar/beschreibbar) für Demo und Tests

> Die konkreten an der Schule verfügbaren Modell-/Bezeichnungen hier ergänzen,
> sobald die Teile ausgegeben wurden.

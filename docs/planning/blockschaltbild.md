# Blockschaltbild der Kommunikation

Zeigt, wie Smartphone, ESP32 und die Peripherie zusammenhängen und wo
**automatische** (A) bzw. **manuelle** (M) Abläufe stattfinden.

## Gesamtübersicht

```
        ┌─────────────────────┐
        │   Smartphone        │   Weboberfläche im Browser
        │   (Browser)         │   - zeigt Werte & Zustände
        └──────────┬──────────┘   - sendet Befehle (M)
                   │
            WLAN (WebSocket)        ◄── bidirektional, Access-Point vom ESP32
                   │                     (kein Internet nötig)
        ┌──────────▼──────────────────────────────────────┐
        │                 ESP32                            │
        │                                                  │
        │   network/  ──  Webserver + WebSocket            │
        │       │                                          │
        │   core/   ──  ModeManager (A/M) + MenuController  │
        │       │                                          │
        │   modules/  ──  einheitlicher Modul-Vertrag      │
        └───┬─────┬──────┬───────┬────────┬────────────────┘
            │     │      │       │        │
          I2C   GPIO   GPIO    I2C      SPI
            │     │      │       │        │
        ┌───▼─┐ ┌─▼──┐ ┌─▼───┐ ┌─▼────┐ ┌─▼────────┐
        │OLED │ │Dreh│ │ IR  │ │PN532 │ │ NRF24L01+│
        │Disp.│ │geber│ │RX/TX│ │ NFC  │ │ 2,4 GHz  │
        └─────┘ └────┘ └─────┘ └──────┘ └──────────┘
         Aktor  Sensor S+Aktor S+Aktor    Aktor
```

## Lokale Bedienung (M)

```
Drehgeber (input) ──► MenuController ──► Modul.handleCommand()
                             │
                             ▼
                        OLED (display)
```

## Fernsteuerung übers Handy (M)

```
Handy ──{module,action,params}──► network ──► Modul.handleCommand()
Handy ◄──── Status-JSON (alle ~500 ms) ◄────── network ◄── Modul.statusJson()
```

## Automatik (A)

```
Sensor-Modul (z. B. nfc) ──► ModeManager prüft Regel
                                   │ (nur wenn Modus = auto)
                                   ▼
                          Aktor-Modul.handleCommand()  (z. B. ir senden)
                                   │
                                   ▼
                          Meldung an display + Handy
```

Der **Automatik-Modus** wird vom Handy (`system/setMode`) oder im Menü
umgeschaltet. Damit sind manuelle und automatische Funktionen klar getrennt und
jederzeit per Smartphone steuerbar.

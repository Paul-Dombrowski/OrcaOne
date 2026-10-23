# Pinbelegung ESP32

> **Vorschlag** — muss beim realen Aufbau geprüft und ggf. angepasst werden.
> Bei Änderungen **immer** auch [`../../firmware/src/config.h`](../../firmware/src/config.h)
> anpassen (dort stehen dieselben Werte als `#define`).

Annahme: ESP32 DevKit (30-Pin). Alle Module an **3,3 V** und gemeinsame **GND**.

## I2C-Bus (geteilt: OLED + PN532)

| Signal | ESP32-Pin |
|---|---|
| SDA | GPIO21 |
| SCL | GPIO22 |

- OLED (SH1106): I2C-Adresse **0x3C**
- PN532 (NFC): I2C-Adresse **0x24** → kein Konflikt, beide am selben Bus.
- Beim PN532 den Modus-Schalter/Jumper auf **I2C** stellen.

## Drehgeber KY-040

| Signal | ESP32-Pin |
|---|---|
| CLK | GPIO25 |
| DT  | GPIO26 |
| SW (Taster) | GPIO27 |

## Infrarot

| Signal | ESP32-Pin | Hinweis |
|---|---|---|
| IR-Empfänger KY-022 (OUT) | GPIO34 | nur Eingang (input-only) |
| IR-LED / KY-005 Sender | GPIO4 | Ausgang |

## NRF24L01+ (SPI)

| Signal | ESP32-Pin |
|---|---|
| SCK  | GPIO18 |
| MISO | GPIO19 |
| MOSI | GPIO23 |
| CE   | GPIO17 |
| CSN  | GPIO5 |

## Belegte Pins — Übersicht

| GPIO | Verwendung |
|---|---|
| 4  | IR-LED (Sender) |
| 5  | NRF24 CSN |
| 17 | NRF24 CE |
| 18 | NRF24 SCK |
| 19 | NRF24 MISO |
| 21 | I2C SDA (OLED + NFC) |
| 22 | I2C SCL (OLED + NFC) |
| 23 | NRF24 MOSI |
| 25 | Encoder CLK |
| 26 | Encoder DT |
| 27 | Encoder SW |
| 34 | IR-Empfänger |

> Hinweis: GPIO34 hat keinen internen Pull-up (für den IR-Empfänger ok). GPIO5
> ist ein Strapping-Pin, als CSN aber unkritisch.

# Jammer: zweite Antenne (2× NRF24L01+, Option A)

Zweites NRF24L01+ am **selben SPI-Bus** wie das erste, nur mit eigenem CE/CSN.
Ziel: zwei Dauerträger gleichzeitig in der Luft (Bandteilung) → bessere
Abdeckung gegen adaptives Frequency-Hopping (AFH).

> **Recht/Ethik:** Wirksames 2,4-GHz-Jammen nur in der eigenen, kontrollierten
> Testumgebung mit eigenen Geräten. Betrieb eines wirksamen Störsenders im
> öffentlichen Raum ist in Deutschland unzulässig (BNetzA).

## Verkabelung Modul #2

| NRF24 #2 | → ESP32 (D1 R32) | Konstante (`config.h`) | Hinweis |
|---|---|---|---|
| VCC  | **3,3 V** | — | **nicht 5 V!** gleiche 3,3-V-Schiene wie Modul #1 |
| GND  | **GND**   | — | gemeinsame Masse |
| CE   | **GPIO32** | `PIN_NRF2_CE`  | eigener Pin |
| CSN  | **GPIO33** | `PIN_NRF2_CSN` | eigener Pin |
| SCK  | **GPIO18** | `PIN_NRF_SCK`  | **geteilt** mit Modul #1 (parallel) |
| MOSI | **GPIO23** | `PIN_NRF_MOSI` | **geteilt** mit Modul #1 |
| MISO | **GPIO19** | `PIN_NRF_MISO` | **geteilt** mit Modul #1 |
| IRQ  | —         | — | frei lassen |

**Merksatz:** SCK/MOSI/MISO parallel an dieselben Pins wie Modul #1 (18/23/19);
nur CE→32 und CSN→33 separat.

### Modul #1 (zur Erinnerung)
CE=17, CSN=5, SCK=18, MISO=19, MOSI=23 (`PIN_NRF_*`). SD-Karte teilt denselben
Bus mit eigenem CS=16 — während des Jammens läuft nur der Jammer-Modus, kein
paralleler SD-Zugriff.

## Stromversorgung

- Beide Module an **dieselbe saubere 3,3-V-Quelle** (aktuell Handynetzteil →
  hält die Spannung auch unter Last; der frühere Brownout ist damit erledigt).
- **Entkopplung:** je 100 nF direkt an VCC/GND jedes Moduls + ein gemeinsamer
  Elko 220–470 µF an der 3,3-V-Schiene. Zwei PA+LNA auf `RF24_PA_MAX` ziehen
  zusammen deutlich mehr Strom.

## Pin-Caveat GPIO32/33

Auf manchen ESP32-Boards liegt an GPIO32/33 ein 32,768-kHz-RTC-Quarz. Am
D1 R32 sind 32/33 normalerweise frei auf den Headern — vor dem Aufbau kurz
gegen die eigene Pinbelegung prüfen (keine `pinMode`-Belegung sonstwo).

## Funktionsweise (Firmware)

- Band wird geteilt: **Radio 1 sweept Kanäle `JAM_CH_MIN..JAM_CH_MID` (0..40)**,
  **Radio 2 `JAM_CH_MID..JAM_CH_MAX` (40..80)**, im Gleichschritt. Beide halten je
  einen Dauerträger.
- Grenze in `config.h` über `JAM_CH_MID` (Default 40) verschiebbar.
- **Fehlt Modul #2** (nicht verdrahtet/erkannt): der Jammer läuft automatisch
  als Einzel-Antenne mit Voll-Sweep `0..80` weiter — keine tote Bandhälfte.

## Test nach dem Flashen

1. `idf.py build` (Ordner `firmware/`), flashen (COM11).
2. Serial-Monitor @115200 beim Boot:
   - Erwartet: `[jammer] NRF24 bereit` **und**
     `[jammer] NRF24 #2 bereit (Dual-Antenne, Bandteilung)`.
   - Steht stattdessen `[jammer] NRF24 #2 nicht gefunden - Einzel-Antenne, Voll-Sweep`,
     stimmt CE32/CSN33 oder die Versorgung von Modul #2 nicht.
3. `status` → JSON enthält `"ok": true` **und** `"ok2": true`.
4. `jammer start` → beide Träger an (eigener BT/BLE-Link zwischen deinen
   Testgeräten bricht hartnäckiger ab als mit einer Antenne); `jammer stop` →
   beide Träger aus, WLAN-Hotspot kommt zurück.

> Hinweis: Während `jammer start` aktiv ist, ist das WLAN (und damit die
> Weboberfläche) bewusst aus. `jammer stop` deshalb über die **Serial-Konsole**
> geben; danach ist der Hotspot wieder erreichbar.

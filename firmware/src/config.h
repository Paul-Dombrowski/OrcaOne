#pragma once
// ============================================================================
//  config.h - Zentrale Einstellungen fuer das ganze Geraet
// ----------------------------------------------------------------------------
//  ALLE Pin-Nummern, WLAN-Daten und Timings stehen HIER an einer Stelle.
//  Vorteil: Wenn sich die Verkabelung aendert, muss nur diese Datei angepasst
//  werden - nicht jedes einzelne Modul.
//  Die Pinbelegung muss mit docs/hardware/pinbelegung.md uebereinstimmen!
// ============================================================================

// ============================================================================
//  ACHTUNG BOARD: ESP32 D1 R32 (Uno-Formfaktor).
//  Die Pin-Nummern unten stammen noch vom 30-Pin-DevKit und sind fuer das
//  D1 R32 NICHT verifiziert. Vor dem Hardware-Aufbau gegen die D1-R32-
//  Pinbelegung pruefen (docs/hardware/pinbelegung.md). Board-Wechsel spaeter =
//  nur Werte in DIESER Datei aendern, sonst nirgendwo.
// ============================================================================

// ---- WLAN: der ESP32 macht einen eigenen Hotspot auf (kein Internet noetig) ----
// Das AP-Passwort steht NICHT im Repo, sondern in secrets.h (git-ignoriert).
// Vorlage: secrets.example.h nach secrets.h kopieren und Wert eintragen.
// Hinweis: Dieses Passwort ist bewusst ein geteiltes Demo-Passwort - Besucher
// brauchen es, um sich mit dem Hotspot zu verbinden (kein echtes Geheimnis).
#if __has_include("secrets.h")
  #include "secrets.h"
#else
  #define WIFI_AP_PASSWORD "changeme123"   // Fallback, bitte secrets.h anlegen
#endif

#define WIFI_AP_SSID      "OrcaOne"
#define WEBSERVER_PORT    80

// ---- I2C-Bus (geteilt von OLED-Display und PN532-NFC) ----
#define PIN_I2C_SDA       21
#define PIN_I2C_SCL       22

// ---- OLED-Display 1,3" (SH1106, I2C) ----
#define OLED_I2C_ADDR     0x3C
#define OLED_WIDTH        128
#define OLED_HEIGHT       64

// ---- PN532 NFC/RFID (I2C) ----
#define PN532_I2C_ADDR    0x24
#define NFC_POLL_INTERVAL_MS 100
#define NFC_READ_TIMEOUT_MS  250
#define NFC_PASSIVE_RETRIES  0x01
// Karten-Emulation: wie lange tgInitAsTarget je update()-Zyklus auf einen Leser
// (das Handy) wartet. Blockiert update() fuer diese Dauer - daher moderat halten.
#define NFC_EMU_INIT_TIMEOUT_MS 1500
// Ausfuehrliches EMV-Protokoll auf Serial. 0 = ruhig (nur die kompakte, von der
// SD zurueckgelesene Zusammenfassung je Karte). 1 = komplettes Protokoll:
// Roh-APDU-Antworten ([DEBUG]), Schritt-fuer-Schritt ([EMV]), Feld-Echos (>>)
// und der vollstaendige TLV-Karten-Dump. Fuer Fehlersuche auf 1 setzen.
#define NFC_VERBOSE_LOG 0

// ---- Drehgeber KY-040 (Menue-Navigation) ----
#define PIN_ENCODER_CLK   25
#define PIN_ENCODER_DT    26
#define PIN_ENCODER_SW    27   // Taster (Druck = Auswahl)

// ---- Infrarot ----
#define PIN_IR_RX         34   // KY-022 Empfaenger (nur Eingang; GPIO34 = input-only)
#define PIN_IR_TX          4   // KY-005 / IR-LED Sender

// ---- NRF24L01+ (2,4 GHz, SPI) ----
#define PIN_NRF_SCK       18
#define PIN_NRF_MISO      19
#define PIN_NRF_MOSI      23 

#define PIN_NRF_CE        17
#define PIN_NRF_CSN        5

// ---- Jammer (NRF24 Sweep-Task) ----
#define JAM_CH_MIN            0   // unterste NRF-Kanalnummer im Sweep
#define JAM_CH_MAX           80   // oberste NRF-Kanalnummer (80 = 2480 MHz, BLE-Adv 39)
#define JAM_TASK_CORE         0   // eigener Core; loop()/Konsole laufen auf Core 1
#define JAM_TASK_STACK     4096   // Bytes Stack fuer den Sweep-Task (ESP-IDF: Bytes)
#define JAM_TASK_PRIORITY     1   // moderat: System-Tasks duerfen vordraengen
#define JAM_WDT_YIELD_MS     50   // nach so vielen ms kurz yielden (Idle/Watchdog)
// Sendepegel des Dauertraegers. RF24_PA_MAX = maximale Leistung, ABER hoechster
// Stromhunger: bricht die 3,3-V-Versorgung ein (v.a. PA+LNA-Modul!), sendet MAX
// dann instabil/schwaecher. In dem Fall RF24_PA_HIGH oder _LOW testen - oft real
// besser. Werte: RF24_PA_MIN / RF24_PA_LOW / RF24_PA_HIGH / RF24_PA_MAX.
#define JAM_PA_LEVEL   RF24_PA_MAX

// ---- Zweites NRF24 (Option A: selber VSPI-Bus, eigene CE/CSN) ----
#define PIN_NRF2_CE   32   // Radio 2: CE  (freier Output-GPIO, kein Strapping-Pin)
#define PIN_NRF2_CSN  33   // Radio 2: CSN (freier Output-GPIO, kein Strapping-Pin)
#define JAM_CH_MID    40   // Bandteilung: Radio 1 sweept MIN..MID, Radio 2 MID..MAX

// ---- Micro-SD (Add-on, SPI, teilt Bus mit NRF24; eigener CS-Pin) ----
// Vom Speicher-Dienst (storage/) genutzt. Muss ein ANDERER Pin sein als
// PIN_NRF_CSN (5). Am D1 R32 vor dem Hardware-Aufbau verifizieren.
#define PIN_SD_CS      16

// ---- On-Board-LED (Test: blinkt, wenn Build + Flash funktionieren) ----
// D1 R32: blaue LED an GPIO2.
#define PIN_LED_BUILTIN    2
#define LED_BLINK_MS     500   // halbe Periode: 500 ms an, 500 ms aus

// ---- Timings ----
#define STATUS_PUSH_MS   500   // wie oft der Status ans Handy geschickt wird (ms)

// ---- IR-Speicher (Ringpuffer, backend-abhaengig) + Aufnahme/Senden ----
// Slot-Anzahl haengt vom Speicher-Backend ab: mit SD grosszuegig, ohne SD klein
// (RAM, Heap-Schutz). Rohe IR-Mitschnitte sind vergleichsweise gross.
#define IR_SD_SLOT_COUNT         50   // Mitschnitt-Slots mit SD-Karte
#define IR_RAM_SLOT_COUNT         3   // Mitschnitt-Slots ohne SD (fluechtig, RAM)
#define IR_MAX_SIGNALS_PER_SLOT  20   // max. Signale je Mitschnitt-Fenster
#define IR_CAPTURE_BUFFER       256   // IRrecv-Pufferlaenge (rohe Timing-Eintraege)
#define IR_SEND_GAP_MS          100   // Pause zwischen zwei gesendeten Signalen (ms)
#define IR_RECORD_UNKNOWN_THRESHOLD 12 // kurze Stoersignale (GPIO34) ausfiltern

// ---- Speicher-Slots NFC ----
#define NFC_SLOT_COUNT    3    // Anzahl speicherbarer NFC-Tags

// ---- Automatik: registrierte Tag-UID, die im Auto-Modus die Aktion ausloest ----
// Beim Aufbau durch eine echte Tag-UID ersetzen (vom "nfc read" im Serial-Monitor).
#define AUTO_TRIGGER_UID  "04A1B2C3"
#define AUTO_ACTION_IR_SLOT  1   // welches IR-Signal dann automatisch gesendet wird

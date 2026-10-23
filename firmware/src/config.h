#pragma once
// ============================================================================
//  config.h - Zentrale Einstellungen fuer das ganze Geraet
// ----------------------------------------------------------------------------
//  ALLE Pin-Nummern, WLAN-Daten und Timings stehen HIER an einer Stelle.
//  Vorteil: Wenn sich die Verkabelung aendert, muss nur diese Datei angepasst
//  werden - nicht jedes einzelne Modul.
//  Die Pinbelegung muss mit docs/hardware/pinbelegung.md uebereinstimmen!
// ============================================================================

// ---- WLAN: der ESP32 macht einen eigenen Hotspot auf (kein Internet noetig) ----
#define WIFI_AP_SSID      "OrcaOne"
#define WIFI_AP_PASSWORD  "orca12345"   // mind. 8 Zeichen; vor Demo ggf. aendern
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

// ---- Timings ----
#define STATUS_PUSH_MS   500   // wie oft der Status ans Handy geschickt wird (ms)

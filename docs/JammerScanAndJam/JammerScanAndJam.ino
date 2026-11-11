// ============================================================================
//  JammerScanAndJam.ino - gezielter 2,4-GHz-Stoerer (NRF24L01+ PA+LNA)
//  Eigenentwicklung. Idee: nicht blind das ganze Band wischen (Leistung
//  verschmiert ueber 81 Kanaele = schwach), sondern ERST finden wo das Ziel
//  funkt, DANN die ganze Leistung auf diesen engen Bereich konzentrieren.
//
//  NUR kontrollierte Testumgebung, EIGENE Geraete. Wirksame Stoersender im
//  oeffentlichen Raum sind in DE unzulaessig (BNetzA).
//
//  ABLAUF (so benutzt du es):
//   1) MODE_SCAN aktiv lassen, flashen, Serial Monitor @115200 oeffnen.
//      Dein Headset AKTIV benutzen (Musik/Call). Du siehst eine Balkenliste:
//      welche Kanaele "leuchten" = dort funkt das Headset (oder WLAN).
//   2) Den/die aktiven Kanalbereich(e) ablesen. Unten JAM_CH_MIN / JAM_CH_MAX
//      eng darum setzen (z. B. nur 8-12 Kanaele breit).
//   3) Auf MODE_JAM umschalten, flashen. Jetzt steckt die gesamte Sendezeit in
//      diesem schmalen Bereich -> viel staerker als Vollband.
//
//  Tipp: Ist die Aktivitaet beim SCAN ueber das GANZE Band verteilt, ist es ein
//  frequenz-hoppendes Bluetooth (AFH). Dann enger geht nicht - dann JAM mit
//  vollem Bereich 0..80 fahren (und auf Reichweite/Leistung setzen). Viele
//  2,4-GHz-USB-Headsets nutzen aber nur einen SCHMALEN Bereich -> ideal.
// ============================================================================
#include "RF24.h"
#include "esp_bt.h"
#include "esp_wifi.h"

// ---- Modus: genau EINEN aktiv lassen -----------------------------------------
//#define MODE_SCAN
//#define MODE_JAM
#define MODE_HUNT

// ---- Verkabelung (CE, CSN) ---------------------------------------------------
#define CE_PIN   17
#define CSN_PIN   5
#define SPI_BUS  VSPI      // VSPI: SCK18/MISO19/MOSI23 | HSPI: SCK14/MISO12/MOSI13
#define SPI_HZ   16000000

// ---- SCAN-Bereich (meist ganzes Band) ----------------------------------------
#define SCAN_CH_MIN   0
#define SCAN_CH_MAX  80

// ---- JAM-Bereich: NACH dem Scan eng auf die aktiven Kanaele setzen! -----------
//  Beispiel: Headset leuchtet bei Kanal 40..48 -> JAM_CH_MIN 40, JAM_CH_MAX 48.
#define JAM_CH_MIN   0
#define JAM_CH_MAX  80

// ---- HUNT: schmales Stoer-Fenster wandert ueber das Band (Finden nach Gehoer) -
#define HUNT_CH_MIN    0     // unterer Rand des Suchbereichs
#define HUNT_CH_MAX   80     // oberer Rand
#define HUNT_WIDTH     8     // Breite des Stoer-Fensters in Kanaelen
#define HUNT_STEP      8     // Schrittweite beim Weiterschieben (8 = ohne Ueberlappung)
#define HUNT_DWELL_MS 2000   // ms pro Fenster (Zeit zum Hinhoeren)

SPIClass spi(SPI_BUS);
RF24 radio(CE_PIN, CSN_PIN, SPI_HZ);

// Kanal -> ungefaehre Frequenz in MHz (NRF: 2400 + Kanalnummer).
static inline uint16_t chFreq(uint8_t ch) { return 2400 + ch; }

void setup() {
  esp_bt_controller_deinit();   // bordeigene Funkmodule aus -> Reserve + Ruhe
  esp_wifi_stop();
  esp_wifi_deinit();

  Serial.begin(115200);
  spi.begin();
  if (!radio.begin(&spi)) {
    Serial.println("[nrf] nicht gefunden - Verkabelung/Versorgung pruefen!");
    while (true) { delay(1000); }   // nur im Test-Sketch ok
  }
  delay(200);

  radio.setAutoAck(false);
  radio.setRetries(0, 0);
  radio.setPayloadSize(32);
  radio.setAddressWidth(5);
  radio.setPALevel(RF24_PA_MAX, true);
  radio.setDataRate(RF24_2MBPS);       // breiteste Belegung pro Kanal
  radio.setCRCLength(RF24_CRC_DISABLED);

#ifdef MODE_JAM
  radio.stopListening();
  radio.startConstCarrier(RF24_PA_MAX, JAM_CH_MIN);
  Serial.print("[jam] Dauertraeger ueber Kanal ");
  Serial.print(JAM_CH_MIN); Serial.print(".."); Serial.print(JAM_CH_MAX);
  Serial.print(" ("); Serial.print(chFreq(JAM_CH_MIN)); Serial.print("..");
  Serial.print(chFreq(JAM_CH_MAX)); Serial.println(" MHz)");
#endif
#ifdef MODE_SCAN
  Serial.println("[scan] Headset AKTIV benutzen. Balken = Aktivitaet pro Kanal.");
#endif
#ifdef MODE_HUNT
  radio.stopListening();
  radio.startConstCarrier(RF24_PA_MAX, HUNT_CH_MIN);
  Serial.println("[hunt] Schmales Stoer-Fenster wandert ueber das Band.");
  Serial.println("[hunt] Headset laufen lassen und HINHOEREN: sobald es AUSSETZT,");
  Serial.println("[hunt] den zuletzt gezeigten Fenster-Bereich merken = Kanaele des Ziels.");
#endif
}

#ifdef MODE_JAM
void loop() {
  // Dauertraeger so schnell wie moeglich ueber den (engen) Bereich wischen.
  // Enger Bereich => jeder Kanal wird viel oefter/laenger getroffen => stark.
  for (uint8_t ch = JAM_CH_MIN; ch <= JAM_CH_MAX; ch++) {
    radio.setChannel(ch);
  }
}
#endif

#ifdef MODE_HUNT
void loop() {
  static uint8_t winStart = HUNT_CH_MIN;
  uint8_t winEnd = winStart + HUNT_WIDTH - 1;
  if (winEnd > HUNT_CH_MAX) winEnd = HUNT_CH_MAX;

  Serial.printf("[hunt] >>> Fenster CH %u..%u  (%u..%u MHz)  --- jetzt aktiv ---\n",
                winStart, winEnd, chFreq(winStart), chFreq(winEnd));

  // Dieses schmale Fenster HUNT_DWELL_MS lang mit dem Dauertraeger bombardieren
  // (so schnell wie moeglich drueber wischen -> starke Energie auf wenigen Kanaelen).
  uint32_t tEnd = millis() + HUNT_DWELL_MS;
  while (millis() < tEnd) {
    for (uint8_t ch = winStart; ch <= winEnd; ch++) radio.setChannel(ch);
  }

  // Fenster weiterschieben; am Ende zurueck an den Anfang.
  if ((uint16_t)winStart + HUNT_STEP > HUNT_CH_MAX) winStart = HUNT_CH_MIN;
  else winStart += HUNT_STEP;
}
#endif

#ifdef MODE_SCAN
void loop() {
  static uint16_t counts[128];
  for (uint16_t k = 0; k < 128; k++) counts[k] = 0;

  // ~2 s lang abtasten. WICHTIG: pro Kanal FRISCH in den Empfang gehen, sonst
  // aktualisiert das RPD-Register nicht (bewaehrtes RF24-Scanner-Muster).
  const uint32_t tEnd = millis() + 2000;
  while (millis() < tEnd) {
    for (uint8_t ch = SCAN_CH_MIN; ch <= SCAN_CH_MAX; ch++) {
      radio.setChannel(ch);
      radio.startListening();          // frisches RX-Fenster fuer diesen Kanal
      delayMicroseconds(200);          // kurz lauschen (> ~130 us Einschwingen)
      bool rpd = radio.testRPD();      // RPD latcht: Signal >= -64 dBm erkannt?
      radio.stopListening();
      if (rpd) counts[ch]++;
    }
  }

  // Maximum finden (fuer die Balken-Skalierung).
  uint16_t maxc = 0;
  for (uint8_t ch = SCAN_CH_MIN; ch <= SCAN_CH_MAX; ch++)
    if (counts[ch] > maxc) maxc = counts[ch];

  Serial.println("---- Scan ----");
  if (maxc == 0) {
    Serial.println("Keine Aktivitaet erkannt.");
    Serial.println(" - Headset wirklich aktiv & nah? (Musik/Call laufen lassen)");
    Serial.println(" - Falls weiterhin NICHTS: dieses NRF24-Modul kann RPD evtl.");
    Serial.println("   nicht (Klon-Chip). Dann Scan ueberspringen, Bereich manuell suchen.");
  } else {
    for (uint8_t ch = SCAN_CH_MIN; ch <= SCAN_CH_MAX; ch++) {
      if (counts[ch] == 0) continue;
      uint8_t bars = (uint8_t)((uint32_t)counts[ch] * 30 / maxc);
      Serial.printf("CH %3u (%u MHz) | ", ch, chFreq(ch));
      for (uint8_t b = 0; b < bars; b++) Serial.print('#');
      Serial.printf("  (%u)\n", counts[ch]);
    }
    Serial.println("Starke Haeufung = dort funkt das Ziel -> JAM-Bereich eng dorthin.");
  }
}
#endif

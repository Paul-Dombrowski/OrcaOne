// ============================================================================
//  main.cpp - Einstiegspunkt der Firmware
// ----------------------------------------------------------------------------
//  Diese Datei "verdrahtet" alle Teile miteinander. Sie enthaelt SELBST keine
//  Hardware-Logik - die steckt in den Modulen (firmware/src/modules/).
//
//  Ablauf:
//    setup()  -> alle Module einmal initialisieren (begin())
//    loop()   -> allen Modulen staendig update() geben + Netzwerk bedienen
//                + Serial-Konsole bedienen (Bedienung ohne Display/Drehgeber)
//
//  Fehlt ein Bauteil, laeuft trotzdem alles andere weiter (ARBEITSWEISE.md §6).
//
//  HINWEIS: Dies ist ein GERUEST. NFC ist als erster Grundtest direkt
//  eingebunden; die TODO-markierten Bereiche folgen mit den weiteren Modulen.
// ============================================================================
#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"
#include "storage/StorageService.h"
#include "modules/nfc/NfcModule.h"
#include "modules/jammer/Jammer.h"

// --- Erstes implementiertes Modul ---
// #include "modules/display/DisplayModule.h"
// #include "modules/input/InputModule.h"
// #include "modules/ir/IrModule.h"
// #include "modules/jammer/JammerModule.h"
// #include "core/ModeManager.h"
// #include "core/CommandRouter.h"
// #include "core/MenuController.h"
// #include "network/NetworkManager.h"
// #include "console/SerialConsole.h"   // stellt das globale Objekt "Console" bereit

// --- Globale Objekte (ein Exemplar pro Modul) ---
// DisplayModule display;
// InputModule   input;
NfcModule nfc;
// IrModule      ir;
JammerModule jammer;
// ModeManager   mode;          // kennt alle Module, fuehrt Automatik aus
// CommandRouter router;        // EIN Weg fuer alle Befehle (Menue, Handy, Serial)
// MenuController menu;         // Menue-Logik; Ausgabe auf Display UND Serial
// NetworkManager net;          // WLAN-AP + Webserver + WebSocket

// Alle Module in einer Liste, damit loop() sie einheitlich durchlaufen kann.
// Module* modules[] = { &display, &input, &nfc, &ir, &jammer };

// --- TEST: On-Board-LED blinken lassen ------------------------------------
// Zeigt, dass Ordnerstruktur, ESP-IDF-Build und Arduino-Komponente
// funktionieren. Nicht blockierend (millis statt delay), damit es spaeter
// neben den Modulen weiterlaufen kann. Entfernen, sobald echte Module laufen.
static unsigned long lastBlink = 0;
static bool ledOn = false;

static void blinkUpdate() {
  if (millis() - lastBlink < LED_BLINK_MS) return;
  lastBlink = millis();
  ledOn = !ledOn;
  digitalWrite(PIN_LED_BUILTIN, ledOn ? HIGH : LOW);
  Serial.println(ledOn ? "[test] LED an" : "[test] LED aus");
}
// --------------------------------------------------------------------------

// --- Vorlaeufige Serial-Befehle (bis die echte Konsole steht) --------------
// Im Serial Monitor einen Befehl + Enter tippen: read | emulate | stop | clear
// Alles landet in nfc.handleCommand() - genau dem Weg, den spaeter auch Menue,
// Handy und die richtige Konsole nutzen. Nicht blockierend.
static void pollSerialCommands() {
  static String line;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      line.trim();
      if (line.length() > 0) {
        JsonDocument params;
        String module = line;
        String action = "";
        const int splitPos = line.indexOf(' ');
        if (splitPos >= 0) {
          module = line.substring(0, splitPos);
          action = line.substring(splitPos + 1);
        }

        bool ok = false;
        if (module == "nfc") {
          ok = nfc.handleCommand(action, params.to<JsonObject>());
        } else if (module == "jammer") {
          ok = jammer.handleCommand(action, params.to<JsonObject>());
        } else if (module == "status") {
          JsonDocument statusDoc;
          JsonObject nfcStatus = statusDoc["nfc"].to<JsonObject>();
          JsonObject jammerStatus = statusDoc["jammer"].to<JsonObject>();
          nfc.statusJson(nfcStatus);
          jammer.statusJson(jammerStatus);
          serializeJsonPretty(statusDoc, Serial);
          Serial.println();
          ok = true;
        } else {
          ok = nfc.handleCommand(line, params.to<JsonObject>());
        }

        String display = ok ? "[cmd] angenommen: " : "[cmd] unbekannt/abgelehnt: ";
        Serial.print(display);
        Serial.println(line);
      }
      line = "";
    } else if (line.length() < 32) {
      line += c;
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n[OrcaOne] Start...");

  pinMode(PIN_LED_BUILTIN, OUTPUT);   // TEST: On-Board-LED

  // SPI ZUERST durch den Jammer (NRF24) initialisieren! SPIClass::begin() im
  // Arduino-Core kehrt vorzeitig zurueck (if(_spi) return true;), sobald der Bus
  // einmal laeuft - der ERSTE Aufrufer legt die Pins fest. Lief Storage.begin()
  // (SD.begin -> spi.begin() mit Default-Pins) zuerst, wurde die explizite
  // NRF24-Pinwahl ignoriert und der Chip antwortete nicht (isChipConnected()=0).
  jammer.begin();

  Storage.begin();  // Speicher-Dienst (SD/RAM). Teilt den SPI-Bus mit dem NRF24;
                    // erbt jetzt die vom Jammer gesetzte Buskonfiguration (eigener CS).

  nfc.begin();  // Vorlaeufiger NFC-Grundtest (I2C, unabhaengig vom SPI-Bus).

  // TODO: uebrige Module initialisieren und danach auch NFC ueber die Liste starten.
  // for (Module* m : modules) m->begin();

  // TODO: Befehls-Verteiler kennt alle Module + ModeManager.
  // router.begin(modules, sizeof(modules)/sizeof(modules[0]), &mode);

  // TODO: Menue, Netzwerk und Serial-Konsole nutzen alle denselben Router.
  // menu.begin(&router, &display);
  // net.begin(&router);
  // Console.begin(&router, &menu);

  Serial.println("[OrcaOne] Bereit.");
}

void loop() {
  //blinkUpdate();   // TEST: On-Board-LED

  pollSerialCommands();  // Vorlaeufig: read/emulate/stop/clear ueber Serial Monitor.
  nfc.update();  // Vorlaeufiger NFC-Grundtest, bis die gemeinsame Modul-Liste steht.
  jammer.update();

  // TODO: uebrigen Modulen Rechenzeit geben und danach auch NFC ueber die Liste updaten.
  // for (Module* m : modules) m->update();

  // TODO: Automatik-Regeln auswerten (nur aktiv, wenn Modus == auto)
  // mode.update();

  // TODO: Drehgeber-Ereignisse ans Menue geben (die Konsole tut dasselbe
  //       mit up/down/ok/back - beide Wege landen in menu.handleInput()).
  // InputEvent e = input.poll();
  // if (e != InputEvent::None) menu.handleInput(e);

  // TODO: Netzwerk bedienen + regelmaessig Status ans Handy schicken
  // net.update();

  // TODO: Eingaben aus dem Serial Monitor verarbeiten (nicht blockierend)
  // Console.update();

  delay(1);  // Gibt dem Idle-Task Zeit und verhindert Watchdog-Ausloesungen.
}

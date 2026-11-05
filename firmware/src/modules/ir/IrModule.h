#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include <utility>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>
#include "../../core/Module.h"
#include "../../config.h"  // IR_RAM_SLOT_COUNT u. a. (Member-Initialisierer unten)

// ============================================================================
//  IrModule - Infrarot als Stoerwerkzeug ("IR-Jammer")
// ----------------------------------------------------------------------------
//  Zwei Betriebsarten, beide laufen bis 'stop':
//   A) Aufnehmen/Senden: Signale eines Fensters mitschneiden (record -> stop),
//      dann zufaellig in Dauerschleife zuruueckspielen (send).
//   B) Ausschalt-Sequenz: kuratierte Off-Code-Liste von SD in Dauerschleife
//      senden (shutoff).
//
//  Zustandsautomat ueber millis(), kein delay() (ARBEITSWEISE.md §6).
//  Speicherung ueber das globale Storage, Namensraum "ir" (Ringpuffer-Slots).
//  Design: docs/superpowers/specs/2026-11-05-ir-modul-serial-bedienung-design.md
// ============================================================================
class IrModule : public Module {
public:
  const char* name() const override { return "ir"; }
  void begin() override;
  void update() override;
  void statusJson(JsonObject obj) override;
  bool handleCommand(const String& action, JsonObjectConst params) override;

private:
  enum class State { Idle, Recording, Sending, Shutoff };

  // Ein einzelnes IR-Signal als rohe Timing-Folge (Mikrosekunden) + Traegerfreq.
  struct Signal {
    uint16_t khz = 38;
    std::vector<uint16_t> raw;
  };

  // --- Slot-/Ringpuffer-Verwaltung ueber Storage ---
  void loadMeta();
  void saveMeta();
  bool loadSlot(int slotNr, std::vector<Signal>& out);  // true = vorhanden+nichtleer
  void storeCapture();                                  // _capture -> neuer Slot
  static void signalsToJson(const std::vector<Signal>& sigs, JsonArray arr);
  static bool jsonToSignals(JsonArrayConst arr, std::vector<Signal>& out);
  void refreshHasShutoff();

  IRrecv* _recv = nullptr;
  IRsend* _send = nullptr;
  decode_results _results;

  State _state = State::Idle;
  int _slotCount = IR_RAM_SLOT_COUNT;  // aktive Ringpuffer-Groesse N (backend-abh.)
  int _metaNext = 1;                   // naechster zu beschreibender Slot (1..N)
  int _metaNewest = 0;                 // zuletzt beschriebener Slot (0 = keiner)
  int _metaCount = 0;                  // belegte Slots (0..N)
  bool _hasShutoff = false;            // Ausschaltliste auf SD vorhanden?

  std::vector<Signal> _capture;        // laufende Aufnahme
  std::vector<Signal> _playList;       // Signale fuer Sending/Shutoff
  size_t _playIdx = 0;                 // Reihenfolge im Shutoff-Modus
  unsigned long _lastSendMs = 0;       // millis-Takt fuers Senden
};

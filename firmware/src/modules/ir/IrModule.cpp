#include "IrModule.h"
#include "../../config.h"
#include "../../storage/StorageService.h"

// ---------------------------------------------------------------------------
//  Lebenszyklus
// ---------------------------------------------------------------------------
void IrModule::begin() {
  // Ringpuffer-Groesse haengt vom Speicher-Backend ab (Heap-Schutz ohne SD).
  _slotCount = Storage.available() ? IR_SD_SLOT_COUNT : IR_RAM_SLOT_COUNT;
  loadMeta();
  refreshHasShutoff();

  // IR-Empfaenger: save_buffer=true erlaubt Dekodieren waehrend der naechste
  // Mitschnitt schon laeuft. Kurze Stoersignale (GPIO34 ohne Empfaenger)
  // werden ueber den Unknown-Threshold ausgefiltert.
  _recv = new IRrecv(PIN_IR_RX, IR_CAPTURE_BUFFER, 15 /*ms Nachlauf*/, true);
  _recv->setUnknownThreshold(IR_RECORD_UNKNOWN_THRESHOLD);
  _recv->enableIRIn();

  _send = new IRsend(PIN_IR_TX);
  _send->begin();

  randomSeed(micros());  // fuer das zufaellige Senden
  Serial.printf("[ir] bereit (Slots: %d, belegt: %d)\n", _slotCount, _metaCount);
}

// ---------------------------------------------------------------------------
//  Hauptschleife (nicht blockierend)
// ---------------------------------------------------------------------------
void IrModule::update() {
  switch (_state) {
    case State::Recording: {
      if (_recv->decode(&_results)) {
        uint16_t len = getCorrectedRawLength(&_results);
        uint16_t* raw = resultToRawArray(&_results);
        if (raw != nullptr) {
          Signal sig;
          sig.khz = 38;  // IR-Standard; decode liefert keine Traegerfrequenz
          sig.raw.assign(raw, raw + len);
          delete[] raw;  // resultToRawArray gibt Heap-Speicher zurueck
          if (!sig.raw.empty()) {
            _capture.push_back(std::move(sig));
            Serial.printf("[ir] Signal %u aufgenommen (%u Eintraege)\n",
                          (unsigned)_capture.size(), (unsigned)len);
          }
        }
        _recv->resume();

        if ((int)_capture.size() >= IR_MAX_SIGNALS_PER_SLOT) {
          Serial.println("[ir] Fenster voll, Aufnahme beendet");
          storeCapture();
          _state = State::Idle;
        }
      }
      break;
    }

    case State::Sending: {
      if (_playList.empty()) { _state = State::Idle; break; }
      unsigned long now = millis();
      if (now - _lastSendMs >= IR_SEND_GAP_MS) {
        _lastSendMs = now;
        const Signal& s = _playList[random(_playList.size())];
        _send->sendRaw(s.raw.data(), s.raw.size(), s.khz);
      }
      break;
    }

    case State::Shutoff: {
      if (_playList.empty()) { _state = State::Idle; break; }
      unsigned long now = millis();
      if (now - _lastSendMs >= IR_SEND_GAP_MS) {
        _lastSendMs = now;
        const size_t n = _playList.size();
        // Letzter Chunk kann kuerzer sein als IR_SHUTOFF_CHUNK.
        size_t chunkLen = min((size_t)IR_SHUTOFF_CHUNK, n - _chunkStart);
        // Keinen Rest-Chunk der Groesse 1 entstehen lassen: sonst laegen dessen
        // zwei Druecke nur IR_SEND_GAP_MS auseinander (kein echter Abstand).
        // Deterministisch aus _chunkStart -> jeder Tick berechnet denselben Wert.
        if (n - _chunkStart - chunkLen == 1) chunkLen = n - _chunkStart;
        const Signal& s = _playList[_chunkStart + _chunkPos];
        _send->sendRaw(s.raw.data(), s.raw.size(), s.khz);

        if (++_chunkPos >= chunkLen) {          // Chunk einmal komplett durch
          _chunkPos = 0;
          if (++_chunkRep >= IR_SHUTOFF_CHUNK_REPEATS) {  // oft genug wiederholt
            _chunkRep = 0;
            _chunkStart += chunkLen;            // naechster Chunk
            if (_chunkStart >= n) _chunkStart = 0;  // Dauerschleife: zurueck an den Anfang
          }
        }
      }
      break;
    }

    case State::Idle:
    default:
      // Kein decode() ausserhalb der Aufnahme -> GPIO34-Rauschen wird ignoriert.
      break;
  }
}

// ---------------------------------------------------------------------------
//  Status
// ---------------------------------------------------------------------------
void IrModule::statusJson(JsonObject obj) {
  obj["ok"] = true;  // IR-Bauteile sind nicht erkennbar -> immer true
  obj["hardwareDetectable"] = false;
  const char* st = "idle";
  switch (_state) {
    case State::Recording: st = "recording"; break;
    case State::Sending:   st = "sending";   break;
    case State::Shutoff:   st = "shutoff";    break;
    default:               st = "idle";       break;
  }
  obj["state"] = st;
  obj["slots"] = _metaCount;
  obj["newest"] = _metaNewest;
  obj["hasShutoff"] = _hasShutoff;
}

// ---------------------------------------------------------------------------
//  Befehle
// ---------------------------------------------------------------------------
bool IrModule::handleCommand(const String& action, JsonObjectConst params) {
  if (action == "record") {
    _capture.clear();
    _recv->resume();  // Empfaenger scharf
    _state = State::Recording;
    Serial.println("[ir] Aufnahme laeuft - Signale senden, dann 'ir stop'");
    return true;
  }

  if (action == "stop") {
    if (_state == State::Recording) {
      if (_capture.empty()) Serial.println("[ir] nichts aufgenommen");
      else storeCapture();
    }
    _state = State::Idle;
    _playList.clear();
    Serial.println("[ir] gestoppt");
    return true;
  }

  if (action == "send") {
    if (_metaCount == 0) { Serial.println("[ir] nichts gespeichert"); return false; }
    int slotNr = params["slot"] | _metaNewest;  // ohne slot = neuester
    std::vector<Signal> sigs;
    if (!loadSlot(slotNr, sigs)) {
      Serial.printf("[ir] slot%d leer/unbekannt\n", slotNr);
      return false;
    }
    _playList = std::move(sigs);
    _state = State::Sending;
    _lastSendMs = 0;
    Serial.printf("[ir] sende slot%d (%u Signale, zufaellig) - 'ir stop' beendet\n",
                  slotNr, (unsigned)_playList.size());
    return true;
  }

  if (action == "shutoff") {
    String s = Storage.load("ir", "shutoff");
    if (s.length() == 0) { Serial.println("[ir] keine Ausschaltliste (SD?)"); return false; }
    JsonDocument d;
    if (deserializeJson(d, s)) { Serial.println("[ir] Ausschaltliste unlesbar"); return false; }
    // Format wie Slots ("signals"); "codes" wird als Alias akzeptiert.
    JsonArrayConst arr = d["signals"].as<JsonArrayConst>();
    if (arr.isNull()) arr = d["codes"].as<JsonArrayConst>();
    std::vector<Signal> sigs;
    if (!jsonToSignals(arr, sigs)) { Serial.println("[ir] Ausschaltliste leer/unlesbar"); return false; }
    _playList = std::move(sigs);
    _chunkStart = 0;
    _chunkPos = 0;
    _chunkRep = 0;
    _state = State::Shutoff;
    _lastSendMs = 0;
    _hasShutoff = true;
    Serial.printf("[ir] Ausschalt-Sequenz (%u Codes, Dauerschleife) - 'ir stop' beendet\n",
                  (unsigned)_playList.size());
    return true;
  }

  return false;  // unbekannte Aktion
}

// ---------------------------------------------------------------------------
//  Slot-/Ringpuffer-Verwaltung
// ---------------------------------------------------------------------------
void IrModule::loadMeta() {
  _metaNext = 1; _metaNewest = 0; _metaCount = 0;
  String s = Storage.load("ir", "meta");
  if (s.length() > 0) {
    JsonDocument d;
    if (!deserializeJson(d, s)) {
      _metaNext   = d["next"]   | 1;
      _metaNewest = d["newest"] | 0;
      _metaCount  = d["count"]  | 0;
    }
  }
  // Clampen, falls N zwischen zwei Starts geschrumpft ist (z. B. SD entfernt).
  if (_metaNext < 1 || _metaNext > _slotCount) _metaNext = 1;
  if (_metaNewest > _slotCount) _metaNewest = _slotCount;
  if (_metaCount > _slotCount) _metaCount = _slotCount;
}

void IrModule::saveMeta() {
  JsonDocument d;
  d["next"] = _metaNext;
  d["newest"] = _metaNewest;
  d["count"] = _metaCount;
  String s;
  serializeJson(d, s);
  Storage.save("ir", "meta", s);
}

void IrModule::storeCapture() {
  if (_capture.empty()) return;
  int slotNr = _metaNext;

  JsonDocument d;
  d["v"] = 1;
  JsonArray arr = d["signals"].to<JsonArray>();
  signalsToJson(_capture, arr);
  String s;
  serializeJson(d, s);

  bool ok = Storage.save("ir", String("slot") + slotNr, s);
  if (ok) {
    _metaNewest = slotNr;
    _metaCount = min(_metaCount + 1, _slotCount);
    _metaNext = (slotNr % _slotCount) + 1;  // Ringpuffer
    saveMeta();
    Serial.printf("[ir] aufgenommen: %u Signale in slot%d%s\n",
                  (unsigned)_capture.size(), slotNr,
                  Storage.available() ? "" : " (fluechtig, kein SD)");
  } else {
    Serial.println("[ir] Speichern fehlgeschlagen");
  }
  _capture.clear();
}

bool IrModule::loadSlot(int slotNr, std::vector<Signal>& out) {
  if (slotNr < 1 || slotNr > _slotCount) return false;
  String s = Storage.load("ir", String("slot") + slotNr);
  if (s.length() == 0) return false;
  JsonDocument d;
  if (deserializeJson(d, s)) return false;
  return jsonToSignals(d["signals"].as<JsonArrayConst>(), out);
}

void IrModule::signalsToJson(const std::vector<Signal>& sigs, JsonArray arr) {
  for (const Signal& sg : sigs) {
    JsonObject o = arr.add<JsonObject>();
    o["khz"] = sg.khz;
    JsonArray r = o["raw"].to<JsonArray>();
    for (uint16_t v : sg.raw) r.add(v);
  }
}

bool IrModule::jsonToSignals(JsonArrayConst arr, std::vector<Signal>& out) {
  out.clear();
  if (arr.isNull()) return false;
  for (JsonObjectConst o : arr) {
    Signal sg;
    sg.khz = o["khz"] | 38;
    for (JsonVariantConst v : o["raw"].as<JsonArrayConst>()) {
      sg.raw.push_back(v.as<uint16_t>());
    }
    if (!sg.raw.empty()) out.push_back(std::move(sg));
  }
  return !out.empty();
}

void IrModule::refreshHasShutoff() {
  _hasShutoff = Storage.load("ir", "shutoff").length() > 0;
}

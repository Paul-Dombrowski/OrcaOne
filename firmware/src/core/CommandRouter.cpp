#include "CommandRouter.h"

void CommandRouter::begin(Module** modules, size_t count) {
  _modules = modules;
  _count = count;
}

bool CommandRouter::dispatch(const String& module, const String& action,
                             JsonObjectConst params, String* errorMessage) {
  // "system" (ModeManager) folgt spaeter - jetzt bewusst abgelehnt.
  if (module == "system") {
    if (errorMessage != nullptr) *errorMessage = "System-Modul nicht eingebunden";
    return false;
  }

  for (size_t i = 0; i < _count; i++) {
    if (module == _modules[i]->name()) {
      if (_modules[i]->handleCommand(action, params)) return true;

      if (errorMessage != nullptr) {
        JsonDocument statusDoc;
        JsonObject status = statusDoc.to<JsonObject>();
        _modules[i]->statusJson(status);
        const char* reason = status["error"] | "";
        if (reason[0] != '\0') {
          *errorMessage = reason;
        } else if (!(status["ok"] | true)) {
          *errorMessage = "Sensor/Aktor nicht bereit";
        } else {
          *errorMessage = "Befehl nicht angenommen";
        }
      }
      return false;
    }
  }
  if (errorMessage != nullptr) *errorMessage = "Modul nicht eingebunden";
  return false;  // unbekanntes Modul
}

void CommandRouter::buildStatus(JsonObject root) {
  root["mode"] = "manual";  // ohne ModeManager vorerst fest
  JsonObject mods = root["modules"].to<JsonObject>();
  for (size_t i = 0; i < _count; i++) {
    JsonObject m = mods[_modules[i]->name()].to<JsonObject>();
    _modules[i]->statusJson(m);
  }
}

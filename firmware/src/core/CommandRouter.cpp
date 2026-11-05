#include "CommandRouter.h"

void CommandRouter::begin(Module** modules, size_t count) {
  _modules = modules;
  _count = count;
}

bool CommandRouter::dispatch(const String& module, const String& action,
                             JsonObjectConst params) {
  // "system" (ModeManager) folgt spaeter - jetzt bewusst abgelehnt.
  if (module == "system") return false;

  for (size_t i = 0; i < _count; i++) {
    if (module == _modules[i]->name()) {
      return _modules[i]->handleCommand(action, params);
    }
  }
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

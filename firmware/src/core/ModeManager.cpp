#include "ModeManager.h"

#include "CommandRouter.h"

void ModeManager::begin(CommandRouter* router) {
  _router = router;
}

bool ModeManager::handleSystem(const String& action, JsonObjectConst params,
                               String* errorMessage) {
  if (action != "setMode") {
    if (errorMessage != nullptr) *errorMessage = "Unbekannter system-Befehl";
    return false;
  }

  const char* mode = params["mode"] | "";
  if (strcmp(mode, "nomercy") == 0 || strcmp(mode, "no-mercy") == 0 ||
      strcmp(mode, "auto") == 0) {
    setNoMercy(true);
    return true;
  }
  if (strcmp(mode, "off") == 0 || strcmp(mode, "manual") == 0) {
    setNoMercy(false);
    return true;
  }

  if (errorMessage != nullptr) *errorMessage = "Unbekannter Modus";
  return false;
}

void ModeManager::setNoMercy(bool on) {
  if (on == _noMercy) return;  // schon im Zielzustand -> nichts doppelt feuern
  _noMercy = on;

  if (_router == nullptr) {
    Serial.println("[mode] Kein Router - Modus kann nicht schalten.");
    return;
  }

  // Leeres params-Objekt fuer die Aktor-Befehle.
  JsonDocument doc;
  JsonObject params = doc.to<JsonObject>();

  if (on) {
    _router->dispatch("jammer", "start", params);
    _router->dispatch("ir", "shutoff", params);
    Serial.println("[mode] No-Mercy EIN: Jammer + IR-Shutoff gestartet.");
  } else {
    _router->dispatch("jammer", "stop", params);
    _router->dispatch("ir", "stop", params);
    Serial.println("[mode] No-Mercy AUS: Jammer + IR gestoppt.");
  }
}

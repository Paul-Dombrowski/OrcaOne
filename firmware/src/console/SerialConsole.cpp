#include "SerialConsole.h"

// Globales Objekt (genau hier definiert, anderswo via extern bekannt).
SerialConsole Console;

static const size_t LINE_MAX = 128;  // laengere Zeilen werden verworfen

void SerialConsole::begin(CommandRouter* router) {
  _router = router;
  _buf = "";
  _overflow = false;
}

void SerialConsole::update() {
  // Nur die bereits vorhandenen Zeichen holen - kein Warten (nicht blockierend).
  while (Serial.available()) {
    char c = (char)Serial.read();
    Serial.write(c);  // Echo: macht die Eingabe im Monitor sichtbar

    if (c == '\n' || c == '\r') {
      if (_overflow) {
        Serial.println("[console] Zeile zu lang - verworfen");
        _overflow = false;
        _buf = "";
        continue;
      }
      String line = _buf;
      _buf = "";
      line.trim();
      if (line.length() > 0) handleLine(line);
      continue;  // CRLF erzeugt eine leere Folgezeile - die wird hier ignoriert
    }

    if (_buf.length() >= LINE_MAX) {
      _overflow = true;  // Rest bis zum Zeilenende verwerfen
      continue;
    }
    _buf += c;
  }
}

void SerialConsole::handleLine(const String& line) {
  if (line == "help") { printHelp(); return; }
  if (line == "status") { printStatus(); return; }
  if (line.charAt(0) == '{') { dispatchJson(line); return; }
  dispatchText(line);
}

void SerialConsole::dispatchText(const String& line) {
  String module, action;
  JsonDocument doc;
  JsonObject params = doc.to<JsonObject>();

  int start = 0;
  int tokenIdx = 0;
  while (start < (int)line.length()) {
    int sp = line.indexOf(' ', start);
    String tok = (sp < 0) ? line.substring(start) : line.substring(start, sp);
    tok.trim();
    if (tok.length() > 0) {
      if (tokenIdx == 0) {
        module = tok;
      } else if (tokenIdx == 1) {
        action = tok;
      } else {
        int eq = tok.indexOf('=');
        if (eq > 0) {
          String key = tok.substring(0, eq);
          String val = tok.substring(eq + 1);
          // Nur-Ziffern -> Zahl, sonst Text (wie Web-API-JSON).
          bool numeric = val.length() > 0;
          for (size_t i = 0; i < val.length(); i++) {
            if (!isDigit(val[i])) { numeric = false; break; }
          }
          if (numeric) params[key] = val.toInt();
          else params[key] = val;
        }
      }
      tokenIdx++;
    }
    if (sp < 0) break;
    start = sp + 1;
  }

  if (module.length() == 0) return;
  if (action.length() == 0) {
    Serial.printf("[console] Aktion fehlt fuer Modul \"%s\" - \"help\" eingeben\n",
                  module.c_str());
    return;
  }
  bool ok = _router->dispatch(module, action, params);
  if (ok) {
    Serial.printf("[%s] %s: OK\n", module.c_str(), action.c_str());
  } else {
    Serial.printf("[%s] %s: abgelehnt (unbekanntes Modul/Aktion oder Modul inaktiv)\n",
                  module.c_str(), action.c_str());
  }
}

void SerialConsole::dispatchJson(const String& line) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, line);
  if (err) {
    Serial.printf("[console] JSON-Fehler: %s\n", err.c_str());
    return;
  }
  String module = doc["module"] | "";
  String action = doc["action"] | "";
  if (module.length() == 0 || action.length() == 0) {
    Serial.println("[console] JSON ohne \"module\"/\"action\"");
    return;
  }
  JsonObjectConst params = doc["params"].as<JsonObjectConst>();
  bool ok = _router->dispatch(module, action, params);
  Serial.printf("[%s] %s: %s\n", module.c_str(), action.c_str(),
                ok ? "OK" : "abgelehnt");
}

void SerialConsole::printHelp() {
  Serial.println("[console] Syntax: <modul> <aktion> [schluessel=wert ...]");
  Serial.print("[console] Module:");
  for (size_t i = 0; i < _router->count(); i++) {
    Serial.printf(" %s", _router->nameAt(i));
  }
  Serial.println();
  Serial.println("[console] Eingebaut: help, status");
  Serial.println("[console] Befehlsliste je Modul: docs/kommunikation/web-api.md");
}

void SerialConsole::printStatus() {
  JsonDocument doc;
  _router->buildStatus(doc.to<JsonObject>());
  serializeJson(doc, Serial);
  Serial.println();
}

#include "RamBackend.h"
#include <ArduinoJson.h>

int RamBackend::findIndex(const String& ns, const String& key) const {
  for (size_t i = 0; i < Cap; ++i) {
    if (_entries[i].used && _entries[i].ns == ns && _entries[i].key == key) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

bool RamBackend::save(const String& ns, const String& key, const String& data) {
  // Vorhandenen Datensatz ueberschreiben.
  const int idx = findIndex(ns, key);
  if (idx >= 0) {
    _entries[idx].data = data;
    return true;
  }
  // Sonst ersten freien Slot belegen.
  for (size_t i = 0; i < Cap; ++i) {
    if (!_entries[i].used) {
      _entries[i].used = true;
      _entries[i].ns   = ns;
      _entries[i].key  = key;
      _entries[i].data = data;
      return true;
    }
  }
  Serial.println("[storage] RAM voll - Datensatz nicht gespeichert");
  return false;
}

String RamBackend::load(const String& ns, const String& key) {
  const int idx = findIndex(ns, key);
  return idx >= 0 ? _entries[idx].data : String();
}

String RamBackend::list(const String& ns) {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (size_t i = 0; i < Cap; ++i) {
    if (_entries[i].used && _entries[i].ns == ns) {
      arr.add(_entries[i].key);
    }
  }
  String out;
  serializeJson(doc, out);  // leeres Array -> "[]"
  return out;
}

bool RamBackend::remove(const String& ns, const String& key) {
  const int idx = findIndex(ns, key);
  if (idx < 0) return false;
  _entries[idx].used = false;
  _entries[idx].ns   = String();
  _entries[idx].key  = String();
  _entries[idx].data = String();
  return true;
}

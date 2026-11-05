#include "SdBackend.h"

#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <ArduinoJson.h>

bool SdBackend::begin(uint8_t csPin) {
  // Nutzt den Standard-SPI-Bus (VSPI: SCK 18 / MISO 19 / MOSI 23) - identisch
  // mit den NRF24-Pins, der Bus wird geteilt. Eigener CS (csPin) trennt die
  // Geraete. SD.begin gibt false zurueck, wenn keine Karte steckt -> dann
  // uebernimmt der Orchestrator das RAM-Backend (nie blockieren, §6.5).
  return SD.begin(csPin);
}

String SdBackend::dirFor(const String& ns) {
  return "/" + ns;
}

String SdBackend::pathFor(const String& ns, const String& key) {
  return "/" + ns + "/" + key + ".json";
}

bool SdBackend::save(const String& ns, const String& key, const String& data) {
  SD.mkdir(dirFor(ns));  // idempotent; false (existiert bereits) ist kein Fehler

  const String path = pathFor(ns, key);
  // Sauberes Ueberschreiben: FILE_WRITE haengt je nach Core an - daher vorher
  // entfernen, damit immer der volle, aktuelle Datensatz in der Datei steht.
  if (SD.exists(path)) SD.remove(path);

  File f = SD.open(path, FILE_WRITE);
  if (!f) {
    Serial.println("[storage] SD: Datei konnte nicht geschrieben werden");
    return false;
  }
  const size_t written = f.print(data);
  f.close();

  if (written != data.length()) {
    Serial.println("[storage] SD: unvollstaendig geschrieben (Karte voll?)");
    return false;
  }
  return true;
}

String SdBackend::load(const String& ns, const String& key) {
  const String path = pathFor(ns, key);
  if (!SD.exists(path)) return String();

  File f = SD.open(path, FILE_READ);
  if (!f) return String();
  String data = f.readString();
  f.close();
  return data;
}

String SdBackend::list(const String& ns) {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();

  File dir = SD.open(dirFor(ns));
  if (dir && dir.isDirectory()) {
    for (File entry = dir.openNextFile(); entry; entry = dir.openNextFile()) {
      if (entry.isDirectory()) continue;

      // name() liefert je nach Core den vollen Pfad oder nur den Dateinamen -
      // beides robust auf den Schluessel (ohne .json) reduzieren.
      String n = entry.name();
      const int slash = n.lastIndexOf('/');
      if (slash >= 0) n = n.substring(slash + 1);
      if (n.endsWith(".json")) n = n.substring(0, n.length() - 5);
      if (n.length() > 0) arr.add(n);
    }
  }
  if (dir) dir.close();

  String out;
  serializeJson(doc, out);  // leer / Ordner fehlt -> "[]"
  return out;
}

bool SdBackend::remove(const String& ns, const String& key) {
  const String path = pathFor(ns, key);
  if (!SD.exists(path)) return false;
  return SD.remove(path);
}

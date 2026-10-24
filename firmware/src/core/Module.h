#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// ============================================================================
//  Modul-Vertrag ("Interface")
// ----------------------------------------------------------------------------
//  JEDE Funktion des Geräts (NFC, IR, Jammer, Display, Input) wird als eigenes
//  Modul umgesetzt, das von dieser Klasse erbt. Dadurch behandeln core/ und
//  network/ alle Module gleich: sie rufen nur begin(), update(), statusJson()
//  und handleCommand() auf und müssen die Innereien nicht kennen.
//
//  WICHTIG für alle (auch KI): Diese vier Methoden sind der verbindliche
//  Vertrag. Ein neues Modul MUSS genau diese Methoden umsetzen - nicht mehr
//  und nicht weniger an Pflicht-Schnittstelle.
// ============================================================================
class Module {
public:
  virtual ~Module() {}

  // Eindeutiger, kurzer Name des Moduls, z. B. "nfc", "ir", "jammer".
  // Wird als Schluessel in der Web-API und im Status-JSON verwendet.
  virtual const char* name() const = 0;

  // Einmalige Hardware-Initialisierung. Wird in setup() genau einmal aufgerufen.
  // Hier: Pins setzen, Bus (I2C/SPI) starten, Sensor-Objekt initialisieren.
  // Das Bauteil kann FEHLEN (nicht jede Person hat alle Bauteile). Dann:
  // einmal per Serial melden, Modul inaktiv lassen, sofort zurueckkehren.
  // NIE while(1); / ESP.restart() / Warten ohne Zeitlimit (ARBEITSWEISE.md §6).
  virtual void begin() = 0;

  // Wird in JEDEM Durchlauf von loop() aufgerufen (viele Male pro Sekunde).
  // MUSS nicht-blockierend sein: KEIN delay(), keine langen Schleifen.
  // Lange Aktionen in kleine Schritte zerlegen (Zustandsautomat). Begruendung:
  // ein delay() hier friert Menue, WLAN und alle anderen Module gleichzeitig ein.
  virtual void update() = 0;

  // Aktuellen Zustand des Moduls in das uebergebene JSON-Objekt schreiben.
  // Dieses JSON geht an das Handy (Weboberflaeche) und speist die Anzeige.
  // Beispiel NFC: obj["ok"] = true; obj["lastUid"] = "04A1B2C3";
  // Pflichtfeld "ok": false, wenn begin() einen Fehler gemeldet hat (Bauteil
  // fehlt), sonst true. Das Handy graut das Modul dann aus.
  virtual void statusJson(JsonObject obj) = 0;

  // Einen Befehl ausfuehren, der vom Handy, vom lokalen Menue ODER von der
  // Serial-Konsole kommt (alle drei landen hier - deshalb nur hier umsetzen).
  // Ist das Modul inaktiv (Bauteil fehlt): false zurueckgeben, nicht haengen.
  // 'action' = was getan werden soll (z. B. "read", "replay", "start").
  // 'params' = optionale Zusatzdaten (z. B. welcher Speicherplatz).
  // Rueckgabe: true, wenn der Befehl verstanden und angenommen wurde.
  virtual bool handleCommand(const String& action, JsonObjectConst params) = 0;
};

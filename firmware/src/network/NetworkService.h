#pragma once
#include <Arduino.h>

// ============================================================================
//  NetworkService - WLAN-Hotspot (Access Point) des OrcaOne
// ----------------------------------------------------------------------------
//  Gemeinsamer DIENST (wie Storage/Console), KEIN Modul: er steht nicht im
//  modules[]-Array und erbt nicht von core/Module.h. main.cpp verdrahtet ihn
//  wie Console - in setup() Net.begin(), in loop() Net.update().
//
//  STUFE 1a (jetzt umgesetzt): Nur den WLAN-Hotspot aufspannen, damit sich ein
//  Handy ueberhaupt mit dem ESP32 verbinden kann. SSID/Passwort stehen in
//  config.h (bzw. secrets.h). Die AP-IP wird einmal im Serial-Monitor gemeldet.
//
//  NOCH NICHT hier (Stufe 2, siehe network/README.md): Webserver, WebSocket,
//  Status-Push und Befehlsempfang ueber den CommandRouter, LittleFS-Auslieferung
//  der Weboberflaeche.
//
//  NAMENSWAHL: Klasse "NetworkService" / Objekt "Net" - bewusst NICHT
//  "NetworkManager"/"Network". arduino-esp32 (Core 3.x) bringt bereits eine
//  Kern-Klasse NetworkManager samt globalem Objekt "Network" mit; gleiche Namen
//  wuerden einen Compile-Konflikt ausloesen.
// ============================================================================
class NetworkService {
public:
  // Einmalige Initialisierung in setup(). Spannt den Access Point auf und gibt
  // die AP-IP im Serial-Monitor aus. Blockiert nicht und bricht nie ab (das
  // WLAN ist on-chip, kann also nicht "fehlen" wie ein externes Bauteil).
  void begin();

  // Wird in jedem loop()-Durchlauf aufgerufen. MUSS nicht-blockierend sein.
  // Stufe 1a: noch nichts zu tun (Platzhalter fuer den Webserver in Stufe 2).
  void update();

  // Ist der Access Point aktiv? (fuer spaetere Status-Ausgabe / Stufe 2)
  bool ready() const { return _ready; }

private:
  bool _ready = false;  // true, sobald softAP() erfolgreich war
};

// Global erreichbares Objekt (in genau einer .cpp definiert, hier nur bekannt
// gemacht). main.cpp und spaeter das network/-Stufe-2 nutzen dieses eine Net.
extern NetworkService Net;

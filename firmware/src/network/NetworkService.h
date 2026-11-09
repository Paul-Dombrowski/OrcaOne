#pragma once
#include <Arduino.h>

// ============================================================================
//  NetworkService - WLAN-Hotspot (Access Point) des OrcaOne
// ----------------------------------------------------------------------------
//  Gemeinsamer DIENST (wie Storage/Console), KEIN Modul: er steht nicht im
//  modules[]-Array und erbt nicht von core/Module.h. main.cpp verdrahtet ihn
//  wie Console - in setup() Net.begin(), in loop() Net.update().
//
//  STUFE 1a: WLAN-Hotspot aufspannen, damit sich ein Handy mit dem ESP32
//  verbinden kann. SSID/Passwort aus config.h (bzw. secrets.h), AP-IP im Serial.
//
//  STUFE 2a (jetzt umgesetzt): HTTP-Server, der die Weboberflaeche (firmware/web/
//  index.html + css + js) ausliefert. Die Dateien sind ins Firmware-Image
//  EINGEBETTET (EMBED_FILES in main/CMakeLists.txt) - KEIN LittleFS noetig.
//  Damit ist die Seite erreichbar, sobald das Handy im OrcaOne-WLAN ist und die
//  AP-IP im Browser oeffnet.
//
//  NOCH NICHT hier (Stufe 2b): WebSocket /ws fuer Live-Status-Push und
//  Befehlsempfang ueber den CommandRouter. Ohne /ws laedt die Seite, zeigt aber
//  "warte auf Verbindung" (app.js versucht /ws automatisch erneut).
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
  // Startet den HTTP-Server und registriert die Routen fuer die eingebetteten
  // Web-Dateien. Fehlschlag wird gemeldet, blockiert aber nicht (ARBEITSWEISE §6).
  void startHttpServer();

  bool _ready = false;        // true, sobald softAP() erfolgreich war
  void* _httpServer = nullptr; // httpd_handle_t (void*), nullptr = nicht gestartet
};

// Global erreichbares Objekt (in genau einer .cpp definiert, hier nur bekannt
// gemacht). main.cpp und spaeter das network/-Stufe-2 nutzen dieses eine Net.
extern NetworkService Net;

// ============================================================================
//  NetworkService.cpp - WLAN-Hotspot aufspannen (Stufe 1a)
// ----------------------------------------------------------------------------
//  Siehe NetworkService.h fuer die Rolle dieses Dienstes und die Stufen.
// ============================================================================
#include "NetworkService.h"
#include <WiFi.h>
#include "../config.h"

// Das eine, global erreichbare Objekt (Muster wie "Storage"/"Console").
NetworkService Net;

void NetworkService::begin() {
  // Reiner Access-Point-Betrieb: der ESP32 macht ein eigenes WLAN auf und
  // verbindet sich NICHT mit einem vorhandenen Netz (kein Internet noetig,
  // wie vom Projektauftrag verlangt).
  WiFi.mode(WIFI_AP);

  // Hotspot starten. Passwort kommt aus config.h/secrets.h. Hinweis: softAP
  // verlangt ein Passwort mit >= 8 Zeichen - sonst oeffnet es ein OFFENES Netz.
  const bool ok = WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD);

  if (!ok) {
    // Fehlschlag nur melden, NICHT blockieren (ARBEITSWEISE.md §6): der Rest
    // des Geraets laeuft normal weiter, auch wenn der AP nicht hochkam.
    Serial.println("[net] WLAN-Hotspot konnte NICHT gestartet werden.");
    _ready = false;
    return;
  }

  _ready = true;

  // AP-IP einmal ausgeben - diese Adresse oeffnet man spaeter im Handy-Browser.
  // In Stufe 1a gibt es dort noch keine Seite; es geht nur ums Verbinden.
  Serial.print("[net] WLAN-Hotspot aktiv. SSID: ");
  Serial.println(WIFI_AP_SSID);
  Serial.print("[net] Mit diesem WLAN verbinden, IP des Geraets: ");
  Serial.println(WiFi.softAPIP());
}

void NetworkService::update() {
  // Stufe 1a: nichts zu tun. Platzhalter fuer den Webserver/WebSocket (Stufe 2).
  // MUSS nicht-blockierend bleiben (kein delay(), keine langen Schleifen).
}

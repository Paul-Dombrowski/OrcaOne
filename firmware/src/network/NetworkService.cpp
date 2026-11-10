// ============================================================================
//  NetworkService.cpp - WLAN-Hotspot + HTTP-Server (Stufe 1a + 2a)
// ----------------------------------------------------------------------------
//  Siehe NetworkService.h fuer die Rolle dieses Dienstes und die Stufen.
//
//  Die Web-Dateien liegen NICHT auf einem Dateisystem, sondern sind ins
//  Firmware-Image eingebettet (EMBED_FILES in main/CMakeLists.txt). Der Linker
//  legt dafuer Symbole "_binary_<datei>_start"/"_binary_<datei>_end" an, auf
//  die wir unten per asm()-Alias zugreifen.
// ============================================================================
#include "NetworkService.h"

#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_http_server.h>

#include <cstring>

#include "../config.h"
#include "../core/CommandRouter.h"

// --- Eingebettete Web-Dateien (siehe main/CMakeLists.txt, EMBED_FILES) --------
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");
extern const uint8_t style_css_start[]  asm("_binary_style_css_start");
extern const uint8_t style_css_end[]    asm("_binary_style_css_end");
extern const uint8_t app_js_start[]     asm("_binary_app_js_start");
extern const uint8_t app_js_end[]       asm("_binary_app_js_end");

namespace {

// Router + Server-Handle fuer die WebSocket-Handler (laufen in eigenen httpd-
// Tasks und erreichen die privaten NetworkService-Felder nicht direkt). Werden
// in startHttpServer() gesetzt.
CommandRouter* g_router = nullptr;
httpd_handle_t g_server = nullptr;

// Obergrenze paralleler Clients fuer die Socket-Liste (httpd-Default ist 7).
constexpr size_t kMaxWsClients = 8;

// Auftrag fuer einen asynchronen Status-Push an EINEN Client. Wird per
// httpd_queue_work im httpd-Task ausgefuehrt, damit Senden NICHT gleichzeitig
// aus Loop- und httpd-Task auf denselben Socket laeuft (sonst zerfranste
// Frames). Der Auftrag besitzt eine eigene Kopie der Nutzdaten.
struct AsyncPush {
  httpd_handle_t server;
  int fd;
  String payload;
};

void pushWork(void* arg) {
  auto* job = static_cast<AsyncPush*>(arg);
  httpd_ws_frame_t frame;
  memset(&frame, 0, sizeof(frame));
  frame.type = HTTPD_WS_TYPE_TEXT;
  frame.payload =
      reinterpret_cast<uint8_t*>(const_cast<char*>(job->payload.c_str()));
  frame.len = job->payload.length();
  httpd_ws_send_frame_async(job->server, job->fd, &frame);
  delete job;
}

// Gesamtstatus als JSON fuer den Push ans Handy. app.js erwartet
// { "type":"status", "mode":..., "modules":{...} } (siehe web-api.md).
String buildStatusJson() {
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  root["type"] = "status";
  if (g_router != nullptr) g_router->buildStatus(root);
  String out;
  serializeJson(doc, out);
  return out;
}

// Beschreibt eine auszuliefernde Datei: Zeiger, Laenge, MIME-Typ.
struct EmbeddedAsset {
  const uint8_t* start;
  size_t length;
  const char* contentType;
};

// Statische Asset-Beschreibungen (als user_ctx an die Handler gehaengt).
EmbeddedAsset kIndexHtml{index_html_start,
                         static_cast<size_t>(index_html_end - index_html_start),
                         "text/html; charset=utf-8"};
EmbeddedAsset kStyleCss{style_css_start,
                        static_cast<size_t>(style_css_end - style_css_start),
                        "text/css; charset=utf-8"};
EmbeddedAsset kAppJs{app_js_start,
                     static_cast<size_t>(app_js_end - app_js_start),
                     "application/javascript; charset=utf-8"};

// Ein Handler fuer alle Dateien: schickt die eingebetteten Bytes 1:1.
esp_err_t serveAsset(httpd_req_t* req) {
  const auto* asset = static_cast<const EmbeddedAsset*>(req->user_ctx);
  httpd_resp_set_type(req, asset->contentType);
  return httpd_resp_send(req, reinterpret_cast<const char*>(asset->start),
                         asset->length);
}

// Eine JSON-Antwort ueber den aktuellen WebSocket zuruecksenden (Text-Frame).
void wsSendText(httpd_req_t* req, const String& payload) {
  httpd_ws_frame_t frame;
  memset(&frame, 0, sizeof(frame));
  frame.type = HTTPD_WS_TYPE_TEXT;
  frame.payload = reinterpret_cast<uint8_t*>(const_cast<char*>(payload.c_str()));
  frame.len = payload.length();
  httpd_ws_send_frame(req, &frame);
}

// WebSocket-Endpunkt /ws (Stufe 2b): nimmt Befehle vom Handy entgegen und
// reicht sie ueber den CommandRouter an die Module weiter - DERSELBE Weg wie
// Menue und Serial-Konsole. Nach dem Dispatch geht eine Quittung
// { "type":"event", "ok":bool, "message":... } zurueck. Der Live-Status wird
// nicht hier, sondern zyklisch in NetworkService::update() gepusht.
esp_err_t handleWebSocket(httpd_req_t* req) {
  if (req->method == HTTP_GET) {
    // Der Server hat den Upgrade bereits bestaetigt (HTTP 101). Direkt einen
    // ersten Status schicken, damit das Handy nicht 500 ms auf Daten wartet.
    Serial.println("[net] WebSocket /ws verbunden.");
    wsSendText(req, buildStatusJson());
    return ESP_OK;
  }

  // Laenge des eingehenden Frames ermitteln, dann einlesen.
  httpd_ws_frame_t frame;
  memset(&frame, 0, sizeof(frame));
  frame.type = HTTPD_WS_TYPE_TEXT;
  if (httpd_ws_recv_frame(req, &frame, 0) != ESP_OK) return ESP_FAIL;
  if (frame.len == 0 || frame.len >= 512) return ESP_OK;  // leer/zu gross

  uint8_t buffer[512];
  frame.payload = buffer;
  if (httpd_ws_recv_frame(req, &frame, sizeof(buffer) - 1) != ESP_OK) {
    return ESP_FAIL;
  }
  buffer[frame.len] = 0;  // null-terminieren fuer den JSON-Parser

  // Befehl parsen: { "module":..., "action":..., "params":{...} }.
  JsonDocument doc;
  if (deserializeJson(doc, reinterpret_cast<const char*>(buffer)) !=
      DeserializationError::Ok) {
    JsonDocument resp;
    resp["type"] = "event";
    resp["ok"] = false;
    resp["message"] = "Ungueltiger Befehl (JSON)";
    String out;
    serializeJson(resp, out);
    wsSendText(req, out);
    return ESP_OK;
  }

  const char* module = doc["module"] | "";
  const char* action = doc["action"] | "";
  Serial.printf("[net] /ws > %s %s\n", module, action);

  String errorMessage;
  bool ok = false;
  if (g_router != nullptr) {
    ok = g_router->dispatch(module, action, doc["params"].as<JsonObjectConst>(),
                            &errorMessage);
  } else {
    errorMessage = "Router nicht bereit";
  }

  // Quittung zuruecksenden; der aktualisierte Status folgt per zyklischem Push.
  JsonDocument resp;
  resp["type"] = "event";
  resp["ok"] = ok;
  resp["message"] =
      ok ? "Befehl ausgefuehrt"
         : (errorMessage.length() > 0 ? errorMessage : "Befehl abgelehnt");
  String out;
  serializeJson(resp, out);
  wsSendText(req, out);
  return ESP_OK;
}

}  // namespace

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

  Serial.print("[net] WLAN-Hotspot aktiv. SSID: ");
  Serial.println(WIFI_AP_SSID);
  Serial.print("[net] Mit diesem WLAN verbinden, Seite oeffnen unter: http://");
  Serial.println(WiFi.softAPIP());

  // Web-Oberflaeche ausliefern.
  startHttpServer();
}

void NetworkService::startHttpServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = WEBSERVER_PORT;

  httpd_handle_t server = nullptr;
  if (httpd_start(&server, &config) != ESP_OK) {
    Serial.println("[net] HTTP-Server konnte NICHT gestartet werden - Seite nicht erreichbar.");
    _httpServer = nullptr;
    return;
  }
  _httpServer = server;
  g_server = server;      // fuer den Status-Push aus update()
  g_router = _router;     // fuer Befehls-Dispatch im WebSocket-Handler

  // Routen: "/" und "/index.html" -> Seite; dazu CSS und JS (von index.html
  // relativ referenziert). GET genuegt; die Seite loest Aktionen ueber /ws aus.
  const httpd_uri_t routes[] = {
      {"/", HTTP_GET, serveAsset, &kIndexHtml},
      {"/index.html", HTTP_GET, serveAsset, &kIndexHtml},
      {"/css/style.css", HTTP_GET, serveAsset, &kStyleCss},
      {"/js/app.js", HTTP_GET, serveAsset, &kAppJs},
  };
  for (const httpd_uri_t& route : routes) {
    httpd_register_uri_handler(server, &route);
  }

  // WebSocket-Endpunkt /ws: Handshake + Befehlsempfang; Status-Push via update().
  httpd_uri_t wsRoute = {};
  wsRoute.uri = "/ws";
  wsRoute.method = HTTP_GET;
  wsRoute.handler = handleWebSocket;
  wsRoute.user_ctx = nullptr;
  wsRoute.is_websocket = true;
  httpd_register_uri_handler(server, &wsRoute);

  Serial.println("[net] HTTP-Server laeuft (Port 80). Web-Oberflaeche erreichbar.");
}

void NetworkService::update() {
  // Stufe 2b: etwa alle 500 ms den Live-Status an ALLE verbundenen WebSocket-
  // Clients schieben. Nicht-blockierend (ARBEITSWEISE.md §6): nur ein kurzer
  // JSON-Aufbau + asynchrones Senden, keine Wartezeiten.
  if (_httpServer == nullptr || _router == nullptr) return;
  if (millis() - _lastPush < 500) return;
  _lastPush = millis();

  httpd_handle_t server = static_cast<httpd_handle_t>(_httpServer);

  // Liste der offenen Sockets holen; daraus nur die echten WebSocket-Clients.
  size_t count = kMaxWsClients;
  int clientFds[kMaxWsClients];
  if (httpd_get_client_list(server, &count, clientFds) != ESP_OK) return;

  const String payload = buildStatusJson();

  for (size_t i = 0; i < count; ++i) {
    const int fd = clientFds[i];
    if (httpd_ws_get_fd_info(server, fd) != HTTPD_WS_CLIENT_WEBSOCKET) continue;
    // Senden im httpd-Task ausfuehren lassen (thread-sicher, siehe AsyncPush).
    auto* job = new AsyncPush{server, fd, payload};
    if (httpd_queue_work(server, pushWork, job) != ESP_OK) delete job;
  }
}

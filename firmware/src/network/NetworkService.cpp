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
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

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

// Router fuer den Befehls-Dispatch. WICHTIG: dispatch() und statusJson() der
// Module duerfen laut Module.h NUR aus dem loop()-Task laufen. Der /ws-Handler
// laeuft aber im httpd-Task; er dispatcht daher NICHT direkt, sondern legt den
// Befehl in g_cmdQueue. NetworkService::update() (loop-Task) arbeitet die Queue
// ab - so bleibt jeder Modulzugriff single-threaded (keine Races).
CommandRouter* g_router = nullptr;

// Obergrenze paralleler Clients fuer die Socket-Liste (httpd-Default ist 7).
constexpr size_t kMaxWsClients = 8;

// Eingehender Befehl vom Handy (rohes JSON), vom httpd-Task in die Queue gelegt
// und im loop()-Task verarbeitet. Fixe Groesse, damit kein Heap im httpd-Task.
struct WsCommand {
  char json[256];
};
QueueHandle_t g_cmdQueue = nullptr;

// Auftrag fuer einen asynchronen Frame-Versand an EINEN Client. Wird per
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

// Text-Frame an ALLE verbundenen WebSocket-Clients schicken (Status + Events).
// Aufruf aus dem loop()-Task; der eigentliche Versand laeuft per httpd_queue_work
// im httpd-Task (thread-sicher gegenueber parallelen Sends auf demselben Socket).
void broadcastToClients(httpd_handle_t server, const String& payload) {
  if (server == nullptr) return;
  size_t count = kMaxWsClients;
  int clientFds[kMaxWsClients];
  if (httpd_get_client_list(server, &count, clientFds) != ESP_OK) return;
  for (size_t i = 0; i < count; ++i) {
    const int fd = clientFds[i];
    if (httpd_ws_get_fd_info(server, fd) != HTTPD_WS_CLIENT_WEBSOCKET) continue;
    auto* job = new AsyncPush{server, fd, payload};
    if (httpd_queue_work(server, pushWork, job) != ESP_OK) delete job;
  }
}

// Gesamtstatus als JSON. NUR aus dem loop()-Task aufrufen (liest Modul-State via
// buildStatus -> statusJson). app.js erwartet
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

// WebSocket-Endpunkt /ws (Stufe 2b): nimmt Befehle vom Handy entgegen. Der
// Handler laeuft im httpd-Task und darf deshalb NICHT selbst dispatchen (Module
// gehoeren dem loop-Task). Er legt das rohe Befehls-JSON in g_cmdQueue; die
// Verarbeitung + Quittung passiert in NetworkService::update(). Der Live-Status
// wird ebenfalls dort zyklisch gepusht.
esp_err_t handleWebSocket(httpd_req_t* req) {
  if (req->method == HTTP_GET) {
    // Der Server hat den Upgrade bereits bestaetigt (HTTP 101). Status folgt mit
    // dem naechsten zyklischen Push (<= 500 ms) aus update().
    Serial.println("[net] WebSocket /ws verbunden.");
    return ESP_OK;
  }

  // Laenge des eingehenden Frames ermitteln, dann einlesen.
  httpd_ws_frame_t frame;
  memset(&frame, 0, sizeof(frame));
  frame.type = HTTPD_WS_TYPE_TEXT;
  if (httpd_ws_recv_frame(req, &frame, 0) != ESP_OK) return ESP_FAIL;
  if (frame.len == 0 || frame.len >= sizeof(WsCommand::json)) {
    return ESP_OK;  // leer oder zu gross fuer die Queue -> verwerfen
  }

  WsCommand cmd;
  frame.payload = reinterpret_cast<uint8_t*>(cmd.json);
  if (httpd_ws_recv_frame(req, &frame, sizeof(cmd.json) - 1) != ESP_OK) {
    return ESP_FAIL;
  }
  cmd.json[frame.len] = 0;  // null-terminieren fuer den JSON-Parser im loop-Task

  // In die Queue zur Verarbeitung im loop-Task. Nicht blockieren: ist die Queue
  // voll, wird der Befehl verworfen (besser als den httpd-Task aufzuhalten).
  if (g_cmdQueue != nullptr) {
    xQueueSend(g_cmdQueue, &cmd, 0);
  }
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
  g_router = _router;     // fuer den Dispatch in update() (loop-Task)

  // Queue fuer eingehende /ws-Befehle (httpd-Task -> loop-Task). Einmalig.
  if (g_cmdQueue == nullptr) {
    g_cmdQueue = xQueueCreate(8, sizeof(WsCommand));
    if (g_cmdQueue == nullptr) {
      Serial.println("[net] WARNUNG: Befehls-Queue nicht angelegt - /ws-Befehle inaktiv.");
    }
  }

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
  // Laeuft im loop()-Task. MUSS nicht-blockierend bleiben (ARBEITSWEISE.md §6).
  if (_httpServer == nullptr || _router == nullptr) return;
  httpd_handle_t server = static_cast<httpd_handle_t>(_httpServer);

  // 1) Eingegangene /ws-Befehle hier (im loop-Task) abarbeiten - so ist der
  //    Modulzugriff single-threaded, genau wie bei Menue und Serial-Konsole.
  if (g_cmdQueue != nullptr) {
    WsCommand cmd;
    while (xQueueReceive(g_cmdQueue, &cmd, 0) == pdTRUE) {
      processWebCommand(cmd.json);
    }
  }

  // 2) Etwa alle 500 ms den Live-Status an ALLE verbundenen Clients schieben.
  if (millis() - _lastPush < 500) return;
  _lastPush = millis();
  broadcastToClients(server, buildStatusJson());
}

void NetworkService::processWebCommand(const char* json) {
  // Rohes Befehls-JSON vom Handy auswerten und ueber den CommandRouter an das
  // Zielmodul geben - DERSELBE Weg wie Menue und Serial-Konsole. Danach eine
  // Quittung { "type":"event", ... } an alle Clients broadcasten.
  JsonDocument doc;
  bool ok = false;
  String message;

  if (deserializeJson(doc, json) != DeserializationError::Ok) {
    message = "Ungueltiger Befehl (JSON)";
  } else {
    const char* module = doc["module"] | "";
    const char* action = doc["action"] | "";
    Serial.printf("[net] /ws > %s %s\n", module, action);

    String errorMessage;
    if (g_router != nullptr) {
      ok = g_router->dispatch(module, action,
                              doc["params"].as<JsonObjectConst>(), &errorMessage);
    } else {
      errorMessage = "Router nicht bereit";
    }
    message = ok ? "Befehl ausgefuehrt"
                 : (errorMessage.length() > 0 ? errorMessage : "Befehl abgelehnt");
  }

  JsonDocument resp;
  resp["type"] = "event";
  resp["ok"] = ok;
  resp["message"] = message;
  String out;
  serializeJson(resp, out);
  broadcastToClients(static_cast<httpd_handle_t>(_httpServer), out);
}

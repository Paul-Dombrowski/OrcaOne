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

#include <WiFi.h>
#include <esp_http_server.h>
#include <ArduinoJson.h>
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

// --- WebSocket-Befehlsqueue (HTTP-Task -> loop()-Task) ----------------------
// Der HTTP-Server laeuft in einem EIGENEN FreeRTOS-Task. Wuerde man Befehle
// direkt dort dispatchen, liefe das parallel zu loop()->update() der Module
// (z. B. IR manipuliert std::vector) -> Race/Crash. Darum legt der WS-Handler
// die rohe Nachricht nur in diese Queue; abgearbeitet (geparst + dispatcht)
// wird sie in loop() (NetworkService::drainCommands), also single-threaded zu
// den Modulen. Feste Puffergroesse -> kein Heap, Queue kopiert by-value.
struct WsCommand {
  char json[240];
};
QueueHandle_t gCommandQueue = nullptr;

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

// WebSocket-Endpunkt /ws. STUFE 2b (Status-Push alle ~500 ms + Befehlsempfang
// ueber den CommandRouter) ist noch NICHT umgesetzt - das kommt als naechstes.
// Dieser Handler nimmt nur den Handshake an und haelt die Verbindung offen,
// damit app.js nicht im 2-Sekunden-Takt neu verbindet (und der Server keine
// "400 Bad Request"-Warnungen mehr erzeugt). Eingehende Frames werden geleert.
esp_err_t handleWebSocket(httpd_req_t* req) {
  if (req->method == HTTP_GET) {
    // Der Server hat den Upgrade bereits bestaetigt (HTTP 101). Verbindung
    // offen lassen, noch keine Daten senden.
    Serial.println("[net] WebSocket /ws verbunden (Status-Push folgt in Stufe 2b).");
    return ESP_OK;
  }

  // Eingehenden Befehls-Frame einlesen. Erst Laenge ermitteln, dann Nutzdaten.
  httpd_ws_frame_t frame;
  memset(&frame, 0, sizeof(frame));
  frame.type = HTTPD_WS_TYPE_TEXT;
  if (httpd_ws_recv_frame(req, &frame, 0) != ESP_OK) return ESP_FAIL;

  if (frame.len > 0 && frame.len < sizeof(WsCommand::json)) {
    uint8_t buffer[sizeof(WsCommand::json)];
    frame.payload = buffer;
    if (httpd_ws_recv_frame(req, &frame, sizeof(buffer)) == ESP_OK) {
      Serial.printf("[net] /ws Frame empfangen (%u Bytes)\n", (unsigned)frame.len);
      if (gCommandQueue != nullptr) {
        // NICHT hier dispatchen (fremder Task) - nur in die Queue legen; loop()
        // arbeitet sie ab. Bei voller Queue verwerfen (kein Blockieren im Handler).
        WsCommand cmd;
        memcpy(cmd.json, buffer, frame.len);
        cmd.json[frame.len] = '\0';
        if (xQueueSend(gCommandQueue, &cmd, 0) != pdTRUE) {
          Serial.println("[net] /ws Befehl verworfen (Queue voll)");
        }
      } else {
        Serial.println("[net] /ws Frame verworfen (keine Queue)");
      }
    }
  }
  return ESP_OK;
}

}  // namespace

// Das eine, global erreichbare Objekt (Muster wie "Storage"/"Console").
NetworkService Net;

void NetworkService::begin(CommandRouter* router) {
  _router = router;

  // Befehlsqueue fuer WS -> loop() (siehe WsCommand). Vor dem HTTP-Server, damit
  // der Handler sie beim ersten Frame schon hat.
  if (gCommandQueue == nullptr) {
    gCommandQueue = xQueueCreate(8, sizeof(WsCommand));
  }

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

  // WebSocket-Endpunkt /ws (Handshake annehmen; Daten folgen in Stufe 2b).
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
  // Laeuft im loop()-Task: erst eingegangene WS-Befehle abarbeiten, dann in
  // festem Takt den Status an alle /ws-Clients senden. Beides nicht-blockierend.
  drainCommands();

  if (_httpServer != nullptr && _router != nullptr &&
      millis() - _lastPush >= STATUS_PUSH_MS) {
    _lastPush = millis();
    pushStatus();
  }
}

void NetworkService::drainCommands() {
  if (gCommandQueue == nullptr || _router == nullptr) return;

  WsCommand cmd;
  while (xQueueReceive(gCommandQueue, &cmd, 0) == pdTRUE) {
    JsonDocument doc;
    if (deserializeJson(doc, cmd.json)) continue;  // kaputtes JSON -> ignorieren
    const char* module = doc["module"] | "";
    const char* action = doc["action"] | "";
    if (module[0] == '\0' || action[0] == '\0') continue;

    JsonObjectConst params = doc["params"].as<JsonObjectConst>();
    String err;
    Serial.printf("[net] /ws Befehl: %s.%s\n", module, action);
    // Dispatch HIER (loop()-Task) -> Modulzugriffe single-threaded. Ergebnis geht
    // nicht als Quittung zurueck; die Wirkung erscheint im naechsten Status-Push.
    _router->dispatch(module, action, params, &err);
    if (err.length() > 0) {
      Serial.printf("[net] WS-Befehl %s.%s: %s\n", module, action, err.c_str());
    }
  }
}

void NetworkService::pushStatus() {
  httpd_handle_t server = static_cast<httpd_handle_t>(_httpServer);

  // Status im loop()-Kontext bauen (liest Modulzustand - safe, da gleicher Task
  // wie update()). app.js erwartet {type:"status", mode, modules:{...}}.
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  root["type"] = "status";
  _router->buildStatus(root);
  String out;
  serializeJson(doc, out);

  // An alle verbundenen WebSocket-Clients senden. Kein eigenes fd-Tracking:
  // httpd liefert die offenen Sockets, httpd_ws_get_fd_info filtert die WS.
  int clientFds[8];
  size_t count = sizeof(clientFds) / sizeof(clientFds[0]);
  if (httpd_get_client_list(server, &count, clientFds) != ESP_OK) return;

  int wsCount = 0;
  for (size_t i = 0; i < count; ++i) {
    const int fd = clientFds[i];
    if (httpd_ws_get_fd_info(server, fd) != HTTPD_WS_CLIENT_WEBSOCKET) continue;
    ++wsCount;
    httpd_ws_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.type = HTTPD_WS_TYPE_TEXT;
    frame.payload = reinterpret_cast<uint8_t*>(const_cast<char*>(out.c_str()));
    frame.len = out.length();
    httpd_ws_send_frame_async(server, fd, &frame);  // Fehler (Client weg) ignorieren
  }

  // Einmal melden, sobald tatsaechlich an WS-Clients gesendet wird (Diagnose:
  // bestaetigt, dass der Status-Push die Browser erreicht).
  static int lastWsCount = -1;
  if (wsCount != lastWsCount) {
    lastWsCount = wsCount;
    Serial.printf("[net] Status-Push an %d WebSocket-Client(s)\n", wsCount);
  }
}

// ============================================================================
//  app.js - Verbindung zur Firmware + Bedienlogik der Weboberflaeche
// ----------------------------------------------------------------------------
//  Setzt den Vertrag aus docs/kommunikation/web-api.md um:
//   - empfaengt Status-JSON vom ESP32 und aktualisiert die Anzeige
//   - sendet Befehls-JSON bei Knopfdruck
//  Dieses Geruest funktioniert bereits; neue Module hier analog ergaenzen.
// ============================================================================

// WebSocket-Adresse: gleiche IP wie die Webseite, Pfad /ws.
// (Beim Oeffnen direkt vom ESP32 passt location.host automatisch.)
const WS_URL = `ws://${location.host}/ws`;

let socket;

function connect() {
  socket = new WebSocket(WS_URL);

  socket.onopen  = () => setConn(true);
  socket.onclose = () => { setConn(false); setTimeout(connect, 2000); }; // auto-reconnect
  socket.onerror = () => socket.close();

  socket.onmessage = (ev) => {
    let msg;
    try { msg = JSON.parse(ev.data); } catch { return; }
    if (msg.type === "status") updateStatus(msg);
  };
}

// Einen Befehl an den ESP32 senden (Handy -> ESP32).
function sendCommand(module, action, params = {}) {
  if (!socket || socket.readyState !== WebSocket.OPEN) return;
  socket.send(JSON.stringify({ module, action, params }));
}

// Anzeige anhand des empfangenen Status aktualisieren (ESP32 -> Handy).
function updateStatus(s) {
  setText("mode", s.mode === "auto" ? "Automatik" : "Manuell");

  const m = s.modules || {};
  if (m.nfc) setText("nfc-uid", m.nfc.lastUid || "–");
  if (m.ir)  setText("ir-state", m.ir.recording ? "nimmt auf…" : "bereit");
}

// --- Bedienelemente verdrahten ---
window.addEventListener("DOMContentLoaded", () => {
  // Modus-Umschaltung (Pflicht: Automatik per Handy an/aus)
  byId("btn-auto").onclick   = () => sendCommand("system", "setMode", { mode: "auto" });
  byId("btn-manual").onclick = () => sendCommand("system", "setMode", { mode: "manual" });

  // Alle Buttons mit data-module/data-action automatisch verdrahten.
  document.querySelectorAll("button[data-module]").forEach((btn) => {
    btn.onclick = () => {
      const params = {};
      if (btn.dataset.slot) params.slot = Number(btn.dataset.slot);
      sendCommand(btn.dataset.module, btn.dataset.action, params);
    };
  });

  connect();
});

// --- kleine Helfer ---
function byId(id) { return document.getElementById(id); }
function setText(id, text) { const el = byId(id); if (el) el.textContent = text; }
function setConn(on) {
  const el = byId("conn");
  el.textContent = on ? "verbunden" : "getrennt";
  el.className = "conn " + (on ? "on" : "off");
}

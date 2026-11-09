// Browser client for docs/kommunikation/web-api.md. The firmware currently
// starts its access point, but its HTTP and WebSocket servers are not wired yet.
(() => {
  const RECONNECT_DELAY_MS = 2000;
  const dom = {};
  let socket = null;
  let reconnectTimer = null;
  let connected = false;
  let statusReceived = false;
  let modules = {};
  let mode = null;
  let irState = "idle";
  let preview = false;

  function byId(id) {
    return document.getElementById(id);
  }

  function showMessage(message) {
    dom.message.textContent = message;
  }

  function setConnection(state, label, summary) {
    dom.connection.className = `connection connection-${state}`;
    dom.connectionLabel.textContent = label;
    dom.deviceSummary.textContent = summary;
    dom.statusOrbit.classList.toggle("is-connected", state === "on");
  }

  function localPreviewLocation() {
    const host = window.location.hostname.toLowerCase();
    return window.location.protocol === "file:"
      || host === "localhost"
      || host === "127.0.0.1"
      || host === "[::1]";
  }

  function moduleReady(name) {
    const module = modules[name];
    if (!module || typeof module.ok !== "boolean") return false;
    if (name === "nfc") return module.ok && module.ready !== false;
    return module.ok;
  }

  function setModuleIndicator(id, state, label) {
    const element = byId(id);
    element.className = `module-ready ${state}`;
    element.lastElementChild.textContent = label;
  }

  function renderModuleStatus() {
    const nfc = modules.nfc;
    const ir = modules.ir;
    const jammer = modules.jammer;
    const nfcReady = moduleReady("nfc");
    const irReady = moduleReady("ir");

    if (!statusReceived || !nfc || typeof nfc.ok !== "boolean") {
      setModuleIndicator("nfc-ready", "", "Warte auf Status");
      dom.nfcUid.textContent = "—";
      dom.nfcCardType.textContent = "—";
      dom.nfcNote.textContent = "Das NFC-Modul wurde noch nicht gemeldet.";
    } else if (!nfcReady) {
      setModuleIndicator("nfc-ready", "not-ready", "Nicht bereit");
      dom.nfcUid.textContent = nfc.lastUid || "—";
      dom.nfcCardType.textContent = nfc.lastCardType || "—";
      dom.nfcNote.textContent = nfc.error || "PN532 nicht bereit oder nicht angeschlossen. Andere Module bleiben davon unberührt.";
    } else {
      setModuleIndicator("nfc-ready", "ready", "Bereit");
      dom.nfcUid.textContent = nfc.lastUid || "—";
      dom.nfcCardType.textContent = nfc.lastCardType || "—";
      dom.nfcNote.textContent = nfc.emulating === true
        ? "Karten-Emulation ist laut Firmware aktiv."
        : nfc.readPending === true
          ? "Lesevorgang angefordert. Halte den Tag an den PN532."
          : nfc.lastUid
            ? "Zuletzt erkannte UID · Tag lesen startet einen neuen Scan."
            : "Der PN532 sucht fortlaufend nach NFC-Tags.";
    }

    if (!statusReceived || !ir || typeof ir.ok !== "boolean") {
      setModuleIndicator("ir-ready", "", "Warte auf Status");
      dom.irState.textContent = "Noch kein Status";
      dom.irSlots.textContent = "—";
      dom.irNewest.textContent = "—";
      dom.irNote.textContent = "Das Infrarot-Modul wurde noch nicht gemeldet.";
      irState = "idle";
    } else if (!irReady) {
      setModuleIndicator("ir-ready", "not-ready", "Nicht bereit");
      dom.irState.textContent = "Modul nicht bereit";
      dom.irSlots.textContent = "—";
      dom.irNewest.textContent = "—";
      dom.irNote.textContent = "Das Infrarot-Modul meldet sich als nicht verfügbar.";
      irState = "idle";
    } else {
      irState = typeof ir.state === "string" ? ir.state : "idle";
      const stateLabels = {
        idle: "Bereit",
        recording: "Aufnahme läuft",
        sending: "Sendet gespeicherte Signale",
        shutoff: "Ausschalt-Sequenz läuft"
      };
      const hardwareDetectable = ir.hardwareDetectable !== false;
      const busy = irState !== "idle";
      setModuleIndicator(
        "ir-ready",
        hardwareDetectable ? "ready" : "unverified",
        hardwareDetectable ? (busy ? "In Betrieb" : "Bereit") : "Sensor nicht prüfbar"
      );
      dom.irState.textContent = stateLabels[irState] || "Unbekannter Zustand";
      dom.irSlots.textContent = Number.isInteger(ir.slots) ? String(ir.slots) : "—";
      dom.irNewest.textContent = Number.isInteger(ir.newest) && ir.newest > 0 ? String(ir.newest) : "—";
      dom.irNote.textContent = !hardwareDetectable
        ? irState === "recording"
          ? "Aufnahme läuft. Ob ein IR-Empfänger angeschlossen ist, kann die Firmware nicht erkennen."
          : "Empfänger und Sender sind nicht prüfbar. Ohne Empfänger werden keine Signale aufgenommen."
        : irState === "recording"
          ? "Die Aufnahme läuft bis zum Stopp-Befehl und legt einen neuen Speicherplatz an."
          : irState === "sending"
            ? "Gespeicherte Signale werden wiedergegeben. Mit Stopp wird die Wiedergabe beendet."
            : irState === "shutoff"
              ? `Die Ausschalt-Sequenz läuft${ir.hasShutoff === true ? "." : "; eine gespeicherte Liste wird benötigt."}`
              : "Aufnahme und Wiedergabe sind im Ruhezustand.";
    }

    if (!statusReceived || !jammer || typeof jammer.ok !== "boolean") {
      setModuleIndicator("jammer-ready", "", "Warte auf Status");
      dom.jammerState.textContent = "Noch kein Status";
      dom.jammerNote.textContent = "Das NRF24-Modul wurde noch nicht gemeldet.";
    } else if (!jammer.ok) {
      setModuleIndicator("jammer-ready", "not-ready", "Nicht bereit");
      dom.jammerState.textContent = "Nicht verfügbar";
      dom.jammerNote.textContent = "Die Firmware hat beim Start keine Antwort vom NRF24 erhalten.";
    } else {
      const active = jammer.active === true;
      setModuleIndicator("jammer-ready", active ? "not-ready" : "ready", active ? "Aktiv" : "Bereit");
      dom.jammerState.textContent = active ? "Aktiv" : "Bereit";
      dom.jammerNote.textContent = active
        ? `Firmware meldet den Betrieb auf Kanal ${Number.isInteger(jammer.channel) ? jammer.channel : "—"}.`
        : "NRF24 erkannt. Die Weboberfläche zeigt den Status, bietet aber keine Aktivierungssteuerung.";
    }

    dom.nfcRead.disabled = !connected || !statusReceived || !nfcReady;
    dom.irRecord.disabled = !connected || !statusReceived || !irReady || irState === "recording";
    dom.irStop.disabled = !connected || !statusReceived || !irReady || irState === "idle";
  }

  function renderMode() {
    if (!statusReceived) {
      dom.mode.textContent = "Noch kein Status";
      setModuleIndicator("system-ready", "", "Warte auf Status");
      dom.systemNote.textContent = "Warte auf Gerätestatus.";
      return;
    }

    if (mode === "no-mercy" || mode === "nomercy" || mode === "auto") {
      dom.mode.textContent = "No-Mercy aktiv";
      setModuleIndicator("system-ready", "not-ready", "Aktiv");
      dom.systemNote.textContent = "Der Firmware-Modus schaltet Jammer und IR-Ausschaltsequenz. Eine Umschaltung über die Weboberfläche ist nicht verfügbar.";
    } else if (mode === "manual" || mode === "off") {
      dom.mode.textContent = "Manuell";
      setModuleIndicator("system-ready", "ready", "Bereit");
      dom.systemNote.textContent = "Manueller Betrieb. Der No-Mercy-Modus kann über die Firmware-Bedienelemente geschaltet werden.";
    } else {
      dom.mode.textContent = "Nicht gemeldet";
      setModuleIndicator("system-ready", "not-ready", "Unbekannt");
      dom.systemNote.textContent = "Die Firmware hat keinen bekannten Betriebsmodus gemeldet.";
    }
  }

  function setOfflineState(message) {
    connected = false;
    statusReceived = false;
    modules = {};
    mode = null;
    irState = "idle";
    renderModuleStatus();
    renderMode();
    dom.statusTime.textContent = "—";
    setConnection("off", "Getrennt", "Keine Verbindung zur Firmware");
    showMessage(message);
  }

  function sendCommand(module, action, params) {
    if (!connected || !statusReceived || !moduleReady(module) || !socket || socket.readyState !== WebSocket.OPEN) {
      showMessage("Aktion nicht gesendet: Verbindung oder Modulstatus ist nicht verfügbar.");
      return;
    }

    try {
      socket.send(JSON.stringify({ module, action, params: params || {} }));
      const actionLabel = action === "read"
        ? "NFC-Leseanfrage gesendet."
        : action === "record"
          ? "IR-Aufnahme angefordert."
          : "Stopp-Anfrage gesendet.";
      showMessage(`${actionLabel} Es gibt laut Protokoll keine Befehlsquittung; der Modulstatus wird per Statusmeldung aktualisiert.`);
    } catch (error) {
      showMessage("Der Befehl konnte nicht gesendet werden. Verbindung prüfen und erneut versuchen.");
    }
  }

  function updateStatus(status) {
    const firstStatus = !statusReceived;
    connected = true;
    statusReceived = true;
    modules = status.modules && typeof status.modules === "object" ? status.modules : {};
    mode = status.mode;

    const time = new Intl.DateTimeFormat("de-DE", { hour: "2-digit", minute: "2-digit", second: "2-digit" });
    dom.statusTime.textContent = time.format(new Date());
    setConnection("on", "Verbunden", "ESP32 verbunden · Status aktuell");
    renderModuleStatus();
    renderMode();
    if (firstStatus) {
      showMessage("Live-Status vom ESP32 empfangen. Die Firmware sendet laut Protokoll Statusmeldungen ungefähr alle 500 ms.");
    }
  }

  function onMessage(event) {
    let message;
    try {
      message = JSON.parse(event.data);
    } catch (error) {
      showMessage("Eine ungültige Nachricht vom Gerät wurde ignoriert.");
      return;
    }

    if (message && message.type === "status") {
      updateStatus(message);
    } else if (message && message.type === "event") {
      const prefix = message.ok === false ? "Firmware meldet einen Fehler" : "Firmware-Ereignis";
      showMessage(message.message ? `${prefix}: ${message.message}` : `${prefix} empfangen.`);
    }
  }

  function connect() {
    if (preview || !/^https?:$/.test(window.location.protocol) || !window.location.host) {
      preview = true;
      setOfflineState("Lokale Vorschau: Es wird keine WebSocket-Verbindung geöffnet. Hardwareaktionen sind deaktiviert. Für Live-Daten muss die Seite vom ESP32 mit verbundenem WebSocket-Server bereitgestellt werden.");
      dom.connection.className = "connection connection-preview";
      dom.connectionLabel.textContent = "Lokale Vorschau";
      dom.deviceSummary.textContent = "Offline · nur Layoutvorschau";
      dom.linkMode.textContent = "LOKALE VORSCHAU";
      return;
    }

    const scheme = window.location.protocol === "https:" ? "wss:" : "ws:";
    try {
      socket = new WebSocket(`${scheme}//${window.location.host}/ws`);
    } catch (error) {
      setOfflineState("WebSocket konnte nicht gestartet werden. Hardwareaktionen bleiben deaktiviert.");
      return;
    }

    socket.onopen = () => {
      connected = true;
      setConnection("on", "Verbunden", "Verbunden · warte auf Status");
      showMessage("WebSocket verbunden. Warte auf die erste Statusmeldung des ESP32.");
      renderModuleStatus();
    };
    socket.onmessage = onMessage;
    socket.onerror = () => {
      if (socket && socket.readyState !== WebSocket.CLOSED) socket.close();
    };
    socket.onclose = () => {
      connected = false;
      statusReceived = false;
      modules = {};
      mode = null;
      irState = "idle";
      renderModuleStatus();
      renderMode();
      dom.statusTime.textContent = "—";
      setConnection("off", "Getrennt", "Verbindung zur Firmware getrennt");
      showMessage("Keine Verbindung zur Firmware. Erneuter Verbindungsversuch in 2 Sekunden. Die Firmware muss Webserver und WebSocket bereitstellen.");
      if (!preview) {
        window.clearTimeout(reconnectTimer);
        reconnectTimer = window.setTimeout(connect, RECONNECT_DELAY_MS);
      }
    };
  }

  window.addEventListener("DOMContentLoaded", () => {
    dom.connection = byId("conn");
    dom.connectionLabel = byId("conn-label");
    dom.deviceSummary = byId("device-summary");
    dom.statusOrbit = document.querySelector(".status-orbit");
    dom.statusTime = byId("status-time");
    dom.linkMode = byId("link-mode");
    dom.nfcUid = byId("nfc-uid");
    dom.nfcCardType = byId("nfc-card-type");
    dom.nfcNote = byId("nfc-note");
    dom.irState = byId("ir-state");
    dom.irSlots = byId("ir-slots");
    dom.irNewest = byId("ir-newest");
    dom.irNote = byId("ir-note");
    dom.jammerState = byId("jammer-state");
    dom.jammerNote = byId("jammer-note");
    dom.systemNote = byId("system-note");
    dom.mode = byId("mode");
    dom.message = byId("app-message");
    dom.nfcRead = byId("btn-nfc-read");
    dom.irRecord = byId("btn-ir-record");
    dom.irStop = byId("btn-ir-stop");

    dom.nfcRead.addEventListener("click", () => sendCommand("nfc", "read"));
    dom.irRecord.addEventListener("click", () => sendCommand("ir", "record"));
    dom.irStop.addEventListener("click", () => sendCommand("ir", "stop"));

    preview = localPreviewLocation();
    connect();
  });
})();

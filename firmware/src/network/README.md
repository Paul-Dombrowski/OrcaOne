# network/ — WLAN, Webserver & WebSocket

Dieser Teil verbindet das Gerät mit dem Smartphone. Er deckt **Aufgabe 3
(IoT-Kommunikation, 25 Punkte)** ab — den punktträchtigsten Bereich.

**Bibliotheken:** `ESPAsyncWebServer` + `AsyncTCP`, `ArduinoJson`.
Die genaue Schnittstelle (Endpunkte, Nachrichtenformat) steht in
[`../../../docs/kommunikation/web-api.md`](../../../docs/kommunikation/web-api.md).

## Aufgaben des NetworkManager

1. **WLAN-Hotspot** aufspannen (Access-Point-Modus) — funktioniert **ohne
   Internet**, wie vom Projektauftrag verlangt. SSID/Passwort aus `config.h`.
2. **Webserver**: liefert die Weboberfläche (HTML/CSS/JS aus dem Dateisystem).
3. **WebSocket** (`/ws`): hält eine Live-Verbindung zum Handy offen für
   - **Status-Push**: alle ~500 ms (`STATUS_PUSH_MS`) den Zustand aller Module
     als JSON ans Handy senden → Werte aktualisieren sich automatisch (Pflicht:
     „ohne manuelles Neuladen").
   - **Befehle empfangen**: JSON vom Handy entgegennehmen und an das richtige
     Modul weiterreichen (`handleCommand`).

## So reicht es Befehle an Module weiter

Der NetworkManager bekommt beim Start die Liste aller Module und den
ModeManager (siehe `main.cpp`). Kommt z. B.
`{ "module":"ir", "action":"send", "params":{"slot":1} }` herein, sucht er das
Modul mit `name() == "ir"` und ruft dessen `handleCommand("send", params)` auf.
Befehle an `"system"` (z. B. Modus umschalten) gehen an den ModeManager.

## Status zusammenbauen

Für den Status-Push geht der NetworkManager alle Module durch und ruft
`statusJson()` auf, gebündelt zu:

```json
{ "mode": "manual", "modules": { "nfc": {...}, "ir": {...}, "jammer": {...} } }
```

## To-do

- [ ] `NetworkManager.h` / `.cpp` anlegen.
- [ ] Access-Point starten, IP im Serial-Monitor ausgeben.
- [ ] Statische Dateien aus LittleFS ausliefern.
- [ ] WebSocket: Status-Push + Befehlsempfang gemäß web-api.md.

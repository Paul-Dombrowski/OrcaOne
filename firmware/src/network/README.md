# network/ — WLAN, Webserver & WebSocket

Dieser Teil verbindet das Gerät mit dem Smartphone. Er deckt **Aufgabe 3
(IoT-Kommunikation, 25 Punkte)** ab — den punktträchtigsten Bereich.

**Bibliotheken:** `ESPAsyncWebServer` + `AsyncTCP`, `ArduinoJson`.
Die genaue Schnittstelle (Endpunkte, Nachrichtenformat) steht in
[`../../../docs/kommunikation/web-api.md`](../../../docs/kommunikation/web-api.md).

## Aufgaben des NetworkManager

1. **WLAN-Hotspot** aufspannen (Access-Point-Modus) — funktioniert **ohne
   Internet**, wie vom Projektauftrag verlangt. SSID/Passwort aus `config.h`
   (bzw. `secrets.h`). Der Hotspot ist **passwortgeschützt**; Besucher verbinden
   sich per **QR-Code**, der SSID + Passwort enthält.
2. **Webserver**: liefert die Weboberfläche (HTML/CSS/JS aus dem Dateisystem).
3. **WebSocket** (`/ws`): hält eine Live-Verbindung zum Handy offen für
   - **Status-Push**: alle ~500 ms (`STATUS_PUSH_MS`) den Zustand aller Module
     als JSON ans Handy senden → Werte aktualisieren sich automatisch (Pflicht:
     „ohne manuelles Neuladen").
   - **Befehle empfangen**: JSON vom Handy entgegennehmen und an das richtige
     Modul weiterreichen (`handleCommand`).

**Zugriffsmodell (festgelegt):** **Jedes** verbundene Handy darf steuern — es gibt
**keine** App-seitige Zugriffssperre und keine „nur einer steuert"-Logik. Der
einzige Schutz ist das WLAN-Passwort. Alle verbundenen Geräte erhalten denselben
Status-Push.

## So reicht es Befehle an Module weiter

Der NetworkManager verteilt Befehle **nicht selbst**, sondern gibt sie an den
gemeinsamen `CommandRouter` in `core/` (den nutzen auch Menü und Serial-Konsole,
siehe [`../core/README.md`](../core/README.md)). Kommt z. B.
`{ "module":"ir", "action":"send", "params":{"slot":1} }` herein, ruft er
`router.dispatch("ir", "send", params)` auf. Der Router sucht das Modul mit
`name() == "ir"` bzw. gibt `"system"` an den ModeManager.

## Status zusammenbauen

Für den Status-Push ruft der NetworkManager `router.buildStatus(...)` auf. Das
Ergebnis ist:

```json
{ "mode": "manual", "modules": { "nfc": {...}, "ir": {...}, "jammer": {...} } }
```

## To-do

- [ ] `NetworkManager.h` / `.cpp` anlegen.
- [ ] Access-Point starten, IP im Serial-Monitor ausgeben.
- [ ] Statische Dateien aus LittleFS ausliefern.
- [ ] WebSocket: Status-Push + Befehlsempfang gemäß web-api.md.

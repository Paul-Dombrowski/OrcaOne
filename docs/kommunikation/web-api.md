# Web-API: Nachrichtenformat Handy ↔ ESP32

**Verbindlicher Vertrag** zwischen Firmware (`firmware/src/network/`) und
Weboberfläche (`firmware/web/js/`). Beide Seiten müssen sich exakt daran halten.
Wer an einer Seite arbeitet, gibt einer KI diese Datei als Kontext mit.

- **Transport:** WebSocket unter `ws://<esp32-ip>/ws`
- **Format:** JSON (Text-Frames)
- **Zugriff:** keine App-seitige Zugriffskontrolle — jedes verbundene Handy darf
  Befehle senden. Schutz allein über das WLAN-Passwort (siehe `README.md`).

---

## 1. ESP32 → Handy: Status-Push

Der ESP32 sendet unaufgefordert alle ~500 ms den Gesamtzustand. Die
Weboberfläche aktualisiert damit ihre Anzeige.

```json
{
  "type": "status",
  "mode": "manual",
  "modules": {
    "nfc":    { "ok": true, "ready": true, "lastUid": "04A1B2C3", "tagPresent": false, "stored": 1 },
    "ir":     { "ok": true, "recording": false, "lastProtocol": "NEC", "slots": [true, false, false] },
    "jammer": { "ok": false, "active": false },
    "display":{ "ok": true },
    "input":  { "ok": true }
  }
}
```

- `mode`: `"auto"` oder `"manual"`.
- `modules`: ein Eintrag pro Modul, Schlüssel = `name()` des Moduls. Der Inhalt
  ist genau das, was das jeweilige Modul in `statusJson()` schreibt (siehe die
  README im Modul-Ordner).
- **`ok` ist Pflicht in jedem Modul:** `false` heißt, das Bauteil hat beim Start
  nicht geantwortet (z. B. nicht angeschlossen). Die Weboberfläche zeigt das Modul
  dann ausgegraut an, statt Befehle zu senden. Alle anderen Module laufen normal
  weiter (siehe `docs/ARBEITSWEISE.md` §6 Regel 5).

---

## 2. Handy → ESP32: Befehl

Die Weboberfläche sendet bei Nutzeraktionen ein Befehls-JSON:

```json
{ "module": "ir", "action": "send", "params": { "slot": 1 } }
```

- `module`: Ziel-Modul (`name()`), oder `"system"` für Geräte-Einstellungen.
- `action`: die gewünschte Aktion (siehe Tabelle unten).
- `params`: optionale Zusatzdaten (Objekt, darf fehlen/leer sein).

### Verfügbare Befehle

| module | action | params | Wirkung |
|---|---|---|---|
| `system` | `setMode` | `{ "mode": "auto"\|"manual" }` | Automatik ein-/ausschalten |
| `nfc` | `read` | – | Tag einlesen |
| `nfc` | `write` | `{ "slot": n }` | Daten auf Tag schreiben |
| `nfc` | `clone` | – | lesen + zum Schreiben vormerken |
| `ir` | `record` | `{ "slot": n }` | IR-Signal in Slot aufnehmen |
| `ir` | `send` | `{ "slot": n }` | IR-Signal aus Slot senden |
| `ir` | `shutoff` | – | [E] Hersteller-Ausschaltcodes nacheinander senden |
| `ir` | `stop` | – | Aufnahme oder Shut-Off-Sequenz abbrechen |
| `jammer` | `start` / `stop` | – | 2,4-GHz-Modul an/aus (abgestimmter Rahmen) |

> Diese Tabelle wächst mit den Modulen. Neue Befehle hier eintragen **und** im
> jeweiligen Modul in `handleCommand()` umsetzen — sonst kennen sich beide Seiten
> nicht.

---

## 3. (Optional) ESP32 → Handy: Quittung / Ereignis

Für Rückmeldungen auf einen Befehl oder spontane Ereignisse:

```json
{ "type": "event", "module": "ir", "ok": true, "message": "Signal gesendet" }
```

Kann später ergänzt werden; für den Grundbetrieb reicht der Status-Push aus
Abschnitt 1.

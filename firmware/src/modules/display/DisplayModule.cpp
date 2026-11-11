#include "DisplayModule.h"

#include "../../config.h"
#include "../../core/CommandRouter.h"
#include "../../storage/StorageService.h"
#include <Wire.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <ArduinoJson.h>
#include <stdio.h>

namespace {
uint8_t wireByteCallback(u8x8_t* u8x8, uint8_t message, uint8_t argument,
                         void* data) {
  switch (message) {
    case U8X8_MSG_BYTE_INIT:
    case U8X8_MSG_BYTE_SET_DC:
      return 1;
    case U8X8_MSG_BYTE_START_TRANSFER:
      Wire.beginTransmission(u8x8_GetI2CAddress(u8x8) >> 1);
      return 1;
    case U8X8_MSG_BYTE_SEND:
      return Wire.write(static_cast<const uint8_t*>(data), argument) == argument;
    case U8X8_MSG_BYTE_END_TRANSFER:
      return Wire.endTransmission() == 0;
    default:
      return 0;
  }
}

uint8_t wireGpioAndDelayCallback(u8x8_t*, uint8_t message, uint8_t argument,
                                 void*) {
  switch (message) {
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
    case U8X8_MSG_GPIO_RESET:
    case U8X8_MSG_DELAY_NANO:
    case U8X8_MSG_DELAY_100NANO:
      return 1;
    case U8X8_MSG_DELAY_MILLI:
      delay(argument);
      return 1;
    case U8X8_MSG_DELAY_10MICRO:
      delayMicroseconds(10U * argument);
      return 1;
    default:
      return 1;
  }
}

// PAN/Schluessel in 4er-Gruppen fuer die Anzeige (max. maxLen Zeichen).
String groupDigits(const String& in, int maxLen) {
  String out;
  for (int i = 0; i < (int)in.length(); ++i) {
    if (i > 0 && i % 4 == 0) out += ' ';
    out += in[i];
    if ((int)out.length() >= maxLen) break;
  }
  return out;
}
}  // namespace

// ===========================================================================
//  Lebenszyklus
// ===========================================================================
void DisplayModule::begin() {
  if (!Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL)) {
    Serial.println("[display] I2C-Bus konnte nicht initialisiert werden - Modul inaktiv");
    _ok = false;
    return;
  }

  Wire.beginTransmission(OLED_I2C_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("[display] OLED (SH1106) nicht gefunden - Modul inaktiv");
    _ok = false;
    return;
  }

  u8g2_Setup_sh1106_i2c_128x64_noname_f(
      &_display, U8G2_R0, wireByteCallback, wireGpioAndDelayCallback);
  u8x8_SetI2CAddress(&_display.u8x8, OLED_I2C_ADDR << 1);
  u8g2_InitDisplay(&_display);

  u8g2_SetPowerSave(&_display, 0);

  // SH1106-Spalten-Offset fuer DIESES Panel: 0 (sichtbar ab Spalte 0; NICHT die
  // bei manchen 1,3"-Modulen uebliche Zentrierung mit Offset 2 - sonst waere
  // alles 2px zu weit rechts: Streifen links + Pfeile rechts abgeschnitten).
  // Zur Sicherheit erst Panel-Spalten 0..129 einmalig schwaerzen (Power-On-Muell
  // in Randspalten, die der SSD1306-Ersatztreiber beim Clear sonst auslaesst);
  // der Durchlauf endet beim Betriebs-Offset 0.
  u8g2_ClearBuffer(&_display);
  _display.u8x8.x_offset = 2;
  u8g2_SendBuffer(&_display);  // Panel 2..129 schwarz
  _display.u8x8.x_offset = 0;
  u8g2_SendBuffer(&_display);  // Panel 0..127 schwarz, Betriebs-Offset 0

  _ok = true;
  _screen = S_MENU;
  _needsRedraw = true;
  Serial.println("[display] Orca-Menue bereit.");
}

// ===========================================================================
//  Hauptschleife
// ===========================================================================
void DisplayModule::update() {
  if (!_ok) return;

  if (isLiveScreen(_screen)) {
    // Live-Screens zeigen Modul-Status -> haeufiger neu zeichnen, aber NIE
    // automatisch verlassen (Rueckkehr nur per Long-Press).
    if (_needsRedraw || millis() - _lastDraw >= kLiveRefreshMs) renderCurrent();
    return;
  }

  if (_screen == S_MESSAGE) {
    if (_needsRedraw || millis() - _lastDraw >= kRefreshMs) renderCurrent();
    if (static_cast<long>(millis() - _messageUntil) >= 0) startScreen(_originMenu);
    return;
  }

  // Menues, Listen, Info-Seiten, Kartenansicht: bei Bedarf oder periodisch
  // (Selbst-Heil-Redraw gegen gestoerte I2C-Frames auf dem geteilten Bus).
  if (_needsRedraw || millis() - _lastDraw >= kRefreshMs) renderCurrent();
}

// ===========================================================================
//  Eingabe
// ===========================================================================
void DisplayModule::handleInput(InputEvent event) {
  if (!_ok || event == InputEvent::None) return;

  // --- Statische Menues ---
  if (isMenuScreen(_screen)) {
    const int index = menuIndex(_screen);
    int count = 0;
    const MenuEntry* entries = menuFor(_screen, count);
    if (entries == nullptr || count == 0) return;

    if (event == InputEvent::Up) {
      _sel[index] = (_sel[index] + count - 1) % count;
    } else if (event == InputEvent::Down) {
      _sel[index] = (_sel[index] + 1) % count;
    } else if (event == InputEvent::Click) {
      activateMenuEntry(entries[_sel[index]]);
      return;
    } else if (event == InputEvent::LongPress) {
      goBack();
      return;
    }
    if (_sel[index] < _off[index]) {
      _off[index] = _sel[index];
    } else if (_sel[index] >= _off[index] + kVisibleMenuRows) {
      _off[index] = _sel[index] - kVisibleMenuRows + 1;
    }
    _needsRedraw = true;
    return;
  }

  // --- Dynamische Listen (IR-Slots / NFC-Karten) ---
  if (isListScreen(_screen)) {
    if (event == InputEvent::Click) {
      activateListItem();
      return;
    }
    if (event == InputEvent::LongPress) {
      goBack();
      return;
    }
    if (_listCount > 0) {
      if (event == InputEvent::Up) {
        _listSel = (_listSel + _listCount - 1) % _listCount;
      } else if (event == InputEvent::Down) {
        _listSel = (_listSel + 1) % _listCount;
      }
      if (_listSel < _listOff) {
        _listOff = _listSel;
      } else if (_listSel >= _listOff + kVisibleListRows) {
        _listOff = _listSel - kVisibleListRows + 1;
      }
    }
    _needsRedraw = true;
    return;
  }

  // --- Live-Screens: nur Long-Press (beendet Aktion + zurueck) ---
  if (isLiveScreen(_screen)) {
    if (event == InputEvent::LongPress) goBack();
    return;
  }

  // --- Info-Seiten, Kartenansicht, Meldung: jede Taste geht zurueck ---
  if (event == InputEvent::Click || event == InputEvent::LongPress) goBack();
}

// ===========================================================================
//  Navigation
// ===========================================================================
void DisplayModule::goBack() {
  if (_screen == S_MENU) {
    _needsRedraw = true;  // Wurzel: nur neu zeichnen
    return;
  }
  if (isLiveScreen(_screen)) stopLive(_screen);  // laufende Aktion beenden
  if (_screen == S_MESSAGE) {
    startScreen(_originMenu);
    return;
  }
  startScreen(parentOf(_screen));
}

void DisplayModule::stopLive(Screen s) {
  String err;
  if (s == S_NFC_READ) {
    dispatchNow("nfc", "stop", nullptr, err);
  } else if (s == S_IR_RECORD) {
    dispatchNow("ir", "stop", nullptr, err);  // speichert die Aufnahme
  } else if (s == S_IR_SEND) {
    dispatchNow("ir", "stop", nullptr, err);
  }
}

bool DisplayModule::isMenuScreen(Screen s) const {
  return s == S_MENU || s == S_IR_MENU || s == S_INFO_MENU;
}
bool DisplayModule::isListScreen(Screen s) const {
  return s == S_IR_SLOTS || s == S_NFC_CARDS;
}
bool DisplayModule::isLiveScreen(Screen s) const {
  return s == S_NFC_READ || s == S_IR_RECORD || s == S_IR_SEND;
}
bool DisplayModule::isInfoPage(Screen s) const {
  return s == S_INFO_WIFI || s == S_INFO_DEVICES;
}

int DisplayModule::menuIndex(Screen s) const {
  switch (s) {
    case S_MENU: return 0;
    case S_IR_MENU: return 1;
    case S_INFO_MENU: return 2;
    default: return -1;
  }
}

DisplayModule::Screen DisplayModule::parentOf(Screen s) const {
  switch (s) {
    case S_IR_MENU: return S_MENU;
    case S_INFO_MENU: return S_MENU;
    case S_IR_SLOTS: return S_IR_MENU;
    case S_NFC_CARDS: return S_MENU;
    case S_NFC_READ: return S_MENU;
    case S_IR_RECORD: return S_IR_MENU;
    case S_IR_SEND: return S_IR_SLOTS;
    case S_CARD_VIEW: return S_NFC_CARDS;
    case S_INFO_WIFI: return S_INFO_MENU;
    case S_INFO_DEVICES: return S_INFO_MENU;
    default: return S_MENU;
  }
}

// ---------------------------------------------------------------------------
//  Menue-Inhalte
// ---------------------------------------------------------------------------
const DisplayModule::MenuEntry* DisplayModule::menuFor(Screen s,
                                                       int& count) const {
  static const MenuEntry mainMenu[] = {
      {"NFC lesen", KGoLive, S_NFC_READ, "nfc", "read"},
      {"Gespeicherte Karten", KList, S_NFC_CARDS},
      {"Infrarot", KNav, S_IR_MENU},
      {"2,4 GHz", KToggle, S_MENU, "jammer", "start", "stop", nullptr, nullptr, 0},
      {"No-Mercy", KToggle, S_MENU, "system", "setMode", "setMode", "nomercy",
       "off", 1},
      {"Info", KNav, S_INFO_MENU},
  };
  static const MenuEntry irMenu[] = {
      {"Aufnehmen", KGoLive, S_IR_RECORD, "ir", "record"},
      {"Senden", KList, S_IR_SLOTS},
      {"Ausschalten", KAction, S_IR_MENU, "ir", "shutoff"},
      {"Stop", KAction, S_IR_MENU, "ir", "stop"},
  };
  static const MenuEntry infoMenu[] = {
      {"WLAN / IP", KNav, S_INFO_WIFI},
      {"Verbundene Geraete", KNav, S_INFO_DEVICES},
  };
  switch (s) {
    case S_MENU:
      count = sizeof(mainMenu) / sizeof(mainMenu[0]);
      return mainMenu;
    case S_IR_MENU:
      count = sizeof(irMenu) / sizeof(irMenu[0]);
      return irMenu;
    case S_INFO_MENU:
      count = sizeof(infoMenu) / sizeof(infoMenu[0]);
      return infoMenu;
    default:
      count = 0;
      return nullptr;
  }
}

bool DisplayModule::dispatchNow(const char* module, const char* action,
                                const char* value, String& err) {
  if (_router == nullptr || module == nullptr || action == nullptr) {
    err = "Router nicht bereit";
    return false;
  }
  JsonDocument p;
  JsonObject o = p.to<JsonObject>();
  if (value != nullptr) o["mode"] = value;
  return _router->dispatch(module, action, o, &err);
}

void DisplayModule::activateMenuEntry(const MenuEntry& e) {
  switch (e.kind) {
    case KNav:
      startScreen(e.target);
      return;
    case KList:
      populateList(e.target);
      startScreen(e.target);
      return;
    case KToggle:
      fireToggle(e);
      return;
    case KAction: {
      String err;
      const bool ok = dispatchNow(e.module, e.action, e.value, err);
      const char* okMsg = "Erledigt";
      if (String(e.action) == "shutoff") okMsg = "Ausschalt-Sequenz laeuft";
      else if (String(e.action) == "stop") okMsg = "Gestoppt";
      showMessage(ok ? okMsg : (err.length() ? err : "Abgelehnt"),
                  ok ? 1500 : 2500);
      return;
    }
    case KGoLive: {
      String err;
      if (!dispatchNow(e.module, e.action, e.value, err)) {
        showMessage(err.length() ? err : "Abgelehnt", 2500);
        return;
      }
      startScreen(e.target);  // bleibender Live-Screen
      return;
    }
  }
}

void DisplayModule::activateListItem() {
  if (_listCount == 0) {
    goBack();
    return;
  }
  const String arg = _listArg[_listSel];

  if (_screen == S_IR_SLOTS) {
    const int slot = arg.toInt();
    JsonDocument p;
    JsonObject o = p.to<JsonObject>();
    o["slot"] = slot;
    String err;
    const bool ok = _router != nullptr &&
                    _router->dispatch("ir", "send", o, &err);
    if (!ok) {
      showMessage(err.length() ? err : "Senden abgelehnt", 2500);
      return;
    }
    _sendSlot = slot;
    startScreen(S_IR_SEND);
    return;
  }

  if (_screen == S_NFC_CARDS) {
    _cardJson = Storage.load("nfc", arg);
    startScreen(S_CARD_VIEW);
    return;
  }
}

void DisplayModule::fireToggle(const MenuEntry& e) {
  const int id = e.toggleId;
  const bool next = (id >= 0 && id < kToggleCount) ? !_toggleState[id] : true;
  const char* action = next ? e.action : e.actionOff;
  const char* value = next ? e.value : e.valueOff;

  String err;
  const bool ok = dispatchNow(e.module, action, value, err);
  if (ok) {
    if (id >= 0 && id < kToggleCount) _toggleState[id] = next;
    showMessage(String(e.label) + (next ? ": AN" : ": AUS"), 1000);
  } else {
    showMessage(err.length() ? err : "Abgelehnt", 2500);
  }
}

void DisplayModule::populateList(Screen s) {
  _listCount = 0;
  _listSel = 0;
  _listOff = 0;

  const char* ns = (s == S_IR_SLOTS) ? "ir" : "nfc";
  const String js = Storage.list(ns);
  JsonDocument d;
  if (deserializeJson(d, js)) return;

  for (JsonVariantConst v : d.as<JsonArrayConst>()) {
    const char* key = v.as<const char*>();
    if (key == nullptr) continue;
    const String k = key;

    if (s == S_IR_SLOTS) {
      if (!k.startsWith("slot")) continue;  // "meta"/"shutoff" ausblenden
      if (_listCount >= kMaxListItems) break;
      _listArg[_listCount] = k.substring(4);
      _listLabel[_listCount] = String("Slot ") + k.substring(4);
      _listCount++;
    } else {  // S_NFC_CARDS
      if (_listCount >= kMaxListItems) break;
      _listArg[_listCount] = k;
      _listLabel[_listCount] = groupDigits(k, 20);
      _listCount++;
    }
  }
}

void DisplayModule::showMessage(const String& text, unsigned long ms) {
  _message = text;
  _messageUntil = millis() + ms;
  _originMenu = isMenuScreen(_screen) ? _screen : S_MENU;
  startScreen(S_MESSAGE);
}

// ===========================================================================
//  Rendering
// ===========================================================================
void DisplayModule::renderCurrent() {
  _needsRedraw = false;
  _lastDraw = millis();
  switch (_screen) {
    case S_MENU:
    case S_IR_MENU:
    case S_INFO_MENU: drawMenu(); break;
    case S_IR_SLOTS:
    case S_NFC_CARDS: drawDynamicList(); break;
    case S_NFC_READ: drawNfcRead(); break;
    case S_IR_RECORD: drawIrRecord(); break;
    case S_IR_SEND: drawIrSend(); break;
    case S_CARD_VIEW: drawCardView(); break;
    case S_INFO_WIFI:
    case S_INFO_DEVICES: drawInfoPage(); break;
    case S_MESSAGE: drawMessage(); break;
  }
}

void DisplayModule::drawMenu() {
  const int index = menuIndex(_screen);
  int count = 0;
  const MenuEntry* entries = menuFor(_screen, count);
  if (index < 0 || entries == nullptr) return;

  u8g2_ClearBuffer(&_display);
  u8g2_SetFont(&_display, u8g2_font_6x12_tr);

  for (int row = 0; row < kVisibleMenuRows; ++row) {
    const int entryIndex = _off[index] + row;
    if (entryIndex >= count) break;
    const MenuEntry& e = entries[entryIndex];
    const int y = row * 16;

    char line[28];
    if (e.kind == KToggle && e.toggleId >= 0 && e.toggleId < kToggleCount) {
      snprintf(line, sizeof(line), "%s: %s", e.label,
               _toggleState[e.toggleId] ? "AN" : "AUS");
    } else {
      snprintf(line, sizeof(line), "%s", e.label);
    }

    if (entryIndex == _sel[index]) {
      u8g2_DrawBox(&_display, 0, y, 128, 15);
      u8g2_SetDrawColor(&_display, 0);
      u8g2_DrawStr(&_display, 4, y + 11, line);
      u8g2_SetDrawColor(&_display, 1);
    } else {
      u8g2_DrawStr(&_display, 4, y + 11, line);
    }
  }

  if (count > kVisibleMenuRows) {
    // Pfeile eingerueckt (max. x=125): bei Offset 2 landet x=125 auf Panel 127
    // = letzte sichtbare Spalte; weiter rechts wuerde abgeschnitten.
    if (_off[index] > 0)
      u8g2_DrawTriangle(&_display, 123, 3, 119, 7, 125, 7);
    if (_off[index] + kVisibleMenuRows < count)
      u8g2_DrawTriangle(&_display, 119, 56, 125, 56, 122, 60);
  }
  u8g2_SendBuffer(&_display);
}

void DisplayModule::drawListFrame(const char* title) {
  u8g2_ClearBuffer(&_display);
  u8g2_SetFont(&_display, u8g2_font_6x10_tr);
  u8g2_DrawStr(&_display, 2, 9, title);
  u8g2_DrawHLine(&_display, 0, 12, 128);
}

void DisplayModule::drawDynamicList() {
  drawListFrame(_screen == S_IR_SLOTS ? "IR senden" : "Karten");
  u8g2_SetFont(&_display, u8g2_font_6x12_tr);

  if (_listCount == 0) {
    u8g2_DrawStr(&_display, 4, 34, "Keine Eintraege");
    u8g2_SetFont(&_display, u8g2_font_6x10_tr);
    u8g2_DrawStr(&_display, 2, 62, "lang = zurueck");
    u8g2_SendBuffer(&_display);
    return;
  }

  // 3 Zeilen unter der Titelzeile (y 16..63).
  const int rows = kVisibleListRows;
  for (int row = 0; row < rows; ++row) {
    const int i = _listOff + row;
    if (i >= _listCount) break;
    const int y = 16 + row * 16;
    if (i == _listSel) {
      u8g2_DrawBox(&_display, 0, y, 128, 15);
      u8g2_SetDrawColor(&_display, 0);
      u8g2_DrawStr(&_display, 4, y + 11, _listLabel[i].c_str());
      u8g2_SetDrawColor(&_display, 1);
    } else {
      u8g2_DrawStr(&_display, 4, y + 11, _listLabel[i].c_str());
    }
  }
  if (_listCount > rows) {
    if (_listOff > 0)
      u8g2_DrawTriangle(&_display, 123, 17, 119, 21, 125, 21);
    if (_listOff + rows < _listCount)
      u8g2_DrawTriangle(&_display, 119, 60, 125, 60, 122, 63);
  }
  u8g2_SendBuffer(&_display);
}

void DisplayModule::drawNfcRead() {
  JsonDocument d;
  JsonObject o = d.to<JsonObject>();
  const bool have = _router != nullptr && _router->moduleStatus("nfc", o);
  const bool moduleOk = o["ok"] | false;
  const bool success = o["cardReadSuccess"] | false;
  const bool pending = o["readPending"] | false;

  u8g2_ClearBuffer(&_display);
  u8g2_SetFont(&_display, u8g2_font_6x10_tr);
  u8g2_DrawStr(&_display, 2, 9, "NFC lesen");
  u8g2_DrawHLine(&_display, 0, 12, 128);

  if (!have || !moduleOk) {
    u8g2_DrawStr(&_display, 2, 30, "NFC nicht bereit");
  } else if (success) {
    const char* num = o["lastCardNumber"] | "";
    const char* exp = o["lastExpiry"] | "";
    const char* type = o["lastCardType"] | "";
    u8g2_DrawStr(&_display, 2, 26, "Karte gelesen:");
    u8g2_DrawStr(&_display, 2, 40, groupDigits(num, 20).c_str());
    char l2[28];
    snprintf(l2, sizeof(l2), "%s  %s", exp, type);
    u8g2_DrawStr(&_display, 2, 52, l2);
  } else if (pending) {
    // Liest noch aktiv -> solange Karte halten (mehrere Versuche bis Erfolg).
    u8g2_DrawStr(&_display, 2, 30, "Lese Karte...");
    u8g2_DrawStr(&_display, 2, 42, "Karte halten");
  } else if ((o["lastUid"] | "")[0] != '\0') {
    // Lesen beendet, Karte war im Feld, aber kein EMV-Dump (z. B. Mifare).
    const char* uid = o["lastUid"] | "";
    u8g2_DrawStr(&_display, 2, 26, "Karte erkannt:");
    char l[28];
    snprintf(l, sizeof(l), "UID %s", uid);
    u8g2_DrawStr(&_display, 2, 40, l);
    u8g2_DrawStr(&_display, 2, 52, "(keine EMV-Daten)");
  } else {
    u8g2_DrawStr(&_display, 2, 30, "Abgebrochen");
  }

  u8g2_SetFont(&_display, u8g2_font_6x10_tr);
  u8g2_DrawStr(&_display, 2, 63, "lang = zurueck");
  u8g2_SendBuffer(&_display);
}

void DisplayModule::drawIrRecord() {
  u8g2_ClearBuffer(&_display);
  u8g2_SetFont(&_display, u8g2_font_6x10_tr);
  u8g2_DrawStr(&_display, 2, 9, "IR Aufnahme");
  u8g2_DrawHLine(&_display, 0, 12, 128);
  u8g2_DrawStr(&_display, 2, 30, "Aufnahme laeuft...");
  u8g2_DrawStr(&_display, 2, 44, "Fernbedienung");
  u8g2_DrawStr(&_display, 2, 54, "druecken");
  u8g2_SetFont(&_display, u8g2_font_6x10_tr);
  u8g2_DrawStr(&_display, 2, 63, "lang = stop + save");
  u8g2_SendBuffer(&_display);
}

void DisplayModule::drawIrSend() {
  u8g2_ClearBuffer(&_display);
  u8g2_SetFont(&_display, u8g2_font_6x10_tr);
  u8g2_DrawStr(&_display, 2, 9, "IR senden");
  u8g2_DrawHLine(&_display, 0, 12, 128);
  char l[24];
  snprintf(l, sizeof(l), "Sende Slot %d", _sendSlot);
  u8g2_DrawStr(&_display, 2, 32, l);
  u8g2_DrawStr(&_display, 2, 46, "(Dauerschleife)");
  u8g2_SetFont(&_display, u8g2_font_6x10_tr);
  u8g2_DrawStr(&_display, 2, 63, "lang = stop");
  u8g2_SendBuffer(&_display);
}

void DisplayModule::drawCardView() {
  u8g2_ClearBuffer(&_display);
  u8g2_SetFont(&_display, u8g2_font_6x10_tr);
  u8g2_DrawStr(&_display, 2, 9, "Karte");
  u8g2_DrawHLine(&_display, 0, 12, 128);

  JsonDocument d;
  if (_cardJson.length() == 0 || deserializeJson(d, _cardJson)) {
    u8g2_DrawStr(&_display, 2, 30, "nicht lesbar");
  } else {
    const char* num = d["cardNumber"] | "";
    const char* exp = d["expiry"] | "";
    const char* type = d["cardType"] | "";
    const char* holder = d["cardholder"] | "";
    u8g2_DrawStr(&_display, 2, 26, groupDigits(num, 20).c_str());
    char l2[28];
    snprintf(l2, sizeof(l2), "Ablauf %s  %s", exp, type);
    u8g2_DrawStr(&_display, 2, 40, l2);
    if (holder[0] != '\0') u8g2_DrawStr(&_display, 2, 52, holder);
  }

  u8g2_SetFont(&_display, u8g2_font_6x10_tr);
  u8g2_DrawStr(&_display, 2, 63, "Taste = zurueck");
  u8g2_SendBuffer(&_display);
}

void DisplayModule::drawInfoPage() {
  u8g2_ClearBuffer(&_display);
  u8g2_SetFont(&_display, u8g2_font_6x10_tr);
  char l[26];

  if (_screen == S_INFO_WIFI) {
    u8g2_DrawStr(&_display, 2, 10, "WLAN / IP");
    u8g2_DrawHLine(&_display, 0, 13, 128);
    u8g2_DrawStr(&_display, 2, 26, "SSID: " WIFI_AP_SSID);
    const String ip = WiFi.softAPIP().toString();
    snprintf(l, sizeof(l), "IP: %s", ip.c_str());
    u8g2_DrawStr(&_display, 2, 38, l);
    snprintf(l, sizeof(l), "Clients: %u",
             (unsigned)WiFi.softAPgetStationNum());
    u8g2_DrawStr(&_display, 2, 50, l);
  } else {
    u8g2_DrawStr(&_display, 2, 10, "Verbundene Geraete");
    u8g2_DrawHLine(&_display, 0, 13, 128);
    wifi_sta_list_t stations;
    if (esp_wifi_ap_get_sta_list(&stations) == ESP_OK) {
      snprintf(l, sizeof(l), "Anzahl: %d", stations.num);
      u8g2_DrawStr(&_display, 2, 26, l);
      for (int i = 0; i < stations.num && i < 2; ++i) {
        const uint8_t* m = stations.sta[i].mac;
        snprintf(l, sizeof(l), "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1],
                 m[2], m[3], m[4], m[5]);
        u8g2_DrawStr(&_display, 2, 38 + i * 11, l);
      }
    } else {
      u8g2_DrawStr(&_display, 2, 26, "nicht verfuegbar");
    }
  }
  u8g2_DrawStr(&_display, 2, 62, "Taste = zurueck");
  u8g2_SendBuffer(&_display);
}

void DisplayModule::drawMessage() {
  u8g2_ClearBuffer(&_display);
  u8g2_SetFont(&_display, u8g2_font_6x10_tr);
  String remaining = _message;
  for (int line = 0; line < 3 && remaining.length() > 0; ++line) {
    int splitAt = remaining.length() > 20 ? remaining.lastIndexOf(' ', 20) : -1;
    if (splitAt <= 0) splitAt = min(static_cast<int>(remaining.length()), 20);
    const String text = remaining.substring(0, splitAt);
    u8g2_DrawStr(&_display, 2, 16 + line * 14, text.c_str());
    remaining.remove(0, splitAt);
    while (remaining.startsWith(" ")) remaining.remove(0, 1);
  }
  u8g2_SendBuffer(&_display);
}

// ===========================================================================
//  Status / Befehle (Web-UI, Serial)
// ===========================================================================
void DisplayModule::statusJson(JsonObject obj) {
  obj["ok"] = _ok;
  obj["screen"] = screenName();
  const int index = menuIndex(_screen);
  obj["selection"] = index >= 0 ? _sel[index] : (isListScreen(_screen) ? _listSel : 0);
  obj["jammer"] = _toggleState[0];
  obj["noMercy"] = _toggleState[1];
  if (_message.length() > 0) obj["message"] = _message;
}

bool DisplayModule::handleCommand(const String& action, JsonObjectConst params) {
  (void)params;
  if (!_ok) return false;

  if (action == "up") {
    handleInput(InputEvent::Up);
    return true;
  }
  if (action == "down") {
    handleInput(InputEvent::Down);
    return true;
  }
  if (action == "ok") {
    handleInput(InputEvent::Click);
    return true;
  }
  if (action == "back") {
    handleInput(InputEvent::LongPress);
    return true;
  }
  return false;
}

void DisplayModule::startScreen(Screen screen) {
  if (screen != S_MESSAGE) _message = "";
  _screen = screen;
  _needsRedraw = true;
}

const char* DisplayModule::screenName() const {
  switch (_screen) {
    case S_MENU: return "menu";
    case S_IR_MENU: return "ir-menu";
    case S_INFO_MENU: return "info-menu";
    case S_IR_SLOTS: return "ir-slots";
    case S_NFC_CARDS: return "nfc-cards";
    case S_NFC_READ: return "nfc-read";
    case S_IR_RECORD: return "ir-record";
    case S_IR_SEND: return "ir-send";
    case S_CARD_VIEW: return "card-view";
    case S_INFO_WIFI: return "info-wifi";
    case S_INFO_DEVICES: return "info-devices";
    case S_MESSAGE: return "message";
    default: return "unknown";
  }
}

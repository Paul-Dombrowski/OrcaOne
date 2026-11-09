#include "DisplayModule.h"

#include "../../config.h"
#include "../../core/CommandRouter.h"
#include <Wire.h>
#include <math.h>
#include <stdio.h>

namespace {
constexpr int kYellowHeight = 16;
constexpr int kOrcaSegments = 16;
constexpr float kOrcaLength = 44.0f;
constexpr int kBubbleCount = 5;
constexpr unsigned long kFrameMs = 30;
constexpr int kVisibleMenuRows = 4;
constexpr uint8_t kOrcaRadii[kOrcaSegments + 1] = {
    1, 1, 2, 2, 3, 3, 4, 5, 5, 6, 6, 6, 6, 5, 5, 4, 3};
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
}  // namespace

void DisplayModule::begin() {
  if (!Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL)) {
    Serial.println("[display] I2C-Bus konnte nicht initialisiert werden - Modul inaktiv");
    _ok = false;
    return;
  }

  // Praesenz des OLED am geteilten I2C-Bus (mit PN532) per Adress-Ping pruefen.
  // Wire.begin() meldet nur den Bus-Controller, nicht ob der Chip antwortet.
  // Ohne diese Pruefung wuerde u8g2 blind auf ein fehlendes Display schreiben
  // -> Dauerspam "i2c_master_transmit failed: ESP_ERR_INVALID_STATE". Fehlt das
  // Display, laeuft der Rest weiter (ARBEITSWEISE.md §6), analog zu NFC/Jammer.
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

  _ok = true;
  randomSeed(micros());
  initBubbles();
  Serial.println("[display] Orca-Menue bereit.");
}

void DisplayModule::update() {
  if (!_ok) return;

  if (isMenuScreen(_screen)) {
    if (_needsRedraw) {
      _needsRedraw = false;
      drawMenu();
    }
    return;
  }

  if (_screen == S_PLATZHALTER || _screen == S_INFO_WIFI ||
      _screen == S_INFO_DEVICES) {
    if (_needsRedraw) {
      _needsRedraw = false;
      if (_screen == S_PLATZHALTER) drawPlatzhalter();
      else drawInfoPage();
    }
    return;
  }

  if (_screen == S_MESSAGE) {
    if (_needsRedraw) {
      _needsRedraw = false;
      drawMessage();
    }
    if (static_cast<long>(millis() - _messageUntil) >= 0) {
      startScreen(_returnTo);
    }
    return;
  }

  if (_screen == S_LADEN) {
    if (millis() - _lastFrame < kFrameMs) return;
    _lastFrame = millis();
    drawLoad();
    if (_hasPendingAction && millis() - _loadStart >= 1200) {
      dispatchPendingAction();
    }
    return;
  }

  if (millis() - _lastFrame < kFrameMs) return;
  _lastFrame = millis();

  switch (_screen) {                                    //switch für regler
    case S_SCHWIMMEN:
      drawSwim();
      break;
    case S_SPRINGEN:
      drawJump();
      break;
    case S_INFO:
      drawInfo();
      break;
    default:
      break;
  }
}

void DisplayModule::handleInput(InputEvent event) {
  if (!_ok || event == InputEvent::None) return;

  if (_screen == S_LADEN) return;

  if (isMenuScreen(_screen)) {
    const int index = menuIndex(_screen);
    int count = 0;
    const MenuEntry* entries = menuFor(_screen, count);
    if (entries == nullptr || count == 0) return;

    if (event == InputEvent::Up) {
      _sel[index] = (_sel[index] + count - 1) % count;
      _needsRedraw = true;
    } else if (event == InputEvent::Down) {
      _sel[index] = (_sel[index] + 1) % count;
      _needsRedraw = true;
    } else if (event == InputEvent::Click) {
      activateMenuEntry(entries[_sel[index]]);
    } else if (event == InputEvent::LongPress && _screen != S_MENU) {
      startScreen(parentMenu(_screen));
    }
    if (event == InputEvent::Up || event == InputEvent::Down) {
      if (_sel[index] < _off[index]) {
        _off[index] = _sel[index];
      } else if (_sel[index] >= _off[index] + kVisibleMenuRows) {
        _off[index] = _sel[index] - kVisibleMenuRows + 1;
      }
    }
    return;
  }

  if (_screen == S_MESSAGE || _screen == S_PLATZHALTER ||
      _screen == S_INFO_WIFI || _screen == S_INFO_DEVICES) {
    if (event == InputEvent::LongPress || event == InputEvent::Click) {
      startScreen(_returnTo);
    }
    return;
  }

  if (event == InputEvent::LongPress || event == InputEvent::Click) {
    startScreen(_returnTo);
  } else if (_screen == S_SCHWIMMEN &&
             (event == InputEvent::Up || event == InputEvent::Down)) {
    _swimSpeed += event == InputEvent::Down ? 0.5f : -0.5f;
    if (_swimSpeed < 0.5f) _swimSpeed = 0.5f;
    if (_swimSpeed > 4.0f) _swimSpeed = 4.0f;
  }
}

void DisplayModule::statusJson(JsonObject obj) {
  obj["ok"] = _ok;
  obj["screen"] = screenName();
  const Screen selectedMenu = isMenuScreen(_screen) ? _screen : _returnTo;
  const int index = menuIndex(selectedMenu);
  obj["selection"] = index >= 0 ? _sel[index] : 0;
  obj["menuOffset"] = index >= 0 ? _off[index] : 0;
  obj["swimSpeed"] = _swimSpeed;
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

int DisplayModule::roundPixel(float value) const {
  return static_cast<int>(lroundf(value));
}

float DisplayModule::bodyAngle(float heading, float curl, float wiggle,
                               float phase, float s) const {
  return heading + curl * PI * s +
         wiggle * (1.0f - s) * sinf(phase + 5.0f * s);
}

void DisplayModule::buildSpine(float tailX, float tailY, float heading,
                               float curl, float wiggle, float phase) {
  const float segmentLength = kOrcaLength / kOrcaSegments;
  _orcaX[0] = tailX;
  _orcaY[0] = tailY;
  for (int i = 0; i <= kOrcaSegments; ++i) {
    _orcaTheta[i] =
        bodyAngle(heading, curl, wiggle, phase, static_cast<float>(i) / kOrcaSegments);
    if (i < kOrcaSegments) {
      const float middleAngle = bodyAngle(
          heading, curl, wiggle, phase, (i + 0.5f) / kOrcaSegments);
      _orcaX[i + 1] = _orcaX[i] + segmentLength * cosf(middleAngle);
      _orcaY[i + 1] = _orcaY[i] + segmentLength * sinf(middleAngle);
    }
  }
}

void DisplayModule::spinePoint(int i, float length, float side, int& x,
                               int& y) const {
  const float dx = cosf(_orcaTheta[i]);
  const float dy = sinf(_orcaTheta[i]);
  const float nx = -dy * _orcaSide;
  const float ny = dx * _orcaSide;
  x = roundPixel(_orcaX[i] + dx * length + nx * side);
  y = roundPixel(_orcaY[i] + dy * length + ny * side);
}

void DisplayModule::discSafe(int x, int y, int radius) {
  if (x < 0 || y < 0 || x > 250 || y > 250) return;
  u8g2_DrawDisc(&_display, x, y, radius, U8G2_DRAW_ALL);
}

void DisplayModule::drawOrca(float tailX, float tailY, float heading, int side,
                             float curl, float wiggle, float phase) {
  _orcaSide = side;
  buildSpine(tailX, tailY, heading, curl, wiggle, phase);

  const float finHeight = 7.0f - 3.0f * curl;
  const float flipperHeight = 6.0f - 3.0f * curl;
  u8g2_SetDrawColor(&_display, 1);

  for (int i = 0; i <= kOrcaSegments; ++i) {
    discSafe(roundPixel(_orcaX[i]), roundPixel(_orcaY[i]), kOrcaRadii[i]);
  }

  int ax, ay, bx, by, tx, ty;
  const float dx = cosf(_orcaTheta[0]);
  const float dy = sinf(_orcaTheta[0]);
  const float nx = -dy;
  const float ny = dx;
  const float x0 = _orcaX[0];
  const float y0 = _orcaY[0];
  u8g2_DrawTriangle(&_display, roundPixel(x0 + dx * 2), roundPixel(y0 + dy * 2),
                    roundPixel(x0 - dx * 5 + nx * 6),
                    roundPixel(y0 - dy * 5 + ny * 6),
                    roundPixel(x0 - dx * 5 - nx * 6),
                    roundPixel(y0 - dy * 5 - ny * 6));
  u8g2_SetDrawColor(&_display, 0);
  u8g2_DrawTriangle(&_display, roundPixel(x0 - dx * 2.5f),
                    roundPixel(y0 - dy * 2.5f),
                    roundPixel(x0 - dx * 6 + nx * 2.5f),
                    roundPixel(y0 - dy * 6 + ny * 2.5f),
                    roundPixel(x0 - dx * 6 - nx * 2.5f),
                    roundPixel(y0 - dy * 6 - ny * 2.5f));
  u8g2_SetDrawColor(&_display, 1);

  spinePoint(8, 0, -(kOrcaRadii[8] - 1), ax, ay);
  spinePoint(12, 0, -(kOrcaRadii[12] - 1), bx, by);
  spinePoint(10, -4, -(kOrcaRadii[10] + finHeight), tx, ty);
  u8g2_DrawTriangle(&_display, ax, ay, bx, by, tx, ty);

  u8g2_SetDrawColor(&_display, 0);
  for (int i = 8; i <= 13; ++i) {
    spinePoint(i, 0, kOrcaRadii[i] - 3, ax, ay);
    u8g2_DrawDisc(&_display, ax, ay, 2, U8G2_DRAW_ALL);
  }
  spinePoint(14, 1, -1, ax, ay);
  u8g2_DrawDisc(&_display, ax, ay, 2, U8G2_DRAW_ALL);
  const int eyeX = ax;
  const int eyeY = ay;

  u8g2_SetDrawColor(&_display, 1);
  spinePoint(11, 0, kOrcaRadii[11] - 1, ax, ay);
  spinePoint(13, 0, kOrcaRadii[13] - 1, bx, by);
  spinePoint(12, -3, kOrcaRadii[12] + flipperHeight, tx, ty);
  u8g2_DrawTriangle(&_display, ax, ay, bx, by, tx, ty);
  u8g2_DrawPixel(&_display, eyeX, eyeY);
}

bool DisplayModule::isMenuScreen(Screen screen) const {
  return screen == S_MENU || screen == S_NFC_MENU || screen == S_IR_MENU ||
         screen == S_JAMMER_MENU ||
         screen == S_MODE_MENU || screen == S_INFO_MENU;
}

int DisplayModule::menuIndex(Screen screen) const {
  switch (screen) {
    case S_MENU: return 0;
    case S_NFC_MENU: return 1;
    case S_IR_MENU: return 2;
    case S_JAMMER_MENU: return 3;
    case S_MODE_MENU: return 4;
    case S_INFO_MENU: return 5;
    default: return -1;
  }
}

DisplayModule::Screen DisplayModule::parentMenu(Screen screen) const {
  switch (screen) {
    case S_NFC_MENU:
    case S_IR_MENU:
    case S_JAMMER_MENU:
    case S_MODE_MENU:
    case S_INFO_MENU:
      return S_MENU;
    default:
      return S_MENU;
  }
}

const DisplayModule::MenuEntry* DisplayModule::menuFor(Screen screen,
                                                       int& count) const {
  static const MenuEntry mainMenu[] = {
      {"NFC / RFID", S_NFC_MENU},
      {"Infrarot", S_IR_MENU},
      {"2,4 GHz", S_JAMMER_MENU},
      {"No-Mercy", S_MODE_MENU},
      {"Info", S_INFO_MENU},
  };
  static const MenuEntry nfcMenu[] = {
      {"Lesen", S_NFC_MENU, "nfc", "read"},
      {"Zurueck", S_MENU},
  };
  static const MenuEntry irMenu[] = {
      {"Aufnehmen", S_IR_MENU, "ir", "record"},
      {"Stop", S_IR_MENU, "ir", "stop"},
      {"Zurueck", S_MENU},
  };
  static const MenuEntry jammerMenu[] = {
      {"Modul nicht verfuegbar", S_PLATZHALTER},
      {"Zurueck", S_MENU},
  };
  static const MenuEntry modeMenu[] = {
      {"No-Mercy AN", S_MODE_MENU, "system", "setMode", 0, "nomercy"},
      {"No-Mercy AUS", S_MODE_MENU, "system", "setMode", 0, "off"},
      {"Zurueck", S_MENU},
  };
  static const MenuEntry infoMenu[] = {
      {"WLAN / IP-Adresse", S_INFO_WIFI},
      {"Verbundene Geraete", S_INFO_DEVICES},
      {"Zurueck", S_MENU},
  };
  switch (screen) {
    case S_MENU:
      count = sizeof(mainMenu) / sizeof(mainMenu[0]);
      return mainMenu;
    case S_NFC_MENU:
      count = sizeof(nfcMenu) / sizeof(nfcMenu[0]);
      return nfcMenu;
    case S_IR_MENU:
      count = sizeof(irMenu) / sizeof(irMenu[0]);
      return irMenu;
    case S_JAMMER_MENU:
      count = sizeof(jammerMenu) / sizeof(jammerMenu[0]);
      return jammerMenu;
    case S_MODE_MENU:
      count = sizeof(modeMenu) / sizeof(modeMenu[0]);
      return modeMenu;
    case S_INFO_MENU:
      count = sizeof(infoMenu) / sizeof(infoMenu[0]);
      return infoMenu;
    default:
      count = 0;
      return nullptr;
  }
}

void DisplayModule::activateMenuEntry(const MenuEntry& entry) {
  if (entry.module == nullptr || entry.action == nullptr) {
    if (entry.target == S_INFO_WIFI || entry.target == S_INFO_DEVICES) {
      _returnTo = _screen;
    }
    startScreen(entry.target);
    return;
  }

  _returnTo = _screen;
  _pendingAction = entry;
  _hasPendingAction = true;
  startScreen(S_LADEN);
}

void DisplayModule::dispatchPendingAction() {
  const MenuEntry entry = _pendingAction;
  _hasPendingAction = false;

  bool accepted = false;
  String errorMessage;
  if (_router != nullptr) {
    JsonDocument params;
    JsonObject arguments = params.to<JsonObject>();
    if (entry.slot > 0) arguments["slot"] = entry.slot;
    if (entry.value != nullptr) arguments["mode"] = entry.value;
    accepted = _router->dispatch(entry.module, entry.action, arguments,
                                 &errorMessage);
  } else {
    Serial.println("[display] Befehl kann nicht weitergeleitet werden: Router fehlt.");
    errorMessage = "Befehlsrouter nicht bereit";
  }

  if (accepted && entry.module != nullptr && entry.action != nullptr) {
    if (strcmp(entry.module, "nfc") == 0 &&
        strcmp(entry.action, "read") == 0) {
      _message = "NFC-Suche gestartet";
    } else if (strcmp(entry.module, "ir") == 0 &&
               strcmp(entry.action, "record") == 0) {
      _message = "IR-Aufnahme gestartet; Sensor nicht pruefbar";
    } else if (strcmp(entry.module, "ir") == 0 &&
               strcmp(entry.action, "stop") == 0) {
      _message = "IR gestoppt";
    } else {
      _message = "Aktion ausgefuehrt";
    }
  } else {
    _message = errorMessage.length() > 0 ? errorMessage : "Aktion abgelehnt";
  }
  if (!accepted) {
    Serial.printf("[display] %s.%s wurde abgelehnt.\n", entry.module,
                  entry.action);
  }
  _messageUntil = millis() + (accepted ? 1800 : 3000);
  startScreen(S_MESSAGE);
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
    const int y = row * 16;
    if (entryIndex == _sel[index]) {
      u8g2_DrawBox(&_display, 0, y, 128, 15);
      u8g2_SetDrawColor(&_display, 0);
      u8g2_DrawStr(&_display, 6, y + 11, entries[entryIndex].label);
      u8g2_SetDrawColor(&_display, 1);
    } else {
      u8g2_DrawFrame(&_display, 0, y, 128, 15);
      u8g2_DrawStr(&_display, 6, y + 11, entries[entryIndex].label);
    }
  }
  u8g2_SendBuffer(&_display);
}

void DisplayModule::drawPlatzhalter() {
  u8g2_ClearBuffer(&_display);
  u8g2_SetFont(&_display, u8g2_font_6x10_tr);
  u8g2_DrawStr(&_display, 2, 18, "2,4-GHz-Modul");
  u8g2_DrawStr(&_display, 2, 34, "nicht eingebunden");
  u8g2_DrawStr(&_display, 2, 54, "Taste = zurueck");
  u8g2_SendBuffer(&_display);
}

void DisplayModule::drawInfoPage() {
  u8g2_ClearBuffer(&_display);
  u8g2_SetFont(&_display, u8g2_font_6x10_tr);
  if (_screen == S_INFO_WIFI) {
    u8g2_DrawStr(&_display, 2, 18, "WLAN / IP");
    u8g2_DrawStr(&_display, 2, 34, "Netzwerk folgt");
  } else {
    u8g2_DrawStr(&_display, 2, 18, "Verbundene Geraete");
    u8g2_DrawStr(&_display, 2, 34, "Netzwerk folgt");
  }
  u8g2_DrawStr(&_display, 2, 54, "Taste = zurueck");
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
    u8g2_DrawStr(&_display, 2, 14 + line * 14, text.c_str());
    remaining.remove(0, splitAt);
    while (remaining.startsWith(" ")) remaining.remove(0, 1);
  }
  u8g2_DrawStr(&_display, 2, 61, "Taste = zurueck");
  u8g2_SendBuffer(&_display);
}

void DisplayModule::drawWaterLine(float phase) {
  for (int x = 0; x < OLED_WIDTH; ++x) {
    const int y = kYellowHeight - 1 +
                  roundPixel(1.5f * sinf((x + phase) / 6.0f));
    u8g2_DrawPixel(&_display, x, y);
  }
}

void DisplayModule::initBubbles() {
  for (int i = 0; i < kBubbleCount; ++i) {
    _bubbleX[i] = random(8, 120);
    _bubbleY[i] = random(20, 64);
  }
}

void DisplayModule::drawBubbles() {
  for (int i = 0; i < kBubbleCount; ++i) {
    _bubbleY[i] -= 0.7f;
    _bubbleX[i] += sinf(_bubbleY[i] / 5.0f) * 0.3f;
    if (_bubbleY[i] < kYellowHeight + 2) {
      _bubbleY[i] = 63;
      _bubbleX[i] = random(8, 120);
    }
    u8g2_DrawCircle(&_display, roundPixel(_bubbleX[i]),
                    roundPixel(_bubbleY[i]), 1, U8G2_DRAW_ALL);
  }
}

void DisplayModule::drawSwim() {
  _swimX += _swimDirection * _swimSpeed;
  const float edgeDistance = kOrcaLength / 2 + 12;
  if (_swimX > OLED_WIDTH - edgeDistance) _swimDirection = -1;
  if (_swimX < edgeDistance) _swimDirection = 1;
  _swimPhase += 0.1f + 0.12f * _swimSpeed;

  const float y = 40 + 7 * sinf(_swimX / 20.0f);
  const float slope = (7.0f / 20.0f) * cosf(_swimX / 20.0f);
  const float heading = atan2f(_swimDirection * slope, _swimDirection);
  const float tailX = _swimX - cosf(heading) * kOrcaLength / 2;
  const float tailY = y - sinf(heading) * kOrcaLength / 2;

  u8g2_ClearBuffer(&_display);
  drawBubbles();
  drawWaterLine(_swimPhase * 8);
  u8g2_SetFont(&_display, u8g2_font_6x10_tr);
  u8g2_DrawStr(&_display, 2, 9, "Orca schwimmt");
  char buffer[10];
  snprintf(buffer, sizeof(buffer), "T:%.1f", _swimSpeed);
  u8g2_DrawStr(&_display, 96, 9, buffer);
  drawOrca(tailX, tailY, heading, _swimDirection, 0.0f, 0.35f, _swimPhase);
  u8g2_SendBuffer(&_display);
}

void DisplayModule::drawJump() {
  _jumpProgress += 0.012f;
  if (_jumpProgress > 1.35f) _jumpProgress = 0;

  u8g2_ClearBuffer(&_display);
  drawWaterLine(_jumpProgress * 200);
  if (_jumpProgress <= 1.0f) {
    const float p = _jumpProgress;
    const float centerX = 20 + 88 * p;
    const float centerY = 60 - 4 * 45 * p * (1 - p);
    const float heading = atan2f(-4 * 45 * (1 - 2 * p), 88.0f);
    const float tailX = centerX - cosf(heading) * kOrcaLength / 2;
    const float tailY = centerY - sinf(heading) * kOrcaLength / 2;
    drawOrca(tailX, tailY, heading, 1, 0.0f, 0.25f, _jumpProgress * 30);

    if ((p > 0.28f && p < 0.45f) || (p > 0.55f && p < 0.72f)) {
      const int splashX = p < 0.5f ? 53 : 75;
      for (int i = 0; i < 6; ++i) {
        u8g2_DrawPixel(&_display, splashX + random(-8, 9),
                       kYellowHeight - 1 - random(0, 9));
      }
    }
  }
  u8g2_SendBuffer(&_display);
}

void DisplayModule::drawLoad() {
  const unsigned long elapsed = millis() - _loadStart;
  float curl = elapsed / 1200.0f;
  if (curl > 1.0f) curl = 1.0f;
  curl = curl * curl * (3 - 2 * curl);
  if (elapsed > 1200) _loadAngle += 0.11f;

  const float radius = kOrcaLength / PI;
  const float tailX = 64 + radius * cosf(_loadAngle);
  const float tailY = 43 + radius * sinf(_loadAngle);
  const float heading = _loadAngle + PI / 2;
  const int percent = (elapsed / 60) % 101;

  u8g2_ClearBuffer(&_display);
  u8g2_SetFont(&_display, u8g2_font_6x10_tr);
  char buffer[20];
  snprintf(buffer, sizeof(buffer), "Laedt... %d%%", percent);
  u8g2_DrawStr(&_display, 2, 9, buffer);
  u8g2_DrawFrame(&_display, 0, 12, 128, 4);
  u8g2_DrawBox(&_display, 0, 12, percent * 128 / 100, 4);
  drawOrca(tailX, tailY, heading, 1, curl, 0.15f, elapsed / 120.0f);
  u8g2_SendBuffer(&_display);
}

void DisplayModule::drawInfo() {
  u8g2_ClearBuffer(&_display);
  u8g2_SetFont(&_display, u8g2_font_6x10_tr);
  u8g2_DrawStr(&_display, 2, 10, "INFO");
  u8g2_DrawStr(&_display, 2, 30, "Orca-Menue v1");
  u8g2_DrawStr(&_display, 2, 42, "ESP32 + SH1106");
  u8g2_DrawStr(&_display, 2, 54, "Taste = zurueck");
  u8g2_SendBuffer(&_display);
}

void DisplayModule::startScreen(Screen screen) {
  if (!isMenuScreen(screen) && isMenuScreen(_screen)) {
    _returnTo = _screen;
  }
  if (screen != S_MESSAGE) _message = "";
  _screen = screen;
  _lastFrame = 0;
  _needsRedraw = true;
  if (screen == S_SPRINGEN) _jumpProgress = 0;
  if (screen == S_LADEN) {
    _loadAngle = -PI / 2;
    _loadStart = millis();
  }
}

const char* DisplayModule::screenName() const {
  switch (_screen) {
    case S_MENU: return "menu";
    case S_NFC_MENU: return "nfc-menu";
    case S_IR_MENU: return "ir-menu";
    case S_JAMMER_MENU: return "jammer-menu";
    case S_MODE_MENU: return "mode-menu";
    case S_INFO_MENU: return "info-menu";
    case S_INFO_WIFI: return "info-wifi";
    case S_INFO_DEVICES: return "info-devices";
    case S_MESSAGE: return "message";
    case S_SCHWIMMEN: return "swim";
    case S_SPRINGEN: return "jump";
    case S_LADEN: return "loading";
    case S_INFO: return "info";
    case S_PLATZHALTER: return "placeholder";
    default: return "unknown";
  }
}

#include "DisplayModule.h"

#include "../../config.h"
#include <Wire.h>
#include <math.h>
#include <stdio.h>

namespace {
constexpr int kYellowHeight = 16;
constexpr int kOrcaSegments = 16;
constexpr float kOrcaLength = 44.0f;
constexpr int kBubbleCount = 5;
constexpr unsigned long kFrameMs = 30;
constexpr char kMenuItems[][16] = {
    "Orca schwimmt", "Orca springt", "Laden (Ring)", "Info"};
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

  u8g2_Setup_sh1106_i2c_128x64_noname_f(
      &_display, U8G2_R0, wireByteCallback, wireGpioAndDelayCallback);
  u8x8_SetI2CAddress(&_display.u8x8, OLED_I2C_ADDR << 1);
  u8g2_InitDisplay(&_display);
  u8g2_SetPowerSave(&_display, 0);

  randomSeed(micros());
  initBubbles();
  Serial.println("[display] Orca-Menue bereit.");
}

void DisplayModule::update() {
  if (!_ok) return;

  if (_screen == S_MENU) {
    if (_needsRedraw) {
      _needsRedraw = false;
      drawMenu();
    }
    return;
  }

  if (millis() - _lastFrame < kFrameMs) return;
  _lastFrame = millis();

  switch (_screen) {
    case S_SCHWIMMEN:
      drawSwim();
      break;
    case S_SPRINGEN:
      drawJump();
      break;
    case S_LADEN:
      drawLoad();
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

  if (_screen == S_MENU) {
    if (event == InputEvent::Up) {
      _selected = (_selected + 3) % 4;
      _needsRedraw = true;
    } else if (event == InputEvent::Down) {
      _selected = (_selected + 1) % 4;
      _needsRedraw = true;
    } else if (event == InputEvent::Click) {
      startScreen(static_cast<Screen>(_selected + 1));
    }
    return;
  }

  if (event == InputEvent::LongPress || event == InputEvent::Click) {
    _screen = S_MENU;
    _needsRedraw = true;
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
  obj["selection"] = _selected;
  obj["swimSpeed"] = _swimSpeed;
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

void DisplayModule::drawMenu() {
  u8g2_ClearBuffer(&_display);
  u8g2_SetFont(&_display, u8g2_font_6x12_tr);
  for (int i = 0; i < 4; ++i) {
    const int y = i * 16;
    if (i == _selected) {
      u8g2_DrawBox(&_display, 0, y, 128, 15);
      u8g2_SetDrawColor(&_display, 0);
      u8g2_DrawStr(&_display, 6, y + 11, kMenuItems[i]);
      u8g2_SetDrawColor(&_display, 1);
    } else {
      u8g2_DrawFrame(&_display, 0, y, 128, 15);
      u8g2_DrawStr(&_display, 6, y + 11, kMenuItems[i]);
    }
  }
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
  _screen = screen;
  _lastFrame = 0;
  if (screen == S_SPRINGEN) _jumpProgress = 0;
  if (screen == S_LADEN) {
    _loadAngle = -PI / 2;
    _loadStart = millis();
  }
}

const char* DisplayModule::screenName() const {
  switch (_screen) {
    case S_MENU: return "menu";
    case S_SCHWIMMEN: return "swim";
    case S_SPRINGEN: return "jump";
    case S_LADEN: return "loading";
    case S_INFO: return "info";
    default: return "unknown";
  }
}

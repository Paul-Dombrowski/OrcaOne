#pragma once

#include "../../core/Module.h"
#include "../../core/InputEvent.h"
#include <u8g2.h>

class DisplayModule : public Module {
public:
  enum Screen {
    S_MENU = 0,
    S_ORCA_MENU = 1,
    S_SCHWIMMEN,
    S_SPRINGEN,
    S_LADEN,
    S_INFO,
    S_PLATZHALTER
  };

  struct MenuEntry {
    const char* label;
    Screen target;
  };

  const char* name() const override { return "display"; }
  void begin() override;
  void update() override;
  void handleInput(InputEvent event);
  void statusJson(JsonObject obj) override;
  bool handleCommand(const String& action, JsonObjectConst params) override;

private:
  int roundPixel(float value) const;
  float bodyAngle(float heading, float curl, float wiggle, float phase, float s) const;
  void buildSpine(float tailX, float tailY, float heading, float curl,
                  float wiggle, float phase);
  void spinePoint(int i, float length, float side, int& x, int& y) const;
  void discSafe(int x, int y, int radius);
  void drawOrca(float tailX, float tailY, float heading, int side, float curl,
                float wiggle, float phase);
  const MenuEntry* menuFor(Screen screen, int& count) const;
  void drawMenu();
  void drawPlatzhalter();
  void drawWaterLine(float phase);
  void initBubbles();
  void drawBubbles();
  void drawSwim();
  void drawJump();
  void drawLoad();
  void drawInfo();
  void startScreen(Screen screen);
  const char* screenName() const;

  u8g2_t _display{};
  bool _ok = true;
  Screen _screen = S_MENU;
  bool _needsRedraw = true;

  float _orcaX[17]{};
  float _orcaY[17]{};
  float _orcaTheta[17]{};
  int _orcaSide = 1;

  unsigned long _lastFrame = 0;
  float _swimX = 64;
  int _swimDirection = 1;
  float _swimSpeed = 1.5f;
  float _swimPhase = 0;
  float _bubbleX[5]{};
  float _bubbleY[5]{};
  float _jumpProgress = 0;
  float _loadAngle = 0;
  unsigned long _loadStart = 0;
  int _sel[2]{};
  int _off[2]{};
  Screen _returnTo = S_MENU;
};

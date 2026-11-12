#pragma once

#include "../../core/Module.h"
#include "../../core/InputEvent.h"
#include <u8g2.h>

class CommandRouter;

// ============================================================================
//  DisplayModule - OLED-Menue (SH1106) + Drehgeber-Navigation (v3)
// ----------------------------------------------------------------------------
//  Leitidee: Eine Aktion FEUERT und der zugehoerige Screen BLEIBT stehen und
//  zeigt das Live-Ergebnis, bis der Nutzer per Long-Press zurueckgeht. Keine
//  Auto-Rueckkehr, kein kuenstlicher Ladebalken.
//
//  Jeder Screen hat einen garantierten Ausgang: Long-Press = Zurueck aus JEDEM
//  Screen; Ziel aus statischem Baum (parentOf). Live-Screens stoppen beim
//  Zurueckgehen automatisch die laufende Aktion (nfc.stop / ir.stop).
//
//  Eintragsarten (MenuEntry::kind):
//    KNav    - oeffnet Untermenue oder Info-Seite (target)
//    KList   - oeffnet eine DYNAMISCHE Liste (IR-Slots / NFC-Karten)
//    KAction - feuert EINEN Befehl, zeigt kurze Meldung, bleibt im Menue
//    KGoLive - feuert einen Befehl UND wechselt auf einen bleibenden Live-Screen
//    KToggle - schaltet um (Zustand im Label)
// ============================================================================
class DisplayModule : public Module {
public:
  enum Screen {
    S_MENU = 0,       // Hauptmenue
    S_IR_MENU,        // Untermenue Infrarot
    S_INFO_MENU,      // Untermenue Info
    S_JAM_MENU,       // Untermenue 2,4 GHz (WLAN / Bluetooth / Stop)
    S_JAM_STATUS,     // Live: Jammer-Status (Modus, Sweeps/s, Antennen)
    S_IR_SLOTS,       // dynamische Liste: aufgenommene IR-Slots -> senden
    S_NFC_CARDS,      // dynamische Liste: gespeicherte NFC-Karten -> ansehen
    S_NFC_READ,       // Live: Karte lesen (zeigt Daten, bleibt stehen)
    S_IR_RECORD,      // Live: IR-Aufnahme laeuft
    S_IR_SEND,        // Live: IR-Slot wird gesendet
    S_CARD_VIEW,      // Detailansicht einer gespeicherten Karte
    S_INFO_WIFI,      // Info-Seite WLAN/IP
    S_INFO_DEVICES,   // Info-Seite verbundene Geraete
    S_MESSAGE         // kurze Meldung (Bestaetigung/Fehler), Auto-Rueckkehr
  };

  enum EntryKind : uint8_t { KNav, KList, KAction, KGoLive, KToggle };

  struct MenuEntry {
    const char* label;
    EntryKind kind = KNav;
    Screen target = S_MENU;
    const char* module = nullptr;    // KAction/KGoLive/KToggle
    const char* action = nullptr;    // Befehl (KToggle: AN-Befehl)
    const char* actionOff = nullptr; // KToggle: AUS-Befehl
    const char* value = nullptr;     // optionaler Param "mode" (AN)
    const char* valueOff = nullptr;  // optionaler Param "mode" (AUS)
    int8_t toggleId = -1;            // KToggle: Index in _toggleState, sonst -1
  };

  const char* name() const override { return "display"; }
  void begin() override;
  void update() override;
  void setCommandRouter(CommandRouter* router) { _router = router; }
  void handleInput(InputEvent event);
  void statusJson(JsonObject obj) override;
  bool handleCommand(const String& action, JsonObjectConst params) override;

private:
  static constexpr int kMenuStateCount = 4;   // S_MENU, S_IR_MENU, S_INFO_MENU, S_JAM_MENU
  static constexpr int kToggleCount = 2;      // 2,4 GHz, No-Mercy
  static constexpr int kVisibleMenuRows = 4;
  static constexpr int kVisibleListRows = 3;  // Listen mit Titelzeile: 3 Zeilen
  static constexpr int kMaxListItems = 20;
  static constexpr unsigned long kRefreshMs = 1500;      // Selbst-Heil-Redraw
  static constexpr unsigned long kLiveRefreshMs = 150;   // Live-Screens fluessig

  // --- Screen-Kategorien ---
  bool isMenuScreen(Screen s) const;
  bool isListScreen(Screen s) const;
  bool isLiveScreen(Screen s) const;
  bool isInfoPage(Screen s) const;
  int menuIndex(Screen s) const;
  Screen parentOf(Screen s) const;

  // --- Navigation / Aktionen ---
  const MenuEntry* menuFor(Screen s, int& count) const;
  void activateMenuEntry(const MenuEntry& e);
  void activateListItem();
  void fireToggle(const MenuEntry& e);
  bool dispatchNow(const char* module, const char* action, const char* value,
                   String& err);
  void goBack();
  void stopLive(Screen s);
  void populateList(Screen s);
  void showMessage(const String& text, unsigned long ms);

  // --- Rendering ---
  void renderCurrent();
  void drawMenu();
  void drawDynamicList();
  void drawNfcRead();
  void drawIrRecord();
  void drawIrSend();
  void drawCardView();
  void drawInfoPage();
  void drawJamStatus();   // Live-Jammer-Status (liest jammer via Router::moduleStatus)
  void drawMessage();
  void drawListFrame(const char* title);  // Titelzeile + Trennlinie, gemeinsam

  void startScreen(Screen screen);
  const char* screenName() const;

  u8g2_t _display{};
  bool _ok = true;
  Screen _screen = S_MENU;
  bool _needsRedraw = true;
  unsigned long _lastDraw = 0;

  int _sel[kMenuStateCount]{};
  int _off[kMenuStateCount]{};
  bool _toggleState[kToggleCount]{};

  // Dynamische Liste (immer nur eine aktiv).
  int _listCount = 0;
  int _listSel = 0;
  int _listOff = 0;
  String _listLabel[kMaxListItems];
  String _listArg[kMaxListItems];

  String _cardJson;    // zwischengespeicherte Karte fuer S_CARD_VIEW
  int _sendSlot = 0;   // aktuell gesendeter Slot (fuer S_IR_SEND)

  Screen _originMenu = S_MENU;  // Rueckkehrziel fuer S_MESSAGE
  CommandRouter* _router = nullptr;
  String _message;
  unsigned long _messageUntil = 0;
};

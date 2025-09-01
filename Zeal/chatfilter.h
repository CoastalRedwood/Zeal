#pragma once
#include <functional>

#include "zeal_settings.h"

#define CHANNEL_MYPETDMG 1000
#define CHANNEL_OTHERPETDMG 1001
#define CHANNEL_MYPETSAY 1002
#define CHANNEL_OTHERPETSAY 1003
#define CHANNEL_MYMELEESPECIAL 1004
#define CHANNEL_OTHERMELEESPECIAL 1005
#define CHANNEL_MYSTATS 1006
#define CHANNEL_ITEMSPEECH 1007
#define CHANNEL_OTHER_MELEE_CRIT 1008
#define CHANNEL_OTHER_DAMAGE_SHIELD 1009

struct CustomFilter {
  std::string name;                     // String name - Appears in the Menu
  int channelMap;                       // Extended Channel Map ID - Zeal developer set
  Zeal::GameUI::ChatWnd *windowHandle;  // Window Handle - Maintains the currently filtered Chat Window handle
  std::function<bool(short &, std::string)> isHandled;

  // Default Constructor
  CustomFilter() : name(""), channelMap(0), windowHandle(nullptr), isHandled(nullptr) {
    // Optionally, add default lambda for isHandled
  }

  CustomFilter(const std::string &name, int channelMap, std::function<bool(short &, std::string)> isHandled)
      : name(name), channelMap(channelMap), isHandled(isHandled) {}

  ~CustomFilter() {}
};

struct damage_data {
  Zeal::GameStructures::Entity *source = nullptr;
  Zeal::GameStructures::Entity *target = nullptr;
  WORD type = 0;
  short spell_id = 0;
  short damage = 0;
};

class chatfilter {
 public:
  chatfilter(class ZealService *pHookWrapper);
  std::vector<CustomFilter> Extended_ChannelMaps;
  Zeal::GameUI::ContextMenu *ZealMenu = nullptr;
  void AddOutputText(Zeal::GameUI::ChatWnd *&wnd, std::string msg, short &channel);
  void LoadSettings(Zeal::GameUI::CChatManager *cm);
  void callback_clean_ui();
  void callback_hit(Zeal::GameStructures::Entity *source, Zeal::GameStructures::Entity *target, WORD type,
                    short spell_id, short damage, char output_text);
  ZealSetting<bool> setting_suppress_missed_notes = {false, "Zeal", "SuppressMissedNotes", false};
  ZealSetting<bool> setting_suppress_other_fizzles = {false, "Zeal", "SupressOtherFizzles", false};
  bool isExtendedCM(int channelMap, int applyOffset = 0);
  bool isStandardCM(int channelMap, int applyOffset = 0);
  int current_string_id = 0;
  bool isDamage = false;
  bool isMyPetSay = false;
  bool isPetMessage = false;
  int menuIndex = -1;
  damage_data damageData;
  ~chatfilter();
};

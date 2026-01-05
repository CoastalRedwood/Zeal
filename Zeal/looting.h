#pragma once
#include <Windows.h>

#include <string>
#include <vector>

#include "game_structures.h"
#include "game_ui.h"
#include "zeal_settings.h"

class Looting {
 public:
  void set_hide_looted(bool val);
  void link_all(const char *channel = nullptr) const;
  void init_ui();
  bool loot_all = false;
  ULONGLONG loot_next_item_time = 0;
  void looted_item();
  void handle_hide_looted();
  void set_last_hidden_corpse(Zeal::GameStructures::Entity *corpse);
  Looting(class ZealService *zeal);
  ~Looting();

  ZealSetting<bool> setting_alt_delimiter = {false, "Zeal", "LinkAllAltDelimiter", false};
  ZealSetting<bool> setting_ctrl_rightclick_loot = {false, "Zeal", "CtrlRightClickLoot", true};
  ZealSetting<bool> setting_hide_looted = {false, "Zeal", "HideLooted", false};
  ZealSetting<bool> setting_compact_linkall = {true, "Zeal", "CompactLinkAll", false};

  // /protect functionality.  Command line-only for now.
  bool is_cursor_protected(const Zeal::GameStructures::GAMECHARINFO *char_info) const;
  bool is_item_protected_from_selling(const Zeal::GameStructures::GAMEITEMINFO *item_info) const;
  bool is_trade_protected(struct Zeal::GameUI::TradeWnd *wnd) const;

  void add_options_callback(std::function<void()> callback) { update_options_ui_callback = callback; };

 protected:
  ZealSetting<bool> setting_protect_enable = {false, "Protect", "Enabled", true};
  ZealSetting<int> setting_protect_value = {10, "Protect", "Value", true};
  ZealSetting<int> setting_loot_last_item = {0, "Zeal", "LootLastItem", true};

  struct ProtectedItem {
    int id;
    std::string name;
  };

  bool parse_loot_last(const std::vector<std::string> &args);
  bool parse_protect(const std::vector<std::string> &args);
  void update_protected_item(int item_id, const std::string &name, bool add_only = false);
  void load_protected_items();
  void unhide_last_hidden();

  std::function<void()> update_options_ui_callback;
  std::vector<ProtectedItem> protected_items;
  Zeal::GameStructures::Entity *last_hidden_entity = nullptr;
  int last_hidden_spawnid = 0;

 private:
  int last_looted = -1;
};

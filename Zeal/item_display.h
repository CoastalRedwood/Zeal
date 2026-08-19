#pragma once
#include <unordered_map>
#include <vector>

#include "game_structures.h"
#include "game_ui.h"
#include "zeal_settings.h"

class ItemDisplay {
 public:
  ItemDisplay(class ZealService *pHookWrapper);
  ~ItemDisplay();
  Zeal::GameUI::ItemDisplayWnd *get_available_window(Zeal::GameStructures::GAMEITEMINFOBASE *item = nullptr);

  std::vector<Zeal::GameUI::ItemDisplayWnd *> get_windows() { return windows; }  // For short-term use only.

  bool close_latest_window();
  void add_to_cache(const Zeal::GameStructures::GAMEITEMINFO *item);
  const Zeal::GameStructures::GAMEITEMINFO *get_cached_item(int item_id) const;
  ZealSetting<bool> setting_enhanced_spell_info = {true, "Zeal", "EnhancedSpellInfo", false};
  ZealSetting<bool> setting_mouseover_tooltips = {true, "Zeal", "MouseoverTooltips", false};
  // 0 = no override; the tooltip window's native/default size is the max.
  ZealSetting<int> setting_mouseover_max_width = {0, "Zeal", "MouseoverMaxWidth", false};
  ZealSetting<int> setting_mouseover_max_height = {0, "Zeal", "MouseoverMaxHeight", false};
  void set_mouseover_tooltips(bool enabled);

 private:
  void InitUI();
  void CleanUI();
  void DeactivateUI();
  const int max_item_displays = 5;
  std::vector<Zeal::GameUI::ItemDisplayWnd *> windows;

  std::unordered_map<int, Zeal::GameStructures::GAMEITEMINFO> item_cache;  // Cache of all displayed items.
};

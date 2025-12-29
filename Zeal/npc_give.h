#pragma once
#include "game_structures.h"
#include "zeal_settings.h"

class NPCGive {
 public:
  NPCGive(class ZealService *pHookWrapper);
  ~NPCGive();
  void HandleItemInCursor();             // For internal callback use only.
  void HandleItemPickup(int from_slot);  // For internal callback use only.
  void ClearItem();                      // Reset the waiting for item flag.

  ZealSetting<bool> setting_enable_give = {false, "SingleClick", "EnableGive", false};
  ZealSetting<bool> setting_log_add_to_trade = {false, "Zeal", "LogAddToTrade", false};

 private:
  Zeal::GameStructures::GAMEITEMINFOBASE *wait_cursor_item = nullptr;  // Auto-move item on cursor.
  int bag_index = 0;  // /singleclick bag <#> with 1-8 valid values. 0 disables.
  void tick();
};

#pragma once

#include <functional>

#include "zeal_settings.h"

// Home for simple utility functions that extend the general game_functions and may require settings or hooks.
class Utils {
 public:
  explicit Utils(class ZealService *zeal);
  ~Utils();

  ZealSetting<int> setting_lock_toggle_bag_slot = {0, "Zeal", "LockToggleBagSlot", true};

  void handle_toggle_all_containers() const;

  void add_options_callback(std::function<void()> callback) { update_options_ui_callback = callback; };

 private:
  std::function<void()> update_options_ui_callback;
};

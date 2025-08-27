#pragma once
#include <Windows.h>

#include "zeal_settings.h"

enum strafe_direction { None, Left, Right };

class PlayerMovement {
 public:
  void handle_movement_binds(int cmd, bool key_down);
  void handle_spellcast_binds(int cmd);
  void handle_movement_keys(int dinput_code);
  // void set_spellbook_autostand(bool enabled);
  // bool spellbook_autostand = false;
  ZealSetting<bool> CastAutoStand = {true, "Zeal", "CastAutoStand", false};
  ZealSetting<bool> SpellBookAutoStand = {true, "Zeal", "SpellBookAutoStand", false};
  ZealSetting<bool> EnhancedAutoRun = {false, "Zeal", "EnhancedAutoRun", false};
  ZealSetting<bool> AutoFollowEnable = {false, "AutoFollow", "Enable", false,
                                        [this](bool val) { sync_auto_follow_enable(); }};
  ZealSetting<float> AutoFollowDistance = {15.f, "AutoFollow", "Distance", false,
                                           [this](bool val) { sync_auto_follow_enable(); }};
  PlayerMovement(class ZealService *zeal);

 private:
  void set_strafe(strafe_direction dir);
  void sync_auto_follow_enable(bool first_boot = false);  // Update when autofollow toggles.
  void handle_autofollow();
  void callback_main();
  strafe_direction current_strafe = strafe_direction::None;
  bool current_strafe_key_down_state = false;  // A bound strafe key is currently pressed.
  bool current_strafe_lock = false;            // Current strafing direction is locked on.
  BYTE orig_reset_strafe[7] = {0};
  DWORD last_follow_time_ms = 0;
};

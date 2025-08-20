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
  PlayerMovement(class ZealService *zeal);

 private:
  void set_strafe(strafe_direction dir);
  void callback_main();
  strafe_direction current_strafe = strafe_direction::None;
  bool current_strafe_key_down_state = false;  // A bound strafe key is currently pressed.
  bool current_strafe_lock = false;            // Current strafing direction is locked on.
  BYTE orig_reset_strafe[7] = {0};
};

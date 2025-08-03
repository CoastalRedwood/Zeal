#include "player_movement.h"

#include "game_addresses.h"
#include "string_util.h"
#include "zeal.h"

static void CloseSpellbook(void) {
  Zeal::Game::get_self()->ChangeStance(Stance::Stand);
  Zeal::Game::Windows->SpellBook->IsVisible = false;
}

void PlayerMovement::handle_movement_binds(int cmd, bool key_down) {
  if (!Zeal::Game::game_wants_input() && key_down) {
    // Reset strafing if forward/back keys are pressed down without a strafe key down.
    if ((cmd == 3 || cmd == 4) && !current_strafe_key_down_state && (current_strafe != strafe_direction::None))
      set_strafe(strafe_direction::None);
    if (!Zeal::Game::KeyMods->Alt && !Zeal::Game::KeyMods->Shift && !Zeal::Game::KeyMods->Ctrl) {
      if (Zeal::Game::is_new_ui()) {
        if (Zeal::Game::Windows->Loot && Zeal::Game::Windows->Loot->IsOpen && Zeal::Game::Windows->Loot->IsVisible) {
          Zeal::Game::GameInternal::CLootWndDeactivate((int)Zeal::Game::Windows->Loot, 0);
          return;
        } else if (Zeal::Game::Windows->SpellBook && Zeal::Game::Windows->SpellBook->IsVisible) {
          switch (cmd) {
            case 3:
              CloseSpellbook();
              break;
            case 4:
            case 5:
            case 6:
            case 211:
            case 212:
              if (!SpellBookAutoStand.get()) {
                return;
              }
              CloseSpellbook();
              break;
            default: {
              return;
            }
          }
        }
      } else {
        if (Zeal::Game::get_active_corpse()) {
          // how do we close corpse without closing the window?
          return;
        } else if (Zeal::Game::OldUI::spellbook_window_open()) {
          // left and right arrows dont turn pages on oldui by default
          // so I'm not sure if we'll add support for other keys
          if (cmd == 4) {
            return;
          } else if (cmd == 5 && !SpellBookAutoStand.get()) {
            return;
          } else if (cmd == 6 && !SpellBookAutoStand.get()) {
            return;
          }
        }
      }

      switch (Zeal::Game::get_controlled()->StandingState) {
        case Zeal::GameEnums::Stance::Sitting:
          Zeal::Game::get_controlled()->ChangeStance(Stance::Stand);
          break;
        default: {
          return;
        }
      }
    }
  }
}

void PlayerMovement::handle_spellcast_binds(int cmd) {
  if (!Zeal::Game::game_wants_input()) {
    if (Zeal::Game::is_new_ui()) {
      if (Zeal::Game::Windows->Loot && Zeal::Game::Windows->Loot->IsOpen && Zeal::Game::Windows->Loot->IsVisible) {
        return;
      } else if (Zeal::Game::Windows->SpellBook && Zeal::Game::Windows->SpellBook->IsVisible) {
        return;
      }
    } else {
      if (Zeal::Game::get_active_corpse()) {
        // how do we close corpse without closing the window?
        return;
      } else if (Zeal::Game::OldUI::spellbook_window_open()) {
        return;
      }
    }

    if (ZealService::get_instance()->movement->CastAutoStand.get() &&
        Zeal::Game::get_self()->StandingState == Zeal::GameEnums::Stance::Sitting)
      Zeal::Game::get_self()->ChangeStance(Stance::Stand);
  }
}

void PlayerMovement::handle_movement_keys(int dinput_code) {
  unsigned int key = dinput_code & 0xd00000ff;  // 0xd mask ignores ctrl key state.
  // These special movement keys bypass the input check.
  const unsigned int *numpad_8_up = (unsigned int *)0x007cd858;
  const unsigned int *numpad_2_down = (unsigned int *)0x007cd85c;
  const unsigned int *key_up = (unsigned int *)0x007cdc58;
  const unsigned int *key_down = (unsigned int *)0x007cdc5c;

  // Reset strafing if they are pressed down without a strafe key down.
  if ((key == *numpad_8_up || key == *numpad_2_down || key == *key_up || key == *key_down) &&
      !current_strafe_key_down_state && (current_strafe != strafe_direction::None))
    set_strafe(strafe_direction::None);
}

void PlayerMovement::set_strafe(strafe_direction dir) {
  // slightly revised so the game properly sets speed based on encumber and handles the stance checks
  current_strafe_lock = false;  // Always reset when setting (key is pressed, resetting).
  if (dir == strafe_direction::None || !Zeal::Game::can_move()) {
    current_strafe = strafe_direction::None;
    *Zeal::Game::strafe_direction = 0;
    *Zeal::Game::strafe_speed = 0;
    if (orig_reset_strafe[0] != 0) mem::copy(0x53f424, orig_reset_strafe, 7);
  } else {
    if (orig_reset_strafe[0] == 0)
      mem::set(0x53f424, 0x90, 7, orig_reset_strafe);
    else if (*(BYTE *)0x53f424 != 0x90)
      mem::set(0x53f424, 0x90, 7);

    if (dir == strafe_direction::Right) {
      current_strafe = strafe_direction::Right;
      *Zeal::Game::strafe_direction = 0x2;
    } else {
      current_strafe = strafe_direction::Left;
      *Zeal::Game::strafe_direction = 0x1;
    }
  }
}

void PlayerMovement::callback_main() {
  if (current_strafe != strafe_direction::None) {
    Zeal::GameStructures::Entity *controlled_player = Zeal::Game::get_controlled();
    *Zeal::Game::strafe_speed = 2.f;  // Default for mounts.
    if (controlled_player != Zeal::Game::get_self()) {
      float encumberance = Zeal::Game::encum_factor();
      *Zeal::Game::strafe_speed = encumberance + encumberance;
    }
    if (controlled_player->IsSneaking || controlled_player->StandingState == Stance::Duck ||
        (controlled_player->ActorInfo && controlled_player->ActorInfo->MovementSpeedModifier < 0))
      *Zeal::Game::strafe_speed *= .5f;
    if (controlled_player->ActorInfo && controlled_player->ActorInfo->Rider != 0) *Zeal::Game::strafe_speed *= .25f;
    if (controlled_player->ActorInfo && controlled_player->ActorInfo->MovementSpeedModifier < -1000.0f) {
      *Zeal::Game::strafe_direction = 0;
      *Zeal::Game::strafe_speed = 0;
    }
    if (controlled_player->StandingState != Stance::Duck && controlled_player->StandingState != Stance::Stand) {
      *Zeal::Game::strafe_direction = 0;
      *Zeal::Game::strafe_speed = 0;
    }
  }
}

// void PlayerMovement::load_settings()
//{
//	if (!ini_handle->exists("Zeal", "SpellbookAutostand"))
//		ini_handle->setValue<bool>("Zeal", "SpellbookAutostand", false);
//	//if (!ini_handle->exists("Zeal", "LeftStrafeSpellbookAutostand"))
//	//	ini_handle->setValue<bool>("Zeal", "LeftStrafeSpellbookAutostand", true);
//	//if (!ini_handle->exists("Zeal", "RightStrafeSpellbookAutostand"))
//	//	ini_handle->setValue<bool>("Zeal", "RightStrafeSpellbookAutostand", true);
//
//	SpellBookAutoStand = ini_handle->getValue<bool>("Zeal", "SpellbookAutostand");
//	/*spellbook_right_autostand = ini_handle->getValue<bool>("Zeal", "RightTurnSpellbookAutostand");
//	spellbook_left_strafe_autostand = ini_handle->getValue<bool>("Zeal", "LeftStrafeSpellbookAutostand");
//	spellbook_right_strafe_autostand = ini_handle->getValue<bool>("Zeal", "RightStrafeSpellbookAutostand");*/
// }
//

static int __fastcall CastSpell(void *this_ptr, void *not_used, unsigned char a1, short a2,
                                Zeal::GameStructures::GAMEITEMINFO **a3, short a4) {
  if (ZealService::get_instance()->movement->CastAutoStand.get() && Zeal::Game::get_self() &&
      Zeal::Game::get_self()->StandingState == Zeal::GameEnums::Stance::Sitting)
    Zeal::Game::get_self()->ChangeStance(Stance::Stand);
  return ZealService::get_instance()->hooks->hook_map["CastSpell"]->original(CastSpell)(this_ptr, not_used, a1, a2, a3,
                                                                                        a4);
}

static int ProcessMovementKeys(int dinput_code, int unknown) {
  ZealService::get_instance()->movement->handle_movement_keys(dinput_code);
  return ZealService::get_instance()->hooks->hook_map["ProcessMovementKeys"]->original(ProcessMovementKeys)(dinput_code,
                                                                                                            unknown);
}

PlayerMovement::PlayerMovement(ZealService *zeal) {
  zeal->hooks->Add("CastSpell", 0x004C483B, CastSpell, hook_type_detour);
  zeal->hooks->Add("ProcessMovementKeys", 0x005257fa, ProcessMovementKeys, hook_type_detour);
  Binds *binds = zeal->binds_hook.get();

  // Support enhanced auto-run: more consistent 'lock on' behavior including strafe support
  binds->replace_cmd(1, [this](int state) {
    // Change nothing on key release or if setting disabled.
    if (!state || !EnhancedAutoRun.get()) return false;

    bool auto_run_active = *reinterpret_cast<int *>(0x00798600) != 0;
    bool forwards_key_down = *reinterpret_cast<int *>(0x007ce058) != 0;
    if (forwards_key_down || (current_strafe_key_down_state && current_strafe != strafe_direction::None)) {
      // Lock strafing if currently active.
      current_strafe_lock = (current_strafe != strafe_direction::None);
      // Skip processing if already in auto-run mode so that it isn't toggled off.
      return auto_run_active;
    }

    if (auto_run_active) set_strafe(strafe_direction::None);  // Disables strafing when auto_run is toggling off.
    return false;                                             // Auto_run will toggle off in primary handler.
  });

  // ISSUE: Mapping LEFT/RIGHT arrow keys to strafe on TAKP2.1 client fails to function.
  binds->replace_cmd(211, [this](int state) {
    current_strafe_key_down_state = state;
    if (!Zeal::Game::game_wants_input()) {
      if (!state && current_strafe == strafe_direction::Left && !current_strafe_lock) {
        set_strafe(strafe_direction::None);
      } else if (state) {
        handle_movement_binds(211, state);
        set_strafe(strafe_direction::Left);
      }
    }
    return false;
  });  // strafe left
  binds->replace_cmd(212, [this](int state) {
    current_strafe_key_down_state = state;
    if (!Zeal::Game::game_wants_input()) {
      if (!state && current_strafe == strafe_direction::Right && !current_strafe_lock) {
        set_strafe(strafe_direction::None);
      } else if (state) {
        handle_movement_binds(212, state);
        set_strafe(strafe_direction::Right);
      }
    }
    return false;
  });  // strafe right

  zeal->commands_hook->Add("/autostand", {}, "Autostands when you cast.", [this](std::vector<std::string> &args) {
    if (args.size() == 1 || args.size() > 2) {
      Zeal::Game::print_chat("usage: /autostand spellbook");
      return true;
    }
    if (args.size() > 1) {
      std::ostringstream oss;
      if (Zeal::String::compare_insensitive(args[1], "spellbook")) {
        SpellBookAutoStand.toggle();
        std::string is_enabled = SpellBookAutoStand.get() ? "enabled" : "disabled";
        oss << "[Autostand] spellbook autostand has been " << is_enabled << "." << std::endl;
        Zeal::Game::print_chat(oss.str());
        return true;
      }
    }
    return false;
  });
  zeal->callbacks->AddGeneric([this]() { callback_main(); }, callback_type::MainLoop);
  zeal->callbacks->AddGeneric(
      [this]() {
        current_strafe_key_down_state = false;
        set_strafe(strafe_direction::None);  // Reset strafe logic including strafe_lock.
      },
      callback_type::InitUI);
}
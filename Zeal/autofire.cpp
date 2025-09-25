#include "autofire.h"

#include "callbacks.h"
#include "commands.h"
#include "game_addresses.h"
#include "game_functions.h"
#include "game_str.h"
#include "zeal.h"

static void suppress_autofire_fault_messages(bool flag) {
  ZealService::get_instance()->gamestr_hook->str_noprint[108] = flag;    // You cannot see your target.
  ZealService::get_instance()->gamestr_hook->str_noprint[123] = flag;    // You can't hit them from here.
  ZealService::get_instance()->gamestr_hook->str_noprint[124] = flag;    // Your target is too far away, get closer!
  ZealService::get_instance()->gamestr_hook->str_noprint[125] = flag;    // You can't attack while stunned!
  ZealService::get_instance()->gamestr_hook->str_noprint[12695] = flag;  // You can't attack while invulnerable!
  ZealService::get_instance()->gamestr_hook->str_noprint[12696] = flag;  // Try attacking someone other...
  ZealService::get_instance()->gamestr_hook->str_noprint[12698] = flag;  // Your target is too close...
  ZealService::get_instance()->gamestr_hook->str_noprint[12699] = flag;  // Your target is too far above/below...
}

void AutoFire::Main() {
  if (!Zeal::Game::get_target() || *(BYTE *)0x7f6ffe) {
    SetAutoFire(false);
    return;
  }
  static ULONGLONG last_print_time = GetTickCount64();
  if (autofire) {
    if (*(BYTE *)0x7cd844) {
      int range = 0;
      if (Zeal::Game::get_char_info()->Inventory.Ranged &&
          Zeal::Game::get_char_info()->Inventory.Ranged->Common.Skill == 0x5) {
        range += Zeal::Game::get_char_info()->Inventory.Ranged->Common.Range;
        if (Zeal::Game::get_char_info()->Inventory.Ammo)
          range += Zeal::Game::get_char_info()->Inventory.Ammo->Common.Range;
      } else if (Zeal::Game::get_char_info()->Inventory.Ranged && Zeal::Game::get_char_info()->Inventory.Ammo &&
                 Zeal::Game::get_char_info()->Inventory.Ammo->Common.Range) {
        range += Zeal::Game::get_char_info()->Inventory.Ammo->Common.Range;
      } else {
        Zeal::Game::print_chat("You do not have a ranged weapon");
        SetAutoFire(false);
      }

      // Suppress spam messages to only a 1 Hz rate.
      if (GetTickCount64() - last_print_time < 1000) {
        suppress_autofire_fault_messages(true);  // Disable fault reporting this cycle.
      } else {
        last_print_time = GetTickCount64();
      }

      if (Zeal::Game::CanIHitTarget(static_cast<float>(range))) {
        *(BYTE *)0x7cd844 = 0;
        Zeal::Game::do_attack(11, 0);
      }
      suppress_autofire_fault_messages(false);  // Always re-enable before exiting.
    }
  }
}

// bool __fastcall DoAttack(Zeal::GameStructures::Entity* player, int unused, uint8_t type, uint8_t p2,
// Zeal::GameStructures::Entity* target)
//{
//     if (player && target && ZealService::get_instance()->autofire->HandleDoAttack(player, type, p2, target))
//         return true;
//
//     return ZealService::get_instance()->hooks->hook_map["DoAttack"]->original(DoAttack)(player, unused, type, p2,
//     target);
// }

void AutoFire::SetAutoFire(bool enabled, bool do_print) {
  if (autofire && !enabled) {
    if (do_print) Zeal::Game::print_chat(USERCOLOR_ECHO_SHOUT, "Autofire disabled");
    Zeal::Game::SetMusicSelection(2, false);
  } else if (enabled) {
    Zeal::Game::do_autoattack(false);
    if (do_print) Zeal::Game::print_chat(USERCOLOR_ECHO_SHOUT, "Autofire enabled.");
    if (!(*(bool *)0x61d25c))  // combat music disabled flag
      Zeal::Game::SetMusicSelection(2, true);
  }
  autofire = enabled;
  do_autofire = false;
}

AutoFire::AutoFire(ZealService *zeal) {
  //  zeal->hooks->Add("DoAttack", 0x50A0F8, DoAttack, hook_type_detour);
  // zeal->callbacks->add_generic([this]() { SetAutoFire(false);  }, callback_type::Zone);
  zeal->callbacks->AddGeneric([this]() { SetAutoFire(false); }, callback_type::CharacterSelect);
  zeal->callbacks->AddGeneric([this]() { SetAutoFire(false); }, callback_type::EndMainLoop);
  zeal->callbacks->AddGeneric([this]() { SetAutoFire(false); }, callback_type::Zone);
  zeal->callbacks->AddGeneric([this]() { Main(); }, callback_type::MainLoop);
  // zeal->callbacks->add_packet([this](UINT opcode, char* buffer, UINT size) {
  //     if (opcode == 0x4161 || opcode == 0x4151)
  //     {
  //
  //     }
  //     return false;
  //    //Zeal::Game::print_chat("Opcode: 0x%x Size: %i Buffer: %s", opcode, size, byteArrayToHexString(buffer,
  //    size).c_str());
  //     //return false;
  // }, callback_type::SendMessage_);
  zeal->commands_hook->Add("/autofire", {"/af"}, "Toggles autofire for your ranged ability.",
                           [this](std::vector<std::string> &args) {
                             SetAutoFire(!autofire, true);
                             return true;  // return true to stop the game from processing any further on this command,
                                           // false if you want to just add features to an existing cmd
                           });
}

AutoFire::~AutoFire() {}

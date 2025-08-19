#include "autofire.h"

#include "callbacks.h"
#include "commands.h"
#include "game_addresses.h"
#include "game_functions.h"
#include "game_str.h"
#include "zeal.h"

// #include "hook_wrapper.h"
// #include "memory.h"

//
// bool AutoFire::HandleDoAttack(Zeal::GameStructures::Entity* player, uint8_t type, uint8_t p2,
// Zeal::GameStructures::Entity* target)
//{
//    Zeal::GameStructures::_GAMEITEMINFO* ranged_item = Zeal::Game::get_char_info()->Inventory.Ranged;
//    if (!autofire || !ranged_item)
//        return false;
//    static bool disabled_auto = false;
//    if (type == 13 || type == 14)
//    {
//        ZealService::get_instance()->gamestr_hook->str_noprint[124] = true; //don't print too far away messaging
//        if (!Zeal::Game::CanIHitTarget(0.f))
//        {
//            if (player->Position.Dist2D(target->Position) > 30.f )
//            {
//                if (!do_autofire)
//                {
//                    was_autoattacking = *(BYTE*)0x7f6ffe;
//                    *(BYTE*)0x7f6ffe = 0; //disable auto attack
//                    do_autofire = true;
//                }
//                else
//                {
//                    do_autofire = false;
//                }
//                ZealService::get_instance()->gamestr_hook->str_noprint[124] = false;
//                return true;
//            }
//        }
//        else
//        {
//            do_autofire = false;
//        }
//    }
//    return false;
//}

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

      if (GetTickCount64() - last_print_time < 1000) {
        ZealService::get_instance()->gamestr_hook->str_noprint[124] = true;
        ZealService::get_instance()->gamestr_hook->str_noprint[108] = true;
        ZealService::get_instance()->gamestr_hook->str_noprint[12695] = true;
        ZealService::get_instance()->gamestr_hook->str_noprint[12696] = true;
        ZealService::get_instance()->gamestr_hook->str_noprint[12698] = true;
        ZealService::get_instance()->gamestr_hook->str_noprint[12699] = true;
      } else {
        last_print_time = GetTickCount64();
      }

      if (Zeal::Game::CanIHitTarget(static_cast<float>(range))) {
        *(BYTE *)0x7cd844 = 0;
        Zeal::Game::do_attack(11, 0);
      }

      ZealService::get_instance()->gamestr_hook->str_noprint[124] = false;
      ZealService::get_instance()->gamestr_hook->str_noprint[108] = false;
      ZealService::get_instance()->gamestr_hook->str_noprint[12695] = false;
      ZealService::get_instance()->gamestr_hook->str_noprint[12696] = false;
      ZealService::get_instance()->gamestr_hook->str_noprint[12698] = false;
      ZealService::get_instance()->gamestr_hook->str_noprint[12699] = false;
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

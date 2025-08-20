#include "labels.h"

#include "commands.h"
#include "experience.h"
#include "game_addresses.h"
#include "game_functions.h"
#include "game_structures.h"
#include "game_ui.h"
#include "hook_wrapper.h"
#include "memory.h"
#include "tick.h"
#include "zeal.h"

void default_empty(Zeal::GameUI::CXSTR *str, bool *override_color, ULONG *color) {
  *override_color = 1;
  *color = 0xffc0c0c0;
  Zeal::Game::CXStr_PrintString(str, "");
}

bool GetLabelFromEq(int type, Zeal::GameUI::CXSTR *str, bool *override_color, ULONG *color) {
  ZealService *zeal = ZealService::get_instance();
  if (!Zeal::Game::is_in_game())
    return ZealService::get_instance()->hooks->hook_map["GetLabel"]->original(GetLabelFromEq)(type, str, override_color,
                                                                                              color);
  switch (type) {
    case 29:
      if (str && (!Zeal::Game::get_target() || Zeal::Game::get_target()->Type > 1)) {
        str->Set("");  // Clear the "0" when there is no target or a corpse.
        if (override_color) *override_color = false;
        *reinterpret_cast<int *>(0x00630984) = 0;  // Value is written to a global.
        return true;
      }
      break;
    case 80: {
      if (!zeal->experience && Zeal::Game::get_char_info()) return true;
      int max_mana = Zeal::Game::get_char_info()
                         ->max_mana();  //  Zeal::Game::GameInternal::get_max_mana(*Zeal::Game::ptr_LocalPC, 0);
      int mana =
          Zeal::Game::get_char_info()->mana();  // Zeal::Game::GameInternal::get_cur_mana(*Zeal::Game::ptr_LocalPC, 0);
      Zeal::Game::CXStr_PrintString(str, "%d/%d", mana, max_mana);
      *override_color = false;
      return true;
    }
    case 81: {
      if (!zeal->experience) return true;
      Zeal::Game::CXStr_PrintString(str, "%.f", zeal->experience->get_exp_per_hour_pct());
      *override_color = false;
      return true;
    }
    case 82: {
      Zeal::GameStructures::Entity *target = Zeal::Game::get_target();
      if (target && target->PetOwnerSpawnId > 0) {
        Zeal::GameStructures::Entity *owner = Zeal::Game::get_entity_by_id(target->PetOwnerSpawnId);
        if (owner) {
          Zeal::Game::CXStr_PrintString(str, "%s", owner->Name);
          *override_color = false;
        }
      } else {
        default_empty(str, override_color, color);
      }
      return true;
    }
    case 83:  // Number of empty inventory slots.
    {
      int num_empty = Zeal::Game::get_num_empty_inventory_slots();
      Zeal::Game::CXStr_PrintString(str, "%d", num_empty);
      *override_color = true;
      *color = (num_empty <= 0) ? 0xffff0000 : ((num_empty == 1) ? 0xffffff00 : 0xffc0c0c0);
      return true;
    }
    case 84:  // Total number of inventory slots.
      Zeal::Game::CXStr_PrintString(str, "%d", Zeal::Game::get_num_inventory_slots());
      *override_color = false;
      return true;
    case 85:  // Number of filled inventory slots.
    {
      int total_num = Zeal::Game::get_num_inventory_slots();
      int num_empty = Zeal::Game::get_num_empty_inventory_slots();
      Zeal::Game::CXStr_PrintString(str, "%d", total_num - num_empty);
      *override_color = true;
      *color = (num_empty <= 0) ? 0xffff0000 : ((num_empty == 1) ? 0xffffff00 : 0xffc0c0c0);
      return true;
    }
    case 86: {
      if (!zeal->experience) return true;
      Zeal::Game::CXStr_PrintString(str, "%.f", zeal->experience->get_aa_exp_per_hour_pct());
      *override_color = false;
      return true;
    }
    case 124: {
      if (Zeal::Game::get_char_info()) Zeal::Game::CXStr_PrintString(str, "%d", Zeal::Game::get_char_info()->mana());
      *override_color = false;
      return true;
    }
    case 125: {
      if (Zeal::Game::get_char_info())
        Zeal::Game::CXStr_PrintString(str, "%d", Zeal::Game::get_char_info()->max_mana());
      *override_color = false;
      return true;
    }
    case 134: {
      auto controlled = Zeal::Game::get_controlled();
      auto actor = controlled ? controlled->ActorInfo : nullptr;
      if (actor && actor->Rider && controlled->Race == 0xd8)  // Horse with a rider.
        actor = actor->Rider->ActorInfo;
      if (actor && actor->CastingSpellId) {
        int spell_id = actor->CastingSpellId;
        if (spell_id == kInvalidSpellId) spell_id = 0;  // avoid crash while player is not casting a spell
        Zeal::GameStructures::SPELL *casting_spell = Zeal::Game::get_spell_mgr()->Spells[spell_id];
        Zeal::Game::CXStr_PrintString(str, "%s", casting_spell->Name);
        *override_color = false;
      }
      return true;
    }
    case 135:  // Buff 16
    case 136:  // Buff 17
    case 137:  // Buff 18
    case 138:  // Buff 19
    case 139:  // Buff 20
    case 140:  // Buff 21
    case 141:  // Buff 22
    case 142:  // Buff 23
    case 143:  // Buff 24
    case 144:  // Buff 25
    case 145:  // Buff 26
    case 146:  // Buff 27
    case 147:  // Buff 28
    case 148:  // Buff 29
    case 149:  // Buff 30
      break;   // Reserved - These labels are supported by the game.dll
    case 255:  // debug label
    {
      Zeal::Game::CXStr_PrintString(str, "%s", ZealService::get_instance()->labels_hook->debug_info.c_str());
      ZealService::get_instance()->labels_hook->debug_info = "";
      *override_color = false;
      return true;
    }
    default:
      break;
  }
  return ZealService::get_instance()->hooks->hook_map["GetLabel"]->original(GetLabelFromEq)(type, str, override_color,
                                                                                            color);
}

static int get_remaining_cast_recovery_time() {
  auto self = Zeal::Game::get_self();
  auto actor_info = self ? self->ActorInfo : nullptr;
  auto display = Zeal::Game::get_display();
  if (!self || !actor_info || !display) return 0;

  DWORD game_time = display->GameTimeMs;
  if (actor_info->FizzleTimeout <= game_time) return 0;  // Idle state.
  return actor_info->FizzleTimeout - game_time;
}

static int get_attack_timer_gauge(Zeal::GameUI::CXSTR *str) {
  // Logic for the attack recovery timer was copied from DoProcessTime() which sets 0x007cd844.
  auto self = Zeal::Game::get_self();
  auto actor_info = self ? self->ActorInfo : nullptr;
  auto char_info = Zeal::Game::get_char_info();
  bool ready_to_attack = *reinterpret_cast<bool *>(0x007cd844);  // 0 = attacking, 1 = ready.
  if (!self || !actor_info || !char_info || ready_to_attack) {
    if (str) str->Set("");
    return 0;
  }

  UINT range_delay = 0;
  bool is_bow = false;
  if (actor_info->LastAttack == 11) {  // Ranged.
    // Calculate ranged time.
    auto range_item = char_info->InventoryItem[10];  // Ranged slot.
    if (range_item && range_item->Common.AttackDelay) {
      if (range_item->Common.Skill < 6 || range_item->Common.Skill == 0xd)
        range_delay = range_item->Common.AttackDelay * 100;
      else if (range_item->Common.Skill == 0x16)
        range_delay = reinterpret_cast<UINT **>(0x007f7aec)[range_item->Common.AttackDelay][1];
      is_bow = range_delay && (range_item->Common.Skill == 5);
    }
  }

  UINT attack_delay = range_delay;  // Use range_delay if it was set non-zero above.
  if (attack_delay == 0) {
    auto primary_item = char_info->InventoryItem[12];        // Primary slot.
    if (!primary_item || !primary_item->Common.AttackDelay)  // No weapon or not a weapon.
      attack_delay = Zeal::Game::get_hand_to_hand_delay() * 100;
    else if (primary_item->Common.Skill < 6 || primary_item->Common.Skill == 0x2d)  // Uses patched 0x2d, not 0xd.
      attack_delay = primary_item->Common.AttackDelay * 100;
    else if (primary_item->Common.Skill == 0x16)  // Hand-to-hand skilldict lookup.
      attack_delay = reinterpret_cast<UINT **>(0x007f7aec)[primary_item->Common.AttackDelay][1];
  }

  if (attack_delay) attack_delay = self->ModifyAttackSpeed(attack_delay, is_bow);

  UINT delay_time = Zeal::Game::get_game_time() - actor_info->AttackTimer;
  if (attack_delay == 0 || attack_delay <= delay_time) {
    if (str) str->Set("");
    return 0;
  }

  int time_left = attack_delay - delay_time;
  if (str) {
    int secs_left = (time_left + 999) / 1000;  // Show 3, 2, 1, 0 countdown effectively.
    Zeal::Game::CXStr_PrintString(str, "%i", secs_left);
  }

  const int full_duration = attack_delay;  // Use 4 seconds as the normalization factor.
  return max(0, min(1000, 1000 * time_left / attack_delay));
}

static int get_recast_time_gauge(int index, Zeal::GameUI::CXSTR *str) {
  bool invalid_index = index < 0 || index >= GAME_NUM_SPELL_GEMS;

  auto self = Zeal::Game::get_self();
  auto actor_info = self ? self->ActorInfo : nullptr;
  auto char_info = Zeal::Game::get_char_info();
  auto display = Zeal::Game::get_display();
  if (invalid_index || !self || !actor_info || !char_info || !display) {
    if (str) str->Set("0");
    return 0;
  }

  // Empty gauge if recast timeout is < current game time or the fizzle timeout (GCD).
  int game_time = display->GameTimeMs;
  int spell_id = char_info->MemorizedSpell[index];
  if (!Zeal::Game::Spells::IsValidSpellIndex(spell_id) || actor_info->CastingSpellId == spell_id ||
      actor_info->RecastTimeout[index] <= game_time || actor_info->RecastTimeout[index] <= actor_info->FizzleTimeout) {
    if (str) str->Set("0");
    return 0;
  }

  int time_left = actor_info->RecastTimeout[index] - game_time;
  if (str) {
    int secs_left = (time_left + 500) / 1000;
    Zeal::Game::CXStr_PrintString(str, "%i", secs_left);
  }

  auto sp_mgr = Zeal::Game::get_spell_mgr();
  int full_duration = sp_mgr ? sp_mgr->Spells[spell_id]->RecastTime : 0;
  full_duration = max(1000, full_duration);  // Ensure non-zero and reasonable number.
  return max(0, min(1000, 1000 * time_left / full_duration));
}

int GetGaugeFromEq(int type, Zeal::GameUI::CXSTR *str) {
  ZealService *zeal = ZealService::get_instance();
  switch (type) {
    case 23: {
      if (!zeal->experience)  // possible nullptr crash (race condition)
        return 0;
      float fpct = zeal->experience->get_exp_per_hour_pct() / 100.f;
      return (int)(std::clamp(1000.f * fpct, 0.f, 1000.f));
    }
    case 24:  // Server Tick
    {
      if (zeal->tick) return zeal->tick->GetTickGauge(str);
      return 0;
    }
    case 25:  // Global cast recovery gauge.
    {
      static constexpr int kNominalMaxRecoveryTime = 2500;  // In milliseconds.
      int recovery_time = get_remaining_cast_recovery_time();
      int value = max(0, min(1000, recovery_time * 1000 / kNominalMaxRecoveryTime));
      Zeal::Game::CXStr_PrintString(str, "%i", (recovery_time + 500) / 1000);
      return value;
    }
    case 26:  // Spell0 recast time.
    case 27:  // Spell1 recast time.
    case 28:  // Spell2 recast time.
    case 29:  // Spell3 recast time.
    case 30:  // Spell4 recast time.
    case 31:  // Spell5 recast time.
    case 32:  // Spell6 recast time.
    case 33:  // Spell7 recast time.
      return get_recast_time_gauge(type - 26, str);
    case 34:  // Attack recovery timer.
      return get_attack_timer_gauge(str);
    case 35: {
      if (!zeal->experience)  // possible nullptr crash (race condition)
        return 0;
      float fpct = zeal->experience->get_aa_exp_per_hour_pct() / 100.f;
      return (int)(std::clamp(1000.f * fpct, 0.f, 1000.f));
    }
    default:
      break;
  }

  int result = ZealService::get_instance()->hooks->hook_map["GetGauge"]->original(GetGaugeFromEq)(type, str);

  switch (type) {
    case 11:  // Intercept the player HP gauges (group window typically) to tag the leader.
    case 12:
    case 13:
    case 14:
    case 15: {
      const Zeal::GameStructures::GroupInfo *group_info = Zeal::Game::GroupInfo;
      if (group_info->is_in_group() && strcmp(str->CastToCharPtr(), group_info->LeaderName) == 0) {
        std::string name = std::string(*str) + "*";
        str->Set(name.c_str());
      }
    } break;
    default:
      break;
  }
  return result;
}

void Labels::print_debug_info(std::string data) { debug_info = data; }

void Labels::print_debug_info(const char *format, ...) {
  va_list argptr;
  char buffer[512];
  va_start(argptr, format);
  // printf()
  vsnprintf(buffer, 511, format, argptr);
  va_end(argptr);
  if (debug_info.length() > 0)
    debug_info += "\n" + std::string(buffer);
  else
    debug_info += std::string(buffer);
}

void Labels::callback_main() {}

bool Labels::GetLabel(int type, std::string &str) {
  Zeal::GameUI::CXSTR tmp("");
  bool override = false;
  ULONG color = 0;
  bool val = GetLabelFromEq(type, (Zeal::GameUI::CXSTR *)&tmp, &override, &color);
  if (tmp.Data) {
    str = std::string(tmp);
    tmp.FreeRep();
  }
  return val;
}

int Labels::GetGauge(int type, std::string &str) {
  Zeal::GameUI::CXSTR tmp("");
  int value = GetGaugeFromEq(type, (Zeal::GameUI::CXSTR *)&tmp);
  if (tmp.Data) {
    str = std::string(tmp);
    tmp.FreeRep();
  }
  return value;
}

Labels::~Labels() {}

Labels::Labels(ZealService *zeal) {
  zeal->commands_hook->Add("/labels", {}, "prints all labels", [this](std::vector<std::string> &args) {
    for (int i = 0; i < 200; i++) {
      Zeal::GameUI::CXSTR tmp("");
      bool override = false;
      ULONG color = 0;
      GetLabelFromEq(i, (Zeal::GameUI::CXSTR *)&tmp, &override, &color);
      if (tmp.Data) {
        Zeal::Game::print_chat("label: %i value: %s", i, tmp.CastToCharPtr());
        tmp.FreeRep();
      }
    }
    return true;  // return true to stop the game from processing any further on this command,
                  // false if you want to just add features to an existing cmd
  });
  // zeal->callbacks->add_generic([this]() { callback_main(); }); //causes a crash because callback_main is empty
  // zeal->hooks->Add("FinalizeLoot", Zeal::Game::GameInternal::fn_finalizeloot, finalize_loot, hook_type_detour);
  zeal->hooks->Add("GetLabel", Zeal::Game::GameInternal::fn_GetLabelFromGame, GetLabelFromEq, hook_type_detour);
  zeal->hooks->Add("GetGauge", Zeal::Game::GameInternal::fn_GetGaugeLabelFromGame, GetGaugeFromEq, hook_type_detour);
}

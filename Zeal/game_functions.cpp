#include "game_functions.h"

#include <Windows.h>

#include <array>
#include <regex>

#include "callbacks.h"
#include "entity_manager.h"
#include "game_addresses.h"
#include "hook_wrapper.h"
#include "memory.h"
#include "ui_manager.h"
#include "zeal.h"

static DWORD get_color_index_option(int index, DWORD defacto) {
  auto zeal = ZealService::get_instance();
  if (zeal->ui && zeal->ui->options) return zeal->ui->options->GetColor(index);
  if (defacto != 0) return defacto;
  return Zeal::Game::get_user_color(index);  // TODO: Verify index mapping.
}

static DWORD get_con_white() { return get_color_index_option(15, 0xfff0f0f0); }

static DWORD get_con_red() { return get_color_index_option(17, 0xfff00000); }

static DWORD get_con_blue() { return get_color_index_option(14, 0xff0040f0); }  // Slightly lightened.

static DWORD get_con_yellow() { return get_color_index_option(16, 0xfff0f000); }

static DWORD get_con_light_blue() { return get_color_index_option(13, 0xff00f0f0); }

static DWORD get_con_green() { return get_color_index_option(12, 0xff00f000); }

namespace Zeal {
namespace Game {
int GetSpellCastingTime()  // GetSpellCastingTime() in client.
{
  return reinterpret_cast<int(__cdecl *)(void)>(0x00435f28)();
}

DWORD GetLevelCon(Zeal::GameStructures::Entity *ent) {
  if (!ent || !Zeal::Game::get_self()) return 0;
  int mylevel = Zeal::Game::get_self()->Level;
  short diff = ent->Level - mylevel;
  DWORD conlevel = 0;

  if (diff == 0)
    return get_con_white();
  else if (diff >= 1 && diff <= 2)
    return get_con_yellow();
  else if (diff >= 3)
    return get_con_red();

  if (mylevel <= 7) {
    if (diff <= -4)
      conlevel = get_con_green();
    else
      conlevel = get_con_blue();  // Zeal::Game::get_user_color(70);
  } else if (mylevel <= 8) {
    if (diff <= -5)
      conlevel = get_con_green();
    else if (diff <= -4)
      conlevel = get_con_light_blue();
    else
      conlevel = get_con_blue();
  } else if (mylevel <= 12) {
    if (diff <= -6)
      conlevel = get_con_green();
    else if (diff <= -4)
      conlevel = get_con_light_blue();
    else
      conlevel = get_con_blue();
  } else if (mylevel <= 16) {
    if (diff <= -7)
      conlevel = get_con_green();
    else if (diff <= -5)
      conlevel = get_con_light_blue();
    else
      conlevel = get_con_blue();
  } else if (mylevel <= 20) {
    if (diff <= -8)
      conlevel = get_con_green();
    else if (diff <= -6)
      conlevel = get_con_light_blue();
    else
      conlevel = get_con_blue();
  } else if (mylevel <= 24) {
    if (diff <= -9)
      conlevel = get_con_green();
    else if (diff <= -7)
      conlevel = get_con_light_blue();
    else
      conlevel = get_con_blue();
  } else if (mylevel <= 28) {
    if (diff <= -10)
      conlevel = get_con_green();
    else if (diff <= -8)
      conlevel = get_con_light_blue();
    else
      conlevel = get_con_blue();
  } else if (mylevel <= 30) {
    if (diff <= -11)
      conlevel = get_con_green();
    else if (diff <= -9)
      conlevel = get_con_light_blue();
    else
      conlevel = get_con_blue();
  } else if (mylevel <= 32) {
    if (diff <= -12)
      conlevel = get_con_green();
    else if (diff <= -9)
      conlevel = get_con_light_blue();
    else
      conlevel = get_con_blue();
  } else if (mylevel <= 36) {
    if (diff <= -13)
      conlevel = get_con_green();
    else if (diff <= -10)
      conlevel = get_con_light_blue();
    else
      conlevel = get_con_blue();
  } else if (mylevel <= 40) {
    if (diff <= -14)
      conlevel = get_con_green();
    else if (diff <= -11)
      conlevel = get_con_light_blue();
    else
      conlevel = get_con_blue();
  } else if (mylevel <= 44) {
    if (diff <= -16)
      conlevel = get_con_green();
    else if (diff <= -12)
      conlevel = get_con_light_blue();
    else
      conlevel = get_con_blue();
  } else if (mylevel <= 48) {
    if (diff <= -17)
      conlevel = get_con_green();
    else if (diff <= -13)
      conlevel = get_con_light_blue();
    else
      conlevel = get_con_blue();
  } else if (mylevel <= 52) {
    if (diff <= -18)

      conlevel = get_con_green();
    else if (diff <= -14)
      conlevel = get_con_light_blue();
    else
      conlevel = get_con_blue();
  } else if (mylevel <= 54) {
    if (diff <= -19)

      conlevel = get_con_green();
    else if (diff <= -15)
      conlevel = get_con_light_blue();
    else
      conlevel = get_con_blue();
  } else if (mylevel <= 56) {
    if (diff <= -20)

      conlevel = get_con_green();
    else if (diff <= -15)
      conlevel = get_con_light_blue();
    else
      conlevel = get_con_blue();
  } else if (mylevel <= 60) {
    if (diff <= -21)
      conlevel = get_con_green();
    else if (diff <= -16)
      conlevel = get_con_light_blue();
    else
      conlevel = get_con_blue();
  } else if (mylevel <= 61) {
    if (diff <= -19)
      conlevel = get_con_green();
    else if (diff <= -14)
      conlevel = get_con_light_blue();
    else
      conlevel = get_con_blue();
  } else if (mylevel <= 62) {
    if (diff <= -17)
      conlevel = get_con_green();
    else if (diff <= -12)
      conlevel = get_con_light_blue();
    else
      conlevel = get_con_blue();
  } else {
    if (diff <= -16)
      conlevel = get_con_green();
    else if (diff <= -11)
      conlevel = get_con_light_blue();
    else
      conlevel = get_con_blue();
  }

  return conlevel;
}

bool IsPlayableRace(WORD race) {
  switch (race) {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 128:
    case 130:
      return true;
  }
  return false;
}

// See server common/patches/mac_limits.h namespace invslot for valid slot numbers. 0 = Cursor.
void move_item(int from_slot, int to_slot, int print_error, int a3) {
  reinterpret_cast<bool(__thiscall *)(int t, int a1, int slot, int a2, int a3)>(0x422b1c)(*(int *)0x63d6b4, from_slot,
                                                                                          to_slot, print_error, a3);
}

bool is_on_ground(Zeal::GameStructures::Entity *ent) {
  if (ent->ActorInfo) {
    return (ent->Position.z - ent->ModelHeightOffset + ent->MovementSpeedZ) <= (ent->ActorInfo->Z + 0.5 + 0.001);
  }
  return true;
}

char *get_string(UINT id) {
  return reinterpret_cast<char *(__thiscall *)(int t, UINT id, bool *)>(0x550EFE)(*(int *)0x7f9490, id, nullptr);
}

float heading_to_yaw(float heading) {
  float y = 512 - heading;
  y *= 0.703125f;
  if (y < 0) y += 360;
  return y;
}

std::string equipSlotToString(int slot) {
  switch (slot) {
    case 0:
      return "LeftEar";
    case 1:
      return "Head";
    case 2:
      return "Face";
    case 3:
      return "RightEar";
    case 4:
      return "Neck";
    case 5:
      return "Shoulder";
    case 6:
      return "Arms";
    case 7:
      return "Back";
    case 8:
      return "LeftWrist";
    case 9:
      return "RightWrist";
    case 10:
      return "Range";
    case 11:
      return "Hands";
    case 12:
      return "Primary";
    case 13:
      return "Secondary";
    case 14:
      return "LeftFinger";
    case 15:
      return "RightFinger";
    case 16:
      return "Chest";
    case 17:
      return "Legs";
    case 18:
      return "Feet";
    case 19:
      return "Waist";
    case 20:
      return "Ammo";
    default:
      return "Unknown";
  }
}

bool can_use_item(Zeal::GameStructures::GAMECHARINFO *c, Zeal::GameStructures::GAMEITEMINFO *item) {
  using FunctionType2 =
      bool(__thiscall *)(Zeal::GameStructures::GAMECHARINFO * char_info, Zeal::GameStructures::GAMEITEMINFO * iItem);
  FunctionType2 can_use_item = reinterpret_cast<FunctionType2>(0x4BB8E8);
  return can_use_item(c, item);
}

bool can_item_equip_in_slot(Zeal::GameStructures::GAMECHARINFO *c, const Zeal::GameStructures::GAMEITEMINFO *item,
                            int slot) {
  using FunctionType = bool(__cdecl *)(Zeal::GameStructures::GAMECHARINFO * char_info, UINT equipable_slots, UINT slot,
                                       const Zeal::GameStructures::GAMEITEMINFO *iItem);
  FunctionType check_loc = reinterpret_cast<FunctionType>(0x4F0DB4);
  return check_loc(c, item->EquipableSlots, slot, item);
}

bool can_equip_item(Zeal::GameStructures::GAMEITEMINFO *item) {
  Zeal::GameStructures::GAMECHARINFO *c = Zeal::Game::get_char_info();
  if (!item || !c) return false;
  if (!can_use_item(c, item)) return false;
  for (int i = 0; i < GAME_NUM_INVENTORY_SLOTS; i++) {
    if (can_item_equip_in_slot(c, item, i + 1) && !c->InventoryItem[i]) {
      // print_chat("equipable? slot: %i  %s   %i", i, equipSlotToString(i).c_str(), c->InventoryItem[i]);
      return true;
    }
  }
  return false;
}

bool can_stack(Zeal::GameStructures::GAMEITEMINFO *base_item, Zeal::GameStructures::GAMEITEMINFO *added_item) {
  if (!base_item || !added_item) return false;
  if (base_item->ID == added_item->ID && base_item->Common.IsStackable) {
    if (base_item->Common.StackCount + added_item->Common.StackCount <= 20) return true;
  }
  return false;
}

bool can_inventory_item(Zeal::GameStructures::GAMEITEMINFO *item) {
  Zeal::GameStructures::GAMECHARINFO *c = Zeal::Game::get_char_info();
  if (!item || !c) return false;

  if (can_equip_item(item)) return true;

  for (int i = 0; i < GAME_NUM_INVENTORY_PACK_SLOTS; i++) {
    Zeal::GameStructures::_GAMEITEMINFO *inventory_item = c->InventoryPackItem[i];
    if (!inventory_item) return true;

    if (inventory_item && inventory_item->Type == 1 && inventory_item->Container.Capacity > 0 &&
        inventory_item->Container.SizeCapacity >= item->Size && item->Type != 1) {
      if (can_stack(inventory_item, item)) return true;
      for (int b = 0; b < inventory_item->Container.Capacity; b++) {
        Zeal::GameStructures::_GAMEITEMINFO *bag_item = inventory_item->Container.Item[b];
        if (!bag_item) return true;
        if (can_stack(bag_item, item)) return true;
      }
    }
  }
  return false;
}

Zeal::GameStructures::ActorLocation get_actor_location(int actor) {
  DWORD addr = *(DWORD *)0x7f99c8;  // game pointer to function
  Zeal::GameStructures::ActorLocation actor_loc{};
  int *r = (int *)&actor_loc;
  __asm
  {
				push r
				push actor
				call addr
				add esp, 0x8
  }

  return actor_loc;
}

bool show_context_menu() {
  int ctx = GameInternal::CXWndShowContextMenu(*(int *)Game::WndManager, 0, *(int *)0x8092e8, *(int *)0x8092ec);
  return ctx;
}

GameUI::CXWndManager *get_wnd_manager() { return *Game::WndManager; }

bool is_gui_visible() {
  return *(reinterpret_cast<int *>(0x0063b918)) != 3;  // ScreenMode == 3 when F10 is pressed.
}

bool is_game_ui_window_hovered() {
  GameUI::CXWndManager *mgr = *Game::WndManager;
  if (mgr)
    return mgr->Hovered != 0;
  else
    return false;
}

bool game_wants_input() {
  int chat_input = GameInternal::UI_ChatInputCheck();
  bool focused_window_needs_input = false;
  if (is_new_ui()) {
    int focused_wnd = GameInternal::GetFocusWnd(*(int *)Game::WndManager, 0);
    if (focused_wnd) focused_window_needs_input = GameInternal::CXWndIsType(focused_wnd, 0, 2);
  }
  return chat_input != 0 || focused_window_needs_input;
}

Vec3 get_ent_head_pos(Zeal::GameStructures::Entity *ent) {
  Vec3 head_pos = ent->Position;
  head_pos.z += ent->Height;
  return head_pos;
}

Vec3 get_player_head_pos() {
  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  Vec3 head_pos = self->ActorInfo->DagHead->Position;
  return head_pos;
}

float encum_factor() {
  if (*(int *)Game::_ControlledPlayer == *(int *)Game::Self)
    return get_char_info()->encum_factor();
  else
    return 1.0f;
}

int *get_sound_manager() { return (int *)(*(int *)0x63dea8); }

void DoPercentConvert(std::string &data) {
  char temp_buffer[2048];  // Same maximum size as internal DoPercentConvert.
  strncpy_s(temp_buffer, data.c_str(), sizeof(temp_buffer));
  temp_buffer[sizeof(temp_buffer) - 1] = 0;
  Zeal::Game::get_game()->DoPercentConvert(temp_buffer, 1);
  data = temp_buffer;
}

Zeal::GameStructures::Entity *get_player_partial_name(const char *name) {
  return reinterpret_cast<Zeal::GameStructures::Entity *(__cdecl *)(const char *name)>(0x0050820e)(name);
}

void log(const char *data) { reinterpret_cast<void(__cdecl *)(const char *data)>(0x005240dc)(data); }

void log(std::string &data) { reinterpret_cast<void(__cdecl *)(const char *data)>(0x5240dc)(data.c_str()); }

Zeal::GameStructures::GAMECHARINFO *get_char_info() { return (Zeal::GameStructures::GAMECHARINFO *)(*(int *)0x7F94E8); }

void do_autoattack(bool enabled) { reinterpret_cast<void(__thiscall *)(int, bool)>(0x5493b5)(0x798540, enabled); }

const Zeal::GameStructures::GameCommand *get_command_struct(const std::string &command) {
  auto command_list = reinterpret_cast<const Zeal::GameStructures::GameCommand *>(0x00609af8);
  for (int i = 0; i < Zeal::GameStructures::GameCommand::kNumCommands; ++i) {
    if ((command_list[i].localized_name && command == command_list[i].localized_name) ||
        (command == command_list[i].name))
      return &command_list[i];
  }
  return nullptr;
}

int get_command_function(const std::string &command) {
  auto game_command = get_command_struct(command);
  if (game_command) return game_command->fn;
  return 0;
}

std::vector<std::string> get_command_matches(const std::string &start_of_name) {
  std::vector<std::string> result;
  auto command_list = reinterpret_cast<const Zeal::GameStructures::GameCommand *>(0x00609af8);
  for (int i = 0; i < Zeal::GameStructures::GameCommand::kNumCommands; ++i) {
    const char *localized_name = command_list[i].localized_name;
    const char *name = command_list[i].name;
    if (localized_name &&
        (start_of_name.empty() || _strnicmp(localized_name, start_of_name.c_str(), start_of_name.length()) == 0))
      result.push_back(command_list[i].localized_name);

    if ((start_of_name.empty() || _strnicmp(name, start_of_name.c_str(), start_of_name.length()) == 0) &&
        (!localized_name || _stricmp(localized_name, name)))
      result.push_back(command_list[i].name);
  }
  return result;
}

Zeal::GameStructures::ViewActor *get_view_actor() {
  Zeal::GameStructures::ViewActor *v = *(Zeal::GameStructures::ViewActor **)Zeal::Game::ViewActor;
  return v;
}

UINT get_game_time() { return reinterpret_cast<UINT(__cdecl *)()>(0x4f35c7)(); }

int get_game_main() { return *(int *)0x7f9574; }

void SetMusicSelection(int number, bool enabled) {
  int *sound_manager = get_sound_manager();
  if (sound_manager)
    reinterpret_cast<void(__thiscall *)(int *, int, bool)>(0x4d54c1)(get_sound_manager(), number, enabled);
}

void WavePlay(int index) {
  if (get_sound_manager())
    reinterpret_cast<void(__thiscall *)(int *, int, int)>(0x004d518b)(get_sound_manager(), index, 0);
}

bool CanIHitTarget(float dist) {
  return reinterpret_cast<bool(__thiscall *)(Zeal::GameStructures::Entity *, Zeal::GameStructures::Entity *, float)>(
      0x509E09)(get_self(), get_target(), dist);
}

bool do_attack(uint8_t type, uint8_t p2) {
  return reinterpret_cast<bool(__thiscall *)(Zeal::GameStructures::Entity * player, uint8_t type, uint8_t p2,
                                             Zeal::GameStructures::Entity * target)>(0x50A0F8)(get_self(), type, p2,
                                                                                               get_target());
}

void do_raidaccept() {
  if (get_self())
    reinterpret_cast<void(__thiscall *)(Zeal::GameStructures::Entity * player, const char *unused)>(0x004f3be5)(
        get_self(), "");
}

void do_raiddecline() {
  if (get_self())
    reinterpret_cast<void(__thiscall *)(Zeal::GameStructures::Entity * player, const char *unused)>(0x004f3bc1)(
        get_self(), "");
}

Zeal::GameStructures::Entity *get_view_actor_entity() {
  Zeal::GameStructures::ViewActor *Actor = get_view_actor();
  if (Actor)
    return Actor->Entity;
  else
    return nullptr;
}

const char *trim_name(const char *name)  // aka trimName in client
{
  // Returns a pointer to a modified name stored in a 64-entry global circular buffer. Increments buffer index.
  // This function cleans the name (removes numbers, special characters) but retains any suffix.
  // Use for cleaning up name's corpse123 => name's corpse.
  if (name == NULL) return (char *)"";
  return Zeal::Game::get_game()->trimName(name);
}

const char *strip_name(const char *name)  // aka stripName in client
{
  // Returns a pointer to a modified name stored in a 64-entry global circular buffer. Does not increment buffer index.
  // Strips any numbers and any text after an apostrophe.  Use for name's corpse123 => name.
  if (name == nullptr) return "";
  return Zeal::Game::get_game()->stripName(name);
}

void send_message(UINT opcode, int *buffer, UINT size, int unknown) {
  reinterpret_cast<void(__cdecl *)(int *connection, UINT opcode, int *buffer, UINT size, int unknown)>(0x54e51a)(
      (int *)0x7952fc, opcode, buffer, size, unknown);
}

bool is_view_actor_me() {
  if (get_controlled() && get_controlled()->ActorInfo) {
    int my_view_actor = (int)get_controlled()->ActorInfo->ViewActor_;
    if ((int)get_view_actor() == my_view_actor) return true;
  }
  return false;
}

void print_group_leader() {
  const Zeal::GameStructures::GroupInfo *group_info = Zeal::Game::GroupInfo;
  if (!group_info->is_in_group())
    print_chat("You are not in a group.");
  else if (strcmp(group_info->LeaderName, Zeal::Game::get_char_info()->Name) == 0)
    print_chat("You are the group leader.");
  else
    print_chat("%s is the leader of your group.", group_info->LeaderName);
}

void print_raid_leaders(bool show_all_groups, bool show_open_groups) {
  Zeal::GameStructures::RaidInfo *raid_info = Zeal::Game::RaidInfo;
  if (!raid_info->is_in_raid()) {
    print_chat("You are not in a raid.");
    return;
  }

  DWORD group_number = get_raid_group_number();
  std::string group_leader = get_raid_group_leader(group_number);
  if (group_leader.empty())
    print_group_leader();  // Not in a raid group, so fallback to report a normal group.
  else {
    if (strcmp(group_leader.c_str(), Zeal::Game::get_char_info()->Name) == 0)
      print_chat("You are the leader of your raid group [%i].", group_number + 1);
    else
      print_chat("%s is the leader of your raid group [%i].", group_leader.c_str(), group_number + 1);

    const Zeal::GameStructures::GroupInfo *group_info = Zeal::Game::GroupInfo;
    if (group_info->is_in_group() && strcmp(group_leader.c_str(), group_info->LeaderName))
      print_chat("Mismatch: %s is the leader of your normal group.", group_info->LeaderName);
  }

  if (raid_info->IsLeader)
    print_chat("You are the raid leader.");
  else
    print_chat("%s is the leader of your raid.", raid_info->LeaderName);

  if (show_all_groups || show_open_groups) {
    for (int i = 0; i < Zeal::GameStructures::RaidInfo::kRaidMaxMembers / 6; i++) {
      int group_count = get_raid_group_count(i);
      if (group_count == 0 && !show_all_groups) continue;

      group_leader = get_raid_group_leader(i);
      if (group_count == 0)
        print_chat("Group[%i]: Empty", i + 1);
      else if (group_count < 6)
        print_chat("Group[%i]: %s (%i slots open)", i + 1, group_leader.c_str(), 6 - group_count);
      else if (show_all_groups)
        print_chat("Group[%i]: %s (FULL)", i + 1, group_leader.c_str());
    }
    print_raid_ungrouped();
  }
}

void print_raid_ungrouped() {
  Zeal::GameStructures::RaidInfo *raid_info = Zeal::Game::RaidInfo;
  if (!raid_info->is_in_raid()) {
    print_chat("You are not in a raid.");
    return;
  }

  bool zero_ungrouped = true;
  for (int i = 0; i < Zeal::GameStructures::RaidInfo::kRaidMaxMembers; ++i) {
    const auto &member = raid_info->MemberList[i];
    if (member.GroupNumber == Zeal::GameStructures::RaidMember::kRaidUngrouped && member.Name[0]) {
      if (zero_ungrouped) print_chat("There are ungrouped raid members:");
      zero_ungrouped = false;
      print_chat("  %s (%s %s)", member.Name, member.PlayerLevel, member.Class);
    }
  }
  if (zero_ungrouped) print_chat("All raid members are grouped.");
}

void dump_raid_state() {
  Zeal::GameStructures::RaidInfo *raid_info = Zeal::Game::RaidInfo;
  if (!raid_info->is_in_raid()) {
    print_chat("You are not in a raid.");
    return;
  }

  print_chat("Raid member count: %i", raid_info->MemberCount);
  print_chat("Raid id: 0x%08x", raid_info->Id);
  print_chat("Raid leader: %s", raid_info->LeaderName);
  print_chat("Is raid leader: %i", raid_info->IsLeader);
  print_chat("Raid loot type: %i", raid_info->LootType);
  for (int i = 0; i < Zeal::GameStructures::RaidInfo::kRaidMaxLooters; ++i) {
    if (raid_info->LooterNames[i][0]) print_chat("Looter[%i]: %s", i, raid_info->LooterNames[i]);
  }
  for (int i = 0; i < Zeal::GameStructures::RaidInfo::kRaidMaxMembers; ++i) {
    if (raid_info->MemberList[i].Name[0]) {
      const Zeal::GameStructures::RaidMember &member = raid_info->MemberList[i];
      print_chat("Member[%i]: %s, %s, %s, %i, %i, %i", i, member.Name, member.PlayerLevel, member.Class,
                 member.ClassValue, member.IsGroupLeader, member.GroupNumber);
    }
  }
}

std::vector<Zeal::GameStructures::RaidMember *> get_raid_list() {
  std::vector<Zeal::GameStructures::RaidMember *> raid_member_list;

  Zeal::GameStructures::RaidInfo *raid_info = Zeal::Game::RaidInfo;
  if (!raid_info->is_in_raid()) {
    return raid_member_list;
  }

  for (int i = 0; i < Zeal::GameStructures::RaidInfo::kRaidMaxMembers; i++)  // sometimes gaps so need to check all
  {
    Zeal::GameStructures::RaidMember *raid_member = &raid_info->MemberList[i];
    if (raid_member->Name[0] != '\0') {
      raid_member_list.push_back(raid_member);
    }
  }

  return raid_member_list;
}

DWORD get_raid_group_number() {
  Zeal::GameStructures::RaidInfo *raid_info = Zeal::Game::RaidInfo;
  if (!raid_info->is_in_raid()) return Zeal::GameStructures::RaidMember::kRaidUngrouped;
  const char *self_name = Zeal::Game::get_char_info()->Name;
  for (int i = 0; i < Zeal::GameStructures::RaidInfo::kRaidMaxMembers; ++i) {
    if (strcmp(self_name, raid_info->MemberList[i].Name) == 0) return raid_info->MemberList[i].GroupNumber;
  }
  return Zeal::GameStructures::RaidMember::kRaidUngrouped;
}

std::string get_raid_group_leader(DWORD group_number) {
  std::string group_leader("");
  Zeal::GameStructures::RaidInfo *raid_info = Zeal::Game::RaidInfo;
  if (!raid_info->is_in_raid() || group_number >= Zeal::GameStructures::RaidInfo::kRaidMaxMembers / 6)
    return group_leader;

  for (int i = 0; i < Zeal::GameStructures::RaidInfo::kRaidMaxMembers; ++i) {
    if (raid_info->MemberList[i].GroupNumber == group_number && raid_info->MemberList[i].IsGroupLeader) {
      // The server/client can get out of sync, so warn if the state is goofy.
      if (group_leader.empty())
        group_leader = std::string(raid_info->MemberList[i].Name);
      else
        print_chat("Error: Extra raid group leader: %s", raid_info->MemberList[i].Name);
    }
  }
  return group_leader;
}

int get_raid_group_count(DWORD group_number) {
  Zeal::GameStructures::RaidInfo *raid_info = Zeal::Game::RaidInfo;
  if (!raid_info->is_in_raid() || group_number >= Zeal::GameStructures::RaidInfo::kRaidMaxMembers / 6) return 0;

  int group_count = 0;
  for (int i = 0; i < Zeal::GameStructures::RaidInfo::kRaidMaxMembers; ++i) {
    if (raid_info->MemberList[i].GroupNumber == group_number && raid_info->MemberList[i].Name[0]) group_count++;
  }
  return min(6, group_count);
}

DWORD get_raid_class_color(BYTE class_id) {
  const DWORD kDefaultWhite = 0xffffffff;
  const auto *raid_wnd = Windows->Raid;
  if (!raid_wnd || !raid_wnd->SidlPiece) return kDefaultWhite;

  // This LUT maps Zeal::GameEnums::ClassTypes to the raid window offsets.
  static const int class_to_color_lut[16] = {
      0,   // Unused placeholder for invalid class_id == 0.
      13,  // Zeal::GameEnums::ClassTypes::Warrior
      2,   // Zeal::GameEnums::ClassTypes::Cleric
      8,   // Zeal::GameEnums::ClassTypes::Paladin
      9,   // Zeal::GameEnums::ClassTypes::Ranger
      11,  // Zeal::GameEnums::ClassTypes::Shadowknight
      3,   // Zeal::GameEnums::ClassTypes::Druid
      6,   // Zeal::GameEnums::ClassTypes::Monk
      0,   // Zeal::GameEnums::ClassTypes::Bard
      10,  // Zeal::GameEnums::ClassTypes::Rogue
      12,  // Zeal::GameEnums::ClassTypes::Shaman
      7,   // Zeal::GameEnums::ClassTypes::Necromancer
      14,  // Zeal::GameEnums::ClassTypes::Wizard
      5,   // Zeal::GameEnums::ClassTypes::Magician
      4,   // Zeal::GameEnums::ClassTypes::Enchanter
      1,   // Zeal::GameEnums::ClassTypes::Beastlord
  };

  if (class_id < 1 || class_id > 15) return kDefaultWhite;
  return raid_wnd->ClassColors[class_to_color_lut[class_id]];
}

bool is_raid_pet(const Zeal::GameStructures::Entity &entity) {
  const int pet_owner_id = entity.PetOwnerSpawnId;
  if (!pet_owner_id || (entity.Type != Zeal::GameEnums::NPC)) return false;

  const auto self = Zeal::Game::get_self();
  if (!self || self->SpawnId == pet_owner_id) return false;  // Don't hide the self pet.

  const auto raid_info = Zeal::Game::RaidInfo;
  if (!raid_info->is_in_raid()) return false;

  auto owner = Zeal::Game::get_entity_by_id(pet_owner_id);
  if (!owner || owner->Type != Zeal::GameEnums::Player) return false;

  for (int i = 0; i < Zeal::GameStructures::RaidInfo::kRaidMaxMembers; ++i) {
    const Zeal::GameStructures::RaidMember &member = raid_info->MemberList[i];
    if (member.Name[0] == 0)  // Empty string check.
      continue;
    auto raid_entity = ZealService::get_instance()->entity_manager->Get(member.Name);
    if (raid_entity && raid_entity->SpawnId == pet_owner_id) return true;
  }

  return false;
}

// Returns true if successfully updated the group window with class colors.
// Note: The group member entity list entry is nulled out in the player deconstructor,
// so if they zone it will go back to default at the next update.
bool update_group_window_colors(bool use_raid_colors, bool store_defaults) {
  auto *group_wnd = Zeal::Game::Windows->Group;
  const auto *group_info = Zeal::Game::GroupInfo;
  if (!group_wnd || !group_info) return !is_new_ui();  // Return true (okay) if old UI.

  const Zeal::GameUI::ARGBCOLOR kDefaultColor = Zeal::GameUI::ARGBCOLOR(0xfff0f0f0);
  static Zeal::GameUI::ARGBCOLOR default_colors[GAME_NUM_GROUP_MEMBERS] = {kDefaultColor, kDefaultColor, kDefaultColor,
                                                                           kDefaultColor, kDefaultColor};

  for (int i = 0; i < GAME_NUM_GROUP_MEMBERS; ++i) {
    auto *gauge_wnd = group_wnd->GetChildItem(std::format("Gauge{}", i + 1), false);
    if (!gauge_wnd) return false;

    if (store_defaults) default_colors[i] = gauge_wnd->TextColor;
    Zeal::GameUI::ARGBCOLOR text_color = default_colors[i];
    if (use_raid_colors && group_info->Names[i][0] && (get_gamestate() == GAMESTATE_INGAME)) {
      Zeal::GameStructures::Entity *member = group_info->EntityList[i];
      if (member) text_color = Zeal::Game::get_raid_class_color(member->Class);
    }
    gauge_wnd->TextColor = text_color;
  }
  return true;
}

bool is_player_pet(const Zeal::GameStructures::Entity &entity) {
  const int pet_owner_id = entity.PetOwnerSpawnId;
  if (!pet_owner_id || (entity.Type != Zeal::GameEnums::NPC)) return false;

  auto owner_entity = Zeal::Game::get_entity_by_id(pet_owner_id);

  return (owner_entity && owner_entity->Type == Zeal::GameEnums::Player);
}

Vec3 get_view_actor_head_pos() {
  // print_chat("movement: %i", get_view_actor()->Entity->ActorInfo->MovementType);
  // if (get_view_actor())
  //{
  //	Zeal::GameStructures::Entity* self = get_view_actor()->Entity;
  //	Vec3 head_pos = self->Position;
  //	Vec3 dag_pos = self->ActorInfo->DagHeadPoint->Position;
  //	head_pos.z = dag_pos.z;
  //	return head_pos;
  // }
  auto view_actor = get_view_actor();
  auto self = view_actor ? view_actor->Entity : nullptr;
  if (self) {
    Vec3 head_pos = self->Position;
    head_pos.z += (self->CameraHeightOffset - self->ModelHeightOffset) - 0.5f;  // standing
    // if (self->StandingState == Stance::Duck || self->StandingState == Stance::Sit)
    //	head_pos.z -= self->Height / 3;// self->CameraHeightOffset - self->ModelHeightOffset;
    // else if (self->StandingState != Stance::Stand)
    //	head_pos.z = self->Position.z;
    return head_pos;
  } else {
    return {0, 0, 0};
  }
}

bool is_in_character_select() { return *(int *)0x63d5d8 != 0; }

int get_region_from_pos(Vec3 *pos) {
  static int last_good_region = 0;
  auto display = Zeal::Game::get_display();
  auto t3dGetRegionNumberFromWorldAndXYZ = reinterpret_cast<int(__cdecl *)(int *, Vec3 *)>(*(int **)(0x007f9a30));

  int rval = display ? t3dGetRegionNumberFromWorldAndXYZ(display->World, pos) : -1;
  if (rval == -1)
    rval = last_good_region;
  else
    last_good_region = rval;
  return rval;
}

bool collide_with_world(Vec3 start, Vec3 end, Vec3 &result, char collision_type, bool debug) {
  auto display = Game::get_display();
  if (!display) return false;
  char x = Game::get_display()->GenericSphereColl(start.x, start.y, start.z, end.x, end.y, end.z, (float *)&result.x,
                                                  (float *)&result.y, (float *)&result.z, collision_type);

  if (debug) {
    print_chat("start: %s  end: %s dist: %f result: %i", start.toString().c_str(), end.toString().c_str(),
               result.Dist(end), x);
  }
  return result.Dist(end) > 0.1;  // return true if there was a collision
}

bool can_move() {
  Zeal::GameStructures::Entity *self = Zeal::Game::get_controlled();
  if (!self) return false;
  // Check if mounted on a horse.
  if (self != Zeal::Game::get_self() && Zeal::Game::get_self()->ActorInfo &&
      Zeal::Game::get_self()->ActorInfo->Mount == self)
    self = self->ActorInfo->Rider;  // Use Rider for check below.

  if (self->ActorInfo->ViewActor_ != Zeal::Game::get_view_actor()) return false;
  if (self->CharInfo && self->CharInfo->StunnedState) return false;
  if (self->StandingState == Zeal::GameEnums::Stance::Standing ||
      self->StandingState == Zeal::GameEnums::Stance::Ducking)
    return true;
  return false;
}

float CalcBoundingRadius(Zeal::GameStructures::Entity *ent) {
  int myrace = ent->Race;
  float mysize = ent->Height;
  float base_size = 5.0f;
  int mygender = ent->Gender;
  float myradius = 6.0f;
  switch (myrace) {
    case 1:   // "Human"
    case 2:   // "Barbarian"
    case 3:   // "Erudite"
    case 4:   // "Wood Elf"
    case 5:   // "High Elf"
    case 6:   // "Dark Elf"
    case 7:   // "Half Elf"
    case 8:   // "Dwarf"
    case 9:   // "Troll",
    case 10:  // "Ogre"
    case 11:  // "Halfling"
    case 12:  // "Gnome"
      // playable races have fixed mods
      mysize = 5.0f;
      myradius = 5.0f;
      break;
    case 13:  // "Aviak"
      break;
    case 14:  // "Were Wolf"
      break;
    case 15:  // "Brownie",
      break;
    case 16:  // "Centaur"
      break;
    case 17:  // "Golem"
      break;
    case 18:  // "Giant / Cyclops"
      break;
    case 19:  // "Trakanon",
      myradius = 10.48f;
      break;
    case 20:
      break;
    case 21:  // "Evil Eye"
      break;
    case 22:  // "Beetle"
      break;
    case 23:  // "Kerra"
      break;
    case 24:  // "Fish"
      break;
    case 25:  // "Fairy"
      break;
    case 26:  // "Old Froglok"
      break;
    case 27:  // "Old Froglok Ghoul"
      break;
    case 28:  // "Fungusman"
      break;
    case 29:  // "Gargoyle"
      break;
    case 30:  // "Gasbag"
      break;
    case 31:  // "Gelatinous Cube"
      break;
    case 33:  // "Ghoul"
      break;
    case 34:  // "Giant Bat"
      mysize = 5.0f;
      break;
    case 36:  // "Giant Rat"
      break;
    case 37:  // "Giant Snake"
      break;
    case 38:  // "Giant Spider"
      break;
    case 39:  // "Gnoll"
      break;
    case 40:  // "Goblin"
      break;
    case 41:  // "Gorilla"
      break;
    case 42:  // "Wolf"
      break;
    case 43:  // "Bear"
      break;
    case 44:  // "Freeport Guards"
      break;
    case 45:  // "Demi Lich"
      break;
    case 46:  // "Imp"
      break;
    case 47:  // "Griffin"
      break;
    case 48:  // "Kobold"
      break;
    case 49:  // "Lava Dragon"
      mysize = 32.5f;
      break;
    case 50:  // "Lion"
      break;
    case 51:  // "Lizard Man"
      break;
    case 52:  // "Mimic"
      break;
    case 53:  // "Minotaur"
      break;
    case 54:  // "Orc"
      break;
    case 55:  // "Human Beggar"
      break;
    case 56:  // "Pixie"
      break;
    case 57:  // "Dracnid"
      break;
    case 58:  // "Solusek Ro"
      break;
    case 59:  // "Bloodgills"
      break;
    case 60:  // "Skeleton"
      break;
    case 61:  // "Shark"
      break;
    case 62:  // "Tunare"
      break;
    case 63:  // "Tiger"
      break;
    case 64:  // "Treant"
      break;
    case 65:  // "Vampire"
      break;
    case 66:  // "Rallos Zek"
      break;
    case 67:  // "Highpass Citizen"
      break;
    case 68:  // "Tentacle"
      break;
    case 69:  // "Will 'O Wisp"
      break;
    case 70:  // "Zombie"
      break;
    case 71:  // "Qeynos Citizen"
      break;
    case 74:  // "Piranha"
      break;
    case 75:  // "Elemental"
      break;
    case 76:  // "Puma"
      break;
    case 77:  // "Neriak Citizen"
      break;
    case 78:  // "Erudite Citizen"
      break;
    case 79:  // "Bixie"
      break;
    case 80:  // "Reanimated Hand"
      break;
    case 81:  // "Rivervale Citizen"
      break;
    case 82:  // "Scarecrow"
      break;
    case 83:  // "Skunk"
      break;
    case 85:  // "Spectre"
      break;
    case 86:  // "Sphinx"
      break;
    case 87:  // "Armadillo"
      break;
    case 88:  // "Clockwork Gnome"
      break;
    case 89:  // "Drake"
      break;
    case 90:  // "Halas Citizen"
      break;
    case 91:  // "Alligator"
      break;
    case 92:  // "Grobb Citizen"
      break;
    case 93:  // "Oggok Citizen"
      break;
    case 94:  // "Kaladim Citizen"
      break;
    case 95:  // "Cazic Thule"
      break;
    case 96:  // "Cockatrice"
      break;
    case 98:  // "Elf Vampire"
      break;
    case 99:  // "Denizen"
      break;
    case 100:  // "Dervish"
      break;
    case 101:  // "Efreeti"
      break;
    case 102:  // "Old Froglok Tadpole"
      break;
    case 103:  // "Kedge"
      break;
    case 104:  // "Leech"
      break;
    case 105:  // "Swordfish"
      break;
    case 106:  // "Felguard"
      break;
    case 107:  // "Mammoth"
      break;
    case 108:  // "Eye of Zomm"
      break;
    case 109:  // "Wasp"
      break;
    case 110:  // "Mermaid"
      break;
    case 111:  // "Harpie"
      break;
    case 112:  // "Fayguard"
      break;
    case 113:  // "Drixie"
      break;
    case 116:  // "Sea Horse"
      break;
    case 117:  // "Ghost Dwarf"
      break;
    case 118:  // "Erudite Ghost"
      break;
    case 119:  // "Sabertooth Cat"
      break;
    case 120:  // "Wolf Elemental",
      break;
    case 121:  // "Gorgon"
      break;
    case 122:  // "Dragon Skeleton"
      break;
    case 123:  // "Innoruuk"
      break;
    case 124:  // "Unicorn"
      break;
    case 125:  // "Pegasus"
      break;
    case 126:  // "Djinn"
      break;
    case 127:  // "Invisible Man"
      break;
    case 128:  // "Iksar"
      // playable races have fixed mods
      mysize = 5.0f;
      myradius = 5.0f;
      break;
    case 129:  // "Scorpion"
      break;
    case 130:  // "Vah Shir"
      // playable races have fixed mods
      mysize = 5.0f;
      myradius = 5.0f;
      break;
    case 131:  // "Sarnak"
      break;
    case 133:  // "Lycanthrope"
      break;
    case 134:  // "Mosquito"
      break;
    case 135:  // "Rhino"
      break;
    case 136:  // "Xalgoz"
      break;
    case 137:  // "Kunark Goblin"
      break;
    case 138:  // "Yeti"
      break;
    case 139:  // "Iksar Citizen"
      break;
    case 140:  // "Forest Giant"
      break;
    case 144:  // "Burynai"
      break;
    case 145:  // "Goo"
      break;
    case 146:  // "Spectral Sarnak"
      break;
    case 147:  // "Spectral Iksar"
      break;
    case 148:  // "Kunark Fish"
      break;
    case 149:  // "Iksar Scorpion"
      break;
    case 150:  // "Erollisi"
      break;
    case 151:  // "Tribunal"
      break;
    case 153:  // "Bristlebane"
      break;
    case 154:  // "Fay Drake"
      break;
    case 155:  // "Sarnak Skeleton"
      break;
    case 156:  // "Ratman"
      break;
    case 157:  // "Wyvern"
      break;
    case 158:  // "Wurm"
      mysize = 16.0f;
      break;
    case 159:  // "Devourer"
      break;
    case 160:  // "Iksar Golem"
      break;
    case 161:  // "Iksar Skeleton"
      break;
    case 162:  // "Man Eating Plant"
      break;
    case 163:  // "Raptor"
      break;
    case 164:  // "Sarnak Golem"
      break;
    case 165:  // "Water Dragon"
      break;
    case 166:  // "Iksar Hand"
      break;
    case 167:  // "Succulent"
      break;
    case 168:  // "Flying Monkey"
      break;
    case 169:  // "Brontotherium"
      break;
    case 170:  // "Snow Dervish"
      break;
    case 171:  // "Dire Wolf"
      break;
    case 172:  // "Manticore"
      break;
    case 173:  // "Totem"
      break;
    case 174:  // "Cold Spectre"
      break;
    case 175:  // "Enchanted Armor"
      break;
    case 176:  // "Snow Bunny"
      break;
    case 177:  // "Walrus"
      break;
    case 178:  // "Rock-gem Men"
      break;
    case 181:  // "Yak Man"
      break;
    case 183:  // "Coldain"
      break;
    case 184:  // "Velious Dragons"
      myradius = 9.48f;
      break;
    case 185:  // "Hag"
      break;
    case 187:  // "Siren"
      break;
    case 188:  // "Frost Giant"
      break;
    case 189:  // "Storm Giant"
      break;
    case 190:  // "Ottermen"
      break;
    case 191:  // "Walrus Man"
      break;
    case 192:  // "Clockwork Dragon"
      myradius = 10.48f;
      break;
    case 193:  // "Abhorent"
      break;
    case 194:  // "Sea Turtle"
      break;
    case 195:  // "Black and White Dragons"
      myradius = 9.48f;
      break;
    case 196:  // "Ghost Dragon"
      myradius = 9.48f;
      break;
    case 198:  // "Prismatic Dragon"
      myradius = 9.48f;
      break;
    case 199:  // "ShikNar"
      break;
    case 200:  // "Rockhopper"
      break;
    case 201:  // "Underbulk"
      break;
    case 202:  // "Grimling"
      break;
    case 203:  // "Vacuum Worm"
      break;
    case 205:  // "Kahli Shah"
      break;
    case 206:  // "Owlbear"
      break;
    case 207:  // "Rhino Beetle"
      break;
    case 208:  // "Vampyre"
      break;
    case 209:  // "Earth Elemental"
      break;
    case 210:  // "Air Elemental"
      break;
    case 211:  // "Water Elemental"
      break;
    case 212:  // "Fire Elemental"
      break;
    case 213:  // "Wetfang Minnow"
      break;
    case 214:  // "Thought Horror"
      break;
    case 215:  // "Tegi"
      break;
    case 216:  // "Horse"
      break;
    case 217:  // "Shissar"
      break;
    case 218:  // "Fungal Fiend"
      break;
    case 219:  // "Vampire Volatalis"
      break;
    case 220:  // "StoneGrabber"
      break;
    case 221:  // "Scarlet Cheetah"
      break;
    case 222:  // "Zelniak"
      break;
    case 223:  // "Lightcrawler"
      break;
    case 224:  // "Shade"
      break;
    case 225:  // "Sunflower"
      break;
    case 226:  // "Sun Revenant"
      break;
    case 227:  // "Shrieker"
      break;
    case 228:  // "Galorian"
      break;
    case 229:  // "Netherbian"
      break;
    case 230:  // "Akheva"
      break;
    case 231:  // "Spire Spirit"
      break;
    case 232:  // "Sonic Wolf"
      break;
    case 234:  // "Vah Shir Skeleton"
      break;
    case 235:  // "Mutant Humanoid"
      break;
    case 236:  // "Seru"
      break;
    case 237:  // "Recuso"
      break;
    case 238:  // "Vah Shir King"
      break;
    case 239:  // "Vah Shir Guard"
      break;
    case 241:  // "Lujein",
    case 242:  // "Naiad",
    case 243:  // "Nymph",
    case 244:  // "Ent",
    case 245:  // "Fly Man",
    case 246:  // "Tarew Marr"
      break;
    case 247:  // "Sol Ro"
      break;
    case 248:  // "Clockwork Golem"
      break;
    case 249:  // "Clockwork Brain",
    case 250:  // "Spectral Banshee",
    case 251:  // "Guard of Justice",
    case 252:  // 'PoM Castle',
    case 253:  // "Disease Boss"
    case 254:  // "Sol Ro Guard"
    case 255:  // "New Bertox",
    case 256:  // "New Tribunal",
    case 257:  // "Terris Thule",
    case 258:  // "Vegerog",
    case 259:  // "Crocodile",
    case 260:  // "Bat",
    case 261:  // "Slarghilug",
    case 262:  // "Tranquilion"
    case 263:  // "Tin Soldier"
    case 264:  // "Nightmare Wraith",
    case 265:  // "Malarian",
    case 266:  // "Knight of Pestilence",
    case 267:  // "Lepertoloth",
    case 268:  // "Bubonian Boss",
    case 269:  // "Bubonian Underling",
    case 270:  // "Pusling",
    case 271:  // "Water Mephit",
    case 272:  // "Stormrider",
    case 273:  // "Junk Beast"
      break;
    case 274:  // "Broken Clockwork"
      break;
    case 275:  // "Giant Clockwork",
    case 276:  // "Clockwork Beetle",
    case 277:  // "Nightmare Goblin",
    case 278:  // "Karana",
    case 279:  // "Blood Raven",
    case 280:  // "Nightmare Gargoyle",
    case 281:  // "Mouths of Insanity",
    case 282:  // "Skeletal Horse",
    case 283:  // "Saryn",
    case 284:  // "Fennin Ro",
    case 285:  // "Tormentor",
    case 286:  // "Necro Priest",
    case 287:  // "Nightmare",
    case 288:  // "New Rallos Zek",
    case 289:  // "Vallon Zek",
    case 290:  // "Tallon Zek",
    case 291:  // "Air Mephit",
    case 292:  // "Earth Mephit",
    case 293:  // "Fire Mephit",
    case 294:  // "Nightmare Mephit",
    case 295:  // "Zebuxoruk",
    case 296:  // "Mithaniel Marr",
    case 297:  // "Undead Knight",
    case 298:  // "The Rathe",
    case 299:  // "Xegony",
    case 300:  // "Fiend",
    case 301:  // "Test Object",
    case 302:  // "Crab",
    case 303:  // "Phoenix",
    case 304:  // "PoP Dragon",
    case 305:  // "PoP Bear",
    case 306:  // "Storm Taarid",
    case 307:  // "Storm Satuur",
    case 308:  // "Storm Kuraaln",
    case 309:  // "Storm Volaas",
    case 310:  // "Storm Mana",
    case 311:  // "Storm Fire",
    case 312:  // "Storm Celestial",
    case 313:  // "War Wraith",
    case 314:  // "Wrulon",
    case 315:  // "Kraken",
    case 316:  // "Poison Frog",
    case 317:  // "Queztocoatal",
    case 318:  // "Valorian",
    case 319:  // "War Boar",
    case 320:  // "PoP Efreeti",
    case 321:  // "War Boar Unarmored",
    case 322:  // "Black Knight",
    case 323:  // "Animated Armor",
    case 324:  // "Undead Footman",
    case 325:  // "Rallos Zek Minion"
    case 326:  // "Arachnid"
    case 327:  // "Crystal Spider",
    case 328:  // "Zeb Cage",
    case 329:  // "BoT Portal",
    case 330:  // "Froglok",
    case 331:  // "Troll Buccaneer",
    case 332:  // "Troll Freebooter",
    case 333:  // "Troll Sea Rover",
    case 334:  // "Spectre Pirate Boss",
    case 335:  // "Pirate Boss",
    case 336:  // "Pirate Dark Shaman",
    case 337:  // "Pirate Officer",
    case 338:  // "Gnome Pirate",
    case 339:  // "Dark Elf Pirate",
    case 340:  // "Ogre Pirate",
    case 341:  // "Human Pirate",
    case 342:  // "Erudite Pirate",
    case 343:  // "Poison Dart Frog",
    case 344:  // "Troll Zombie",
    case 345:  // "Luggald Land",
    case 346:  // "Luggald Armored",
    case 347:  // "Luggald Robed",
    case 348:  // "Froglok Mount",
    case 349:  // "Froglok Skeleton",
    case 350:  // "Undead Froglok",
    case 351:  // "Chosen Warrior",
    case 352:  // "Chosen Wizard",
    case 353:  // "Veksar",
    case 354:  // "Greater Veksar",
    case 355:  // "Veksar Boss",
    case 356:  // "Chokadai",
    case 357:  // "Undead Chokadai",
    case 358:  // "Undead Veksar",
    case 359:  // "Vampire Lesser",
    case 360:  // "Vampire Elite",
      break;
    default:
      myradius = 6.0;
      break;
  }
  myradius *= mysize;
  myradius /= base_size;
  return myradius;
}

float CalcZOffset(Zeal::GameStructures::Entity *ent) {
  float size = std::clamp(ent->Height, 0.f, 20.f);  // rule has bestzsizemax at 20 on server code
  switch (ent->Race) {
    case 49:   // dragon
    case 158:  // wurm
    case 196:  // ghost dragon
      return 20.f;
    case 63:  // tiger
      return (size / 5.0f * 3.125f * 0.44999999f);
    case 42:   // wolf
    case 120:  // wolf elemntal
      if (ent->Gender == 2) return (size / 5.0f * 3.125f * 0.44999999f);
  }
  return (size / 5.0f * 3.125f);
}

float CalcCombatRange(Zeal::GameStructures::Entity *entity1, Zeal::GameStructures::Entity *entity2) {
  float size_mod = (CalcBoundingRadius(entity1) + CalcBoundingRadius(entity2)) * 0.75f;
  float z_diff = std::abs(CalcZOffset(entity1) - CalcZOffset(entity2));
  if (entity2->MovementSpeed > 0) size_mod += 2.0f;
  size_mod += z_diff;
  size_mod = std::clamp(size_mod, 14.f, 75.f);
  return size_mod;
}

bool is_view_actor_invisible(Zeal::GameStructures::Entity *entity) {
  // Replicates logic of t3dIsActorInvisible. Note this more of a 'clickability' check then visibility.
  if (entity && entity->ActorInfo && entity->ActorInfo->ViewActor_)
    return (entity->ActorInfo->ViewActor_->Flags & 0x40000000) != 0;
  return false;
}

// Triggers an update of the display's camera location state.
void update_get_camera_location() {
  auto display = Zeal::Game::get_display();
  if (!display) return;

  auto t3dGetCameraLocation = reinterpret_cast<int(__cdecl *)(float *, float *)>(*(int **)(0x007f99d4));
  t3dGetCameraLocation(display->ActiveCamera, display->CameraLocation);
}

// Updates the display's VisibleReferenceList and returns the number of visible actors.
int update_get_world_visible_actor_list(float max_distance) {
  auto display = Zeal::Game::get_display();
  if (!display) return 0;

  auto s3dGetWorldVisibleActorList =
      reinterpret_cast<int(__cdecl *)(int *world, float *active_camera, float *position, float distance, int parameter,
                                      Zeal::GameStructures::Display::ReferenceList *list)>(*(int **)(0x007f9850));
  return s3dGetWorldVisibleActorList(display->World, display->ActiveCamera, NULL, max_distance, 0x11,
                                     &display->VisibleReferenceList);
}

// Identical logic to CDisplay::IsInvisible with the exception of providing the two boolean flags
// instead of calculating can i see invis each call.
static bool is_invisible_to_me(Zeal::GameStructures::Entity *target, bool can_i_see_invis, bool am_i_gm) {
  if (!target) return false;

  if (target->Type >= Zeal::GameEnums::EntityTypes::NPCCorpse || target->HpCurrent <= 0) return false;

  if (target->TargetType > 0x40) return true;
  if (target->VisibilityState != 0x01) return false;
  if (!can_i_see_invis) return true;
  if (!target->IsGameMaster) return false;
  if (am_i_gm) return false;
  return true;
}

bool is_targetable(Zeal::GameStructures::Entity *ent) {
  auto self = Zeal::Game::get_self();
  auto char_info = Zeal::Game::get_char_info();
  if (!self || !char_info) return false;
  bool can_see_invis = char_info->can_i_see_invis();
  if (!ent || ent->StructType != 0x03 || !ent->ActorInfo || ent->ActorInfo->IsInvisible ||
      is_invisible_to_me(ent, can_see_invis, self->IsGameMaster))
    return false;

  return true;
}

std::vector<Zeal::GameStructures::Entity *> get_world_visible_actor_list(float max_dist, bool only_targetable) {
  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  update_get_camera_location();
  int ent_count = update_get_world_visible_actor_list(max_dist);
  int *cObject = Zeal::Game::get_display()->VisibleReferenceList.list;
  Zeal::GameStructures::Entity *current_ent;

  auto char_info = Zeal::Game::get_char_info();
  bool can_see_invis = char_info && char_info->can_i_see_invis();
  std::vector<Zeal::GameStructures::Entity *> rEnts;
  for (int i = 0; i < ent_count; i++) {
    if (cObject[i]) {
      current_ent = *(Zeal::GameStructures::Entity **)(cObject[i] + 0x60);
      if (!current_ent || current_ent == self || current_ent->StructType != 0x03 || !current_ent->ActorInfo ||
          current_ent->ActorInfo->IsInvisible || is_invisible_to_me(current_ent, can_see_invis, self->IsGameMaster))
        continue;  // Skip self, invalid struct or target spawn types, non-visible.

      if (current_ent->Position.Dist2D(self->Position) <= max_dist) {
        bool add_to_list = !only_targetable;
        if (only_targetable) {
          Vec3 result;
          Vec3 ent_head = get_ent_head_pos(current_ent);
          Vec3 my_head = self->Position;
          my_head.z += self->Height;
          std::vector<std::pair<Vec3, Vec3>> collision_checks = {
              {my_head, ent_head},                      // face to face
              {my_head, current_ent->Position},         // your face to their feet
              {self->Position, current_ent->Position},  // your feet to their feet
              {self->Position, ent_head},               // your feet to their face
          };

          for (auto &[pos1, pos2] : collision_checks) {
            if (!collide_with_world(pos1, pos2, result, 0x3, false))  // had no collision
            {
              add_to_list = true;
              break;  // we don't really care which version of this had no world collision so break the for loop
            }
          }
        }
        if (add_to_list) rEnts.push_back(current_ent);
      }
    }
  }
  return rEnts;
}

float get_target_blink_fade_factor(float speed_factor, bool auto_attack_only) {
  if (auto_attack_only && !is_autoattacking())  // Auto attack disabled.
    return 1.0f;                                // No fading.

  // Calculate a fraction of the cycle time. Phase alignment doesn't matter, so just
  // do a modulo off of the current time in millseconds.
  const float client_cycle_time_ms = 300.f;  // Base client flicker cycle rate is ~300 ms.
  const float cycle_time_ms = max(2.f, (speed_factor <= 0) ? 2.f : (client_cycle_time_ms * speed_factor));
  const float fraction = static_cast<float>(fmod(static_cast<double>(GetTickCount64()), cycle_time_ms)) / cycle_time_ms;

  // Fade in during the first half of the cycle and out the second.
  const float fade_factor = (fraction < 0.5) ? (fraction * 2) : (1.0f - fraction) * 2;
  return fade_factor;
}

std::vector<std::string> splitStringByNewLine(const std::string &str) {
  std::vector<std::string> tokens;
  std::istringstream iss(str);
  std::string token;

  while (std::getline(iss, token, '\n')) {
    tokens.push_back(token);
  }

  return tokens;
}

void do_consent(const char *name) {
  auto do_consent_fn = reinterpret_cast<void(__cdecl *)(Zeal::GameStructures::Entity *, const char *)>(0x004fb39e);
  do_consent_fn(get_self(), name);
}

void do_say(bool hide_local, const char *format, ...) {
  BYTE orig[13] = {0};
  if (hide_local) mem::set(0x538672, 0x90, 13, orig);

  va_list argptr;
  char buffer[512];
  va_start(argptr, format);
  // printf()
  vsnprintf(buffer, 511, format, argptr);
  va_end(argptr);

  GameInternal::do_say(get_self(), buffer);

  if (hide_local && orig) {
    mem::copy(0x538672, orig, 13);
  }
}

void do_say(bool hide_local, std::string data) {
  BYTE orig[13] = {0};
  if (hide_local) mem::set(0x538672, 0x90, 13, orig);

  GameInternal::do_say(get_self(), data.c_str());

  if (hide_local && orig) {
    mem::copy(0x538672, orig, 13);
  }
}

void do_tell(const char *target_name_and_message) { GameInternal::do_tell(get_self(), target_name_and_message); }

void do_gsay(std::string data) { GameInternal::do_gsay(get_self(), data.c_str()); }

void do_guildsay(std::string data) { GameInternal::do_guildsay(get_self(), data.c_str()); }

void do_auction(std::string data) { GameInternal::do_auction(get_self(), data.c_str()); }

void do_ooc(std::string data) { GameInternal::do_ooc(get_self(), data.c_str()); }

void send_raid_chat(std::string data) { GameInternal::send_raid_chat(Zeal::Game::RaidInfo, 0, data.c_str()); }

void print_chat(const std::string &data) {
  if (!is_in_game()) {
    ZealService::get_instance()->queue_chat_message(data);
    return;
  }
  std::vector<std::string> vd = splitStringByNewLine(data);
  for (auto &d : vd) print_chat(d.c_str());
}

void print_chat(const char *format, ...) {
  va_list argptr;
  char buffer[512];
  va_start(argptr, format);
  vsnprintf(buffer, 511, format, argptr);
  va_end(argptr);
  if (!is_in_game()) {
    ZealService::get_instance()->queue_chat_message(buffer);
    return;
  }
  Zeal::Game::get_game()->dsp_chat(buffer, 0, true);
}

void print_debug(const char *format, ...) {
  va_list argptr;
  char buffer[512];
  char buffer_with_newline[514];  // Additional space for the newline and null terminator

  va_start(argptr, format);
  vsnprintf(buffer, sizeof(buffer), format, argptr);
  va_end(argptr);

  // Append newline character to the formatted string
  snprintf(buffer_with_newline, sizeof(buffer_with_newline), "%s\n", buffer);

  OutputDebugStringA(buffer_with_newline);
}

void print_chat(short color, const char *format, ...) {
  va_list argptr;
  char buffer[512];
  va_start(argptr, format);
  // printf()
  vsnprintf(buffer, 511, format, argptr);
  va_end(argptr);
  if (!is_in_game()) {
    ZealService::get_instance()->queue_chat_message(buffer);
    return;
  }
  Zeal::Game::get_game()->dsp_chat(buffer, color, true);
}

// Kludge function definition hack for the cross-module hook original call.
static void _fastcall AddOutputText(Zeal::GameUI::ChatWnd *wnd, int u, Zeal::GameUI::CXSTR msg, short channel){};

void print_chat_wnd(Zeal::GameUI::ChatWnd *wnd, short color, const char *format, ...) {
  va_list argptr;
  char buffer[512];
  va_start(argptr, format);
  // printf()
  vsnprintf(buffer, 511, format, argptr);
  va_end(argptr);
  if (!is_in_game()) {
    ZealService::get_instance()->queue_chat_message(buffer);
    return;
  }

  Zeal::GameUI::CXSTR cxBuff(buffer);  // Callers of AddOutputText() must FreeRep().
  ZealService::get_instance()->hooks->hook_map["AddOutputText"]->original(AddOutputText)(wnd, 0, cxBuff, color);
  cxBuff.FreeRep();  // Required here to match client behavior calling AddOutputText.
}

void destroy_held() {
  // Call void CInventoryWnd::DestroyHeld(void) which is effectively a static and doesn't use the
  // class pointer. This will either destroy the cursor item immediately if fast destroy is set
  // or it will pop up a confirmation dialog.
  auto inventory_wnd = Zeal::Game::Windows->Inventory;
  reinterpret_cast<void(__fastcall *)(Zeal::GameUI::SidlWnd *, int unused_edx)>(0x00421637)(inventory_wnd, 0);
}

int get_gamestate() {
  if (get_game()) return get_game()->game_state;
  return -1;
}

GameStructures::GameClass *get_game() { return *reinterpret_cast<GameStructures::GameClass **>(0x00809478); }

GameStructures::Display *get_display() { return *reinterpret_cast<GameStructures::Display **>(0x007F9510); }

const char *get_ui_skin() { return reinterpret_cast<const char *>(0x0063D3C0); }

std::string get_ui_ini_filename() {
  // First try to use client's function (GetUIIniFilename) to retrieve it.
  const char *ui_ini_file = reinterpret_cast<const char *(__cdecl *)(void)>(0x00437481)();
  if (ui_ini_file && ui_ini_file[0]) return ui_ini_file;

  Zeal::GameStructures::GAMECHARINFO *c = Zeal::Game::get_char_info();
  if (c) {
    std::string char_name = c->Name;
    return ".\\UI_" + char_name + "_pq.proj.ini";  // Note: Client uses ".\\" and not some path lookup.
  }
  return "";
}

std::string get_host_tag() {
  // Try to use client's function (GetUIIniFilename) to retrieve the filesafe host description.
  const char *ui_ini_file = reinterpret_cast<const char *(__cdecl *)(void)>(0x00437481)();
  if (ui_ini_file && ui_ini_file[0]) {
    std::string ini_file = std::string(ui_ini_file);
    std::regex pattern(".*_([^_]+)\\.ini$");  // Captures all chars after last underscore before .ini.
    std::smatch matches;
    if (std::regex_search(ini_file, matches, pattern) && matches.size() > 1) return "_" + matches[1].str();
  }
  return "";
}

std::filesystem::path get_game_path() {
  static std::filesystem::path executable_path;

  if (!executable_path.empty()) return executable_path;

  // Retrieve the full path to the game executable.
  std::array<wchar_t, MAX_PATH> buffer = {0};
  DWORD length = GetModuleFileNameW(NULL, buffer.data(), buffer.size());
  if (length > 0 && length < MAX_PATH)
    executable_path = std::filesystem::path(buffer.data()).parent_path();  // Drop the exe name.
  else
    executable_path = std::filesystem::current_path();  // Fallback if that fails.

  return executable_path;
}

std::filesystem::path get_default_ui_skin_path() {
  return get_game_path() / std::filesystem::path("uifiles") / std::filesystem::path("default");
}

int get_channel_number(const char *name)  // ChannelServerApi::GetChannelNumber()
{
  auto game = get_game();
  if (!game || !game->ChannelServerApi) return -1;
  return reinterpret_cast<int(__fastcall *)(void *ChannelServerApi, int unused_edx, const char *name)>(0x0049cdaf)(
      game->ChannelServerApi, 0, name);
}

void do_join(Zeal::GameStructures::Entity *player, const char *name) {
  reinterpret_cast<void(__cdecl *)(Zeal::GameStructures::Entity *, const char *)>(0x00500106)(player, name);
}

void send_to_channel(int chat_channel_zero_based, const char *message) {
  reinterpret_cast<void(__cdecl *)(int, const char *)>(0x00500266)(chat_channel_zero_based + 1, message);
}

void do_inspect(Zeal::GameStructures::Entity *player) { get_game()->doInspect(player); }

void pet_command(int cmd, short spawn_id) { get_game()->IssuePetCommand(cmd, spawn_id); }

void execute_cmd(UINT cmd, int isdown, int unk2) {
  reinterpret_cast<void(__cdecl *)(UINT, int, int)>(0x54050c)(cmd, isdown, unk2);
}

std::string generateTimestamp() {
  time_t rawtime;
  struct tm timeinfo;
  time(&rawtime);
  localtime_s(&timeinfo, &rawtime);

  std::ostringstream oss;
  oss << std::put_time(&timeinfo, "%Y-%m-%d_%H-%M-%S");
  return oss.str();
}

int get_effect_required_level(Zeal::GameStructures::GAMEITEMINFO *item) {
  if (!item || item->Type != 0) return 0;
  if (item->Common.Skill == 21 || item->Common.Skill == 42 || item->Common.Skill == 20)  // potion, poison, scroll
    return 0;
  if (item->Common.SpellIdEx < 1 || item->Common.SpellIdEx >= 4000) return 0;
  switch (item->Common.IsStackableEx) {
    case Zeal::GameEnums::ItemEffectCombatProc:
    case Zeal::GameEnums::ItemEffectMustEquipClick:
    case Zeal::GameEnums::ItemEffectCanEquipClick:
      return item->Common.CastingLevelEx;
    case Zeal::GameEnums::ItemEffectClick:
    case Zeal::GameEnums::ItemEffectExpendable:
      return 0;
  }
  return 0;
}

bool use_item(int item_index, bool quiet) {
  Zeal::GameStructures::GAMECHARINFO *chr = Zeal::Game::get_char_info();
  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  if (!chr || !self) {
    Zeal::Game::print_chat(USERCOLOR_SHOUT, "[Fatal Error] Failed to get charinfo for useitem!");
    return false;
  }
  Zeal::GameStructures::GAMEITEMINFO *item = nullptr;
  if (item_index < 0 || item_index > 29) {
    Zeal::Game::print_chat("useitem requires an item slot between 0 and 29, you tried to use %i", item_index);
    return false;
  }
  if (item_index < 21)
    item = chr->InventoryItem[item_index];
  else
    item = chr->InventoryPackItem[item_index - 22];  //-22 to make it back to 0 index

  if (!item) {
    if (!quiet) Zeal::Game::print_chat("There isn't an item at %i", item_index);
    return false;
  }
  if (item->Type != 0 || !item->Common.SpellId) {
    if (!quiet) Zeal::Game::print_chat("Item %s does not have a spell attached to it.", item->Name);
    return false;
  }
  // List of checks copied from server zone/client_packet.cpp:
  if ((item->Common.EffectType != Zeal::GameEnums::ItemEffect::ItemEffectClick) &&
      (item->Common.EffectType != Zeal::GameEnums::ItemEffect::ItemEffectExpendable) &&
      (item->Common.EffectType != Zeal::GameEnums::ItemEffect::ItemEffectMustEquipClick) &&
      (item->Common.EffectType != Zeal::GameEnums::ItemEffect::ItemEffectCanEquipClick)) {
    if (!quiet) Zeal::Game::print_chat("Item %s does not have a click effect.", item->Name);
    return false;
  }
  if (!self->ActorInfo || self->ActorInfo->CastingSpellId != kInvalidSpellId) {
    Zeal::Game::print_chat(USERCOLOR_SPELLS, "You must stop casting to cast this spell!");
    return false;
  }
  if (self->StandingState != Stance::Stand &&
      self->StandingState != Stance::Sit)  // game.dll casting has autostand while sitting
  {
    Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "You must be standing to cast a spell.");
    return false;
  }
  chr->cast(0xA, 0, (int *)&item, item_index < 21 ? item_index + 1 : item_index);
  return true;
}

bool is_autoattacking() { return *reinterpret_cast<BYTE *>(0x007f6ffe); }

Zeal::GameStructures::Entity *get_active_corpse() {
  return *(Zeal::GameStructures::Entity **)Zeal::Game::Active_Corpse;
}

Zeal::GameStructures::Entity *get_target() { return *(Zeal::GameStructures::Entity **)Zeal::Game::Target; }

void set_target(Zeal::GameStructures::Entity *target) {
  auto old_target = Zeal::Game::get_target();
  if (old_target && !target) print_chat(get_string(0x3057));  // you no longer have a target
  *(Zeal::GameStructures::Entity **)Zeal::Game::Target = target;

  // Note: The change in target will get detected in game Character::DoPassageOfTime() which
  // is called by the RenderReal_World() code. That sends a TargetMouse opcode (0x4162)
  // that will keep the server in sync and also trigger a target HP update packet response.
}

void do_target(const char *name) { reinterpret_cast<void(__cdecl *)(int, const char *)>(0x4FD9A7)(0, name); }

void do_consider(Zeal::GameStructures::Entity *entity, const char *str) {
  reinterpret_cast<void(__cdecl *)(Zeal::GameStructures::Entity *, const char *)>(0x004f6364)(entity, str);
}

Zeal::GameStructures::Entity *get_entity_list() { return *(Zeal::GameStructures::Entity **)Zeal::Game::EntListPtr; }

bool get_attack_on_assist() { return *Zeal::Game::attack_on_assist != 0; }

void set_attack_on_assist(bool enable) { *Zeal::Game::attack_on_assist = enable; }

long get_user_color(int index) {
  index -= 1;
  long _param_1 = reinterpret_cast<long(__cdecl *)(int)>(0x4AA2C1)(index);
  if (_param_1 == 0) return 0xFFFFFFFF;
  return (_param_1 & 0xff00 | _param_1 >> 0x10 & 0xff | (_param_1 | 0xffffff00) << 0x10);
}

Zeal::GameStructures::Entity *get_entity_by_id(short id) {
  if ((id < 0) || (id >= kEntityIdArraySize)) return nullptr;
  return EntityIdArray[id];
}

Zeal::GameStructures::Entity *get_entity_by_parent_id(short parent_id) {
  Zeal::GameStructures::Entity *current_ent = get_entity_list();
  while (current_ent) {
    if (current_ent->PetOwnerSpawnId == parent_id) return current_ent;
    current_ent = current_ent->Next;
  }
  return 0;
}

Zeal::GameStructures::SPELLMGR *get_spell_mgr() { return *(Zeal::GameStructures::SPELLMGR **)0x805CB0; }

int get_spell_level(int spell_id) {
  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();
  const auto *spell_mgr = Zeal::Game::get_spell_mgr();
  if (spell_id < 1 || spell_id >= GAME_NUM_SPELLS || !spell_mgr || !char_info) return -1;

  const auto *spell = spell_mgr->Spells[spell_id];
  if (!spell) return -1;

  return spell->ClassLevel[char_info->Class];
}

const char *get_spell_name(int spell_id) {
  const auto *spell_mgr = Zeal::Game::get_spell_mgr();
  if (spell_id < 1 || spell_id >= GAME_NUM_SPELLS || !spell_mgr) return nullptr;

  const auto *spell = spell_mgr->Spells[spell_id];
  if (!spell) return nullptr;

  return spell->Name;
}

void dump_spell_info(int spell_id) {
  const auto *spell_mgr = Zeal::Game::get_spell_mgr();
  if (spell_id < 1 || spell_id >= GAME_NUM_SPELLS || !spell_mgr) {
    print_chat("Invalid spell id: %d", spell_id);
    return;
  };

  const auto *spell = spell_mgr->Spells[spell_id];
  if (!spell) {
    print_chat("Null spell at id: %d", spell_id);
    return;
  };

  print_chat("[%d]: %s:, SpellType: %d, TargetType: %d", spell_id, spell->Name, spell->SpellType, spell->TargetType);
  for (int i = 0; i < 12; ++i) {
    if (spell->Attrib[i] == 254) continue;  // Inactive effect slot.
    print_chat("[Idx %d]: Id: %d:, Formula: %d, Base: %d, Max: %d,", i, spell->Attrib[i], spell->Calc[i],
               spell->Base[i], spell->Max[i]);
  }

  const bool effect_debug = true;  // Optional extra debug spam.
  if (effect_debug) {
    print_chat("[%d]: SpellAffectIndex: %d @ 0x%08x, SpellAnim: %d @ 0x%08x", spell_id, spell->SpellAffectIndex,
               (int)spell->OldParticleEffect, spell->SpellAnim, (int)spell->NewParticleEffect);
    for (int ii = 0; ii < 3; ++ii) {
      if (spell->OldParticleEffect->subEffect[ii].effectMode >= 0)
        print_chat("[%d][%d]: effectMode: %d, Name[0]: %s", spell_id, ii,
                   spell->OldParticleEffect->subEffect[ii].effectMode,
                   spell->OldParticleEffect->subEffect[ii].blitSprite[0]);
    }
  }
}

Zeal::GameStructures::Entity *get_self() { return *(Zeal::GameStructures::Entity **)Zeal::Game::Self; }

Zeal::GameStructures::Entity *get_pet() {
  Zeal::GameStructures::Entity *pet_entity = NULL;
  Zeal::GameStructures::Entity *self = get_self();

  if (self && self->ActorInfo) {
    int pet_position = self->ActorInfo->PetID;
    if (pet_position) {
      pet_entity = Zeal::Game::get_entity_by_id(pet_position);
    }
  }
  return pet_entity;
}

Zeal::GameStructures::Entity *get_controlled() {
  return *(Zeal::GameStructures::Entity **)Zeal::Game::_ControlledPlayer;
}

Zeal::GameStructures::CameraInfo *get_camera() { return *(Zeal::GameStructures::CameraInfo **)Zeal::Game::CameraInfo; }

bool is_mouse_hovering_window() { return *Zeal::Game::mouse_hover_window != 0; }

// void set_camera_position(Vec3* pos)
//{
//	int cam_position_ptr = *(int*)((*(int*)Zeal::Game::Display) + 0x8);
//	Vec3* cam_pos = (Vec3*)(cam_position_ptr) + 0xC);
//	*cam_pos = *pos;
// }

void CXStr_PrintString(Zeal::GameUI::CXSTR *str, const char *format, ...) {
  va_list argptr;
  char buffer[512];
  va_start(argptr, format);
  // printf()
  vsnprintf(buffer, 511, format, argptr);
  va_end(argptr);

  GameInternal::CXStr_PrintString(str, buffer);
}

const char *get_aa_title_name(BYTE class_id, int aa_rank, BYTE gender_id) {
  const char *title_name = "";
  switch (class_id) {
    case Zeal::GameEnums::ClassTypes::Warrior:
      if (aa_rank == 1 && gender_id == 0) title_name = "Barron";
      if (aa_rank == 1 && gender_id == 1) title_name = "Barroness";
      if (aa_rank == 2) title_name = "Veteran";
      if (aa_rank == 3) title_name = "Marshall";
      break;
    case Zeal::GameEnums::ClassTypes::Cleric:
      if (aa_rank == 1 && gender_id == 0) title_name = "Barron";
      if (aa_rank == 1 && gender_id == 1) title_name = "Barroness";
      if (aa_rank == 2) title_name = "Venerable";
      if (aa_rank == 3) title_name = "Exarch";
      break;
    case Zeal::GameEnums::ClassTypes::Paladin:
      if (aa_rank == 1 && gender_id == 0) title_name = "Barron";
      if (aa_rank == 1 && gender_id == 1) title_name = "Barroness";
      if (aa_rank == 2 && gender_id == 0) title_name = "Sir";
      if (aa_rank == 2 && gender_id == 1) title_name = "Lady";
      if (aa_rank == 3 && gender_id == 0) title_name = "Duke";
      if (aa_rank == 3 && gender_id == 1) title_name = "Duchess";
      break;
    case Zeal::GameEnums::ClassTypes::Ranger:
      if (aa_rank == 1 && gender_id == 0) title_name = "Barron";
      if (aa_rank == 1 && gender_id == 1) title_name = "Barroness";
      if (aa_rank == 2) title_name = "Veteran";
      if (aa_rank == 3 && gender_id == 0) title_name = "Hunter";
      if (aa_rank == 3 && gender_id == 1) title_name = "Huntress";
      break;
    case Zeal::GameEnums::ClassTypes::Shadowknight:
      if (aa_rank == 1 && gender_id == 0) title_name = "Barron";
      if (aa_rank == 1 && gender_id == 1) title_name = "Barroness";
      if (aa_rank == 2 && gender_id == 0) title_name = "Sir";
      if (aa_rank == 2 && gender_id == 1) title_name = "Lady";
      if (aa_rank == 3 && gender_id == 0) title_name = "Duke";
      if (aa_rank == 3 && gender_id == 1) title_name = "Duchess";
      break;
    case Zeal::GameEnums::ClassTypes::Druid:
      if (aa_rank == 1 && gender_id == 0) title_name = "Barron";
      if (aa_rank == 1 && gender_id == 1) title_name = "Barroness";
      if (aa_rank == 2) title_name = "Venerable";
      if (aa_rank == 3) title_name = "Elder";
      break;
    case Zeal::GameEnums::ClassTypes::Monk:
      if (aa_rank == 1 && gender_id == 0) title_name = "Barron";
      if (aa_rank == 1 && gender_id == 1) title_name = "Barroness";
      if (aa_rank == 2 && gender_id == 0) title_name = "Brother";
      if (aa_rank == 2 && gender_id == 1) title_name = "Sister";
      if (aa_rank == 3) title_name = "Sensei";
      break;
    case Zeal::GameEnums::ClassTypes::Bard:
      if (aa_rank == 1 && gender_id == 0) title_name = "Barron";
      if (aa_rank == 1 && gender_id == 1) title_name = "Barroness";
      if (aa_rank == 2) title_name = "Veteran";
      if (aa_rank == 3 && gender_id == 0) title_name = "Impresario";
      if (aa_rank == 3 && gender_id == 1) title_name = "Muse";
      break;
    case Zeal::GameEnums::ClassTypes::Rogue:
      if (aa_rank == 1 && gender_id == 0) title_name = "Barron";
      if (aa_rank == 1 && gender_id == 1) title_name = "Barroness";
      if (aa_rank == 2) title_name = "Veteran";
      if (aa_rank == 3) title_name = "Marauder";
      break;
    case Zeal::GameEnums::ClassTypes::Shaman:
      if (aa_rank == 1 && gender_id == 0) title_name = "Barron";
      if (aa_rank == 1 && gender_id == 1) title_name = "Barroness";
      if (aa_rank == 2) title_name = "Venerable";
      if (aa_rank == 3) title_name = "Elder";
      break;
    case Zeal::GameEnums::ClassTypes::Necromancer:
      if (aa_rank == 1 && gender_id == 0) title_name = "Barron";
      if (aa_rank == 1 && gender_id == 1) title_name = "Barroness";
      if (aa_rank == 2) title_name = "Sage";
      if (aa_rank == 3) title_name = "Lich";
      break;
    case Zeal::GameEnums::ClassTypes::Wizard:
      if (aa_rank == 1 && gender_id == 0) title_name = "Barron";
      if (aa_rank == 1 && gender_id == 1) title_name = "Barroness";
      if (aa_rank == 2 && gender_id == 0) title_name = "Master";
      if (aa_rank == 2 && gender_id == 1) title_name = "Mistress";
      if (aa_rank == 3) title_name = "Sage";
      break;
    case Zeal::GameEnums::ClassTypes::Magician:
      if (aa_rank == 1 && gender_id == 0) title_name = "Barron";
      if (aa_rank == 1 && gender_id == 1) title_name = "Barroness";
      if (aa_rank == 2 && gender_id == 0) title_name = "Master";
      if (aa_rank == 2 && gender_id == 1) title_name = "Mistress";
      if (aa_rank == 3) title_name = "Sage";
      break;
    case Zeal::GameEnums::ClassTypes::Enchanter:
      if (aa_rank == 1 && gender_id == 0) title_name = "Barron";
      if (aa_rank == 1 && gender_id == 1) title_name = "Barroness";
      if (aa_rank == 2 && gender_id == 0) title_name = "Master";
      if (aa_rank == 2 && gender_id == 1) title_name = "Mistress";
      if (aa_rank == 3) title_name = "Sage";
      break;
    case Zeal::GameEnums::ClassTypes::Beastlord:
      if (aa_rank == 1 && gender_id == 0) title_name = "Barron";
      if (aa_rank == 1 && gender_id == 1) title_name = "Barroness";
      if (aa_rank == 2) title_name = "Venerable";
      if (aa_rank == 3) title_name = "Elder";
      break;
    default:
      title_name = "";
      break;
  }
  return title_name;
}

std::string class_name(int class_id) {
  std::string class_string = "";
  int modified_class_id = class_id;
  if (modified_class_id > 16 && modified_class_id < 32) modified_class_id -= 16;
  switch (modified_class_id) {
    case Zeal::GameEnums::ClassTypes::Warrior:
      class_string = "Warrior";
      break;
    case Zeal::GameEnums::ClassTypes::Cleric:
      class_string = "Cleric";
      break;
    case Zeal::GameEnums::ClassTypes::Paladin:
      class_string = "Paladin";
      break;
    case Zeal::GameEnums::ClassTypes::Ranger:
      class_string = "Ranger";
      break;
    case Zeal::GameEnums::ClassTypes::Shadowknight:
      class_string = "Shadowknight";
      break;
    case Zeal::GameEnums::ClassTypes::Druid:
      class_string = "Druid";
      break;
    case Zeal::GameEnums::ClassTypes::Monk:
      class_string = "Monk";
      break;
    case Zeal::GameEnums::ClassTypes::Bard:
      class_string = "Bard";
      break;
    case Zeal::GameEnums::ClassTypes::Rogue:
      class_string = "Rogue";
      break;
    case Zeal::GameEnums::ClassTypes::Shaman:
      class_string = "Shaman";
      break;
    case Zeal::GameEnums::ClassTypes::Necromancer:
      class_string = "Necromancer";
      break;
    case Zeal::GameEnums::ClassTypes::Wizard:
      class_string = "Wizard";
      break;
    case Zeal::GameEnums::ClassTypes::Magician:
      class_string = "Magician";
      break;
    case Zeal::GameEnums::ClassTypes::Enchanter:
      class_string = "Enchanter";
      break;
    case Zeal::GameEnums::ClassTypes::Beastlord:
      class_string = "Beastlord";
      break;
    case Zeal::GameEnums::ClassTypes::Banker:
      class_string = "Banker";
      break;
    case Zeal::GameEnums::ClassTypes::Merchant:
      class_string = "Merchant";
      break;
    default:
      class_string = "Unknown";
      break;
  }
  if (class_id > 16 && class_id < 32) class_string += " GuildMaster";
  return class_string;
}

std::string class_name_short(int class_id) {
  std::string class_string = "";
  int modified_class_id = class_id;
  if (modified_class_id > 16 && modified_class_id < 32) modified_class_id -= 16;
  switch (modified_class_id) {
    case Zeal::GameEnums::ClassTypes::Warrior:
      class_string = "War";
      break;
    case Zeal::GameEnums::ClassTypes::Cleric:
      class_string = "Clr";
      break;
    case Zeal::GameEnums::ClassTypes::Paladin:
      class_string = "Pal";
      break;
    case Zeal::GameEnums::ClassTypes::Ranger:
      class_string = "Rng";
      break;
    case Zeal::GameEnums::ClassTypes::Shadowknight:
      class_string = "Shd";
      break;
    case Zeal::GameEnums::ClassTypes::Druid:
      class_string = "Dru";
      break;
    case Zeal::GameEnums::ClassTypes::Monk:
      class_string = "Mnk";
      break;
    case Zeal::GameEnums::ClassTypes::Bard:
      class_string = "Brd";
      break;
    case Zeal::GameEnums::ClassTypes::Rogue:
      class_string = "Rog";
      break;
    case Zeal::GameEnums::ClassTypes::Shaman:
      class_string = "Shm";
      break;
    case Zeal::GameEnums::ClassTypes::Necromancer:
      class_string = "Nec";
      break;
    case Zeal::GameEnums::ClassTypes::Wizard:
      class_string = "Wiz";
      break;
    case Zeal::GameEnums::ClassTypes::Magician:
      class_string = "Mag";
      break;
    case Zeal::GameEnums::ClassTypes::Enchanter:
      class_string = "Enc";
      break;
    case Zeal::GameEnums::ClassTypes::Beastlord:
      class_string = "Bst";
      break;
    case Zeal::GameEnums::ClassTypes::Banker:
      class_string = "Banker";
      break;
    case Zeal::GameEnums::ClassTypes::Merchant:
      class_string = "Merchant";
      break;
    default:
      class_string = "Unknown";
      break;
  }
  if (class_id > 16 && class_id < 32) class_string += " GuildMaster";
  return class_string;
}

int get_showname() {
  // Holds value of /showname command.
  //  1 = first names, 2 = first/last names, 3 = first/last/guild names, 4 = everything
  //  5 = title/first names 6. title/first/last names 7. first/guild names
  return *reinterpret_cast<int32_t *>(0x007d01e4);
}

int get_show_pc_names() {
  // Holds value of Options -> Display -> Show PC Names
  // 0 = off, 1 = on
  return *reinterpret_cast<int *>(0x0063D6C8);
}

int get_show_npc_names() {
  // Holds value of Options -> Display -> Show NPC Names
  // 0 = off, 1 = on
  return *reinterpret_cast<int *>(0x0063D6CC);
}

std::string get_full_zone_name(int zone_id) {
  const int fn_GetFullZoneName = 0x00523e49;
  void *pWorld = *reinterpret_cast<void **>(0x007F9494);
  char buffer[512];  // LongName is only 0x80 in GAMEZONEINFO.
  buffer[0] = 0;     // Returns "Unknown Zone" if invalid index
  reinterpret_cast<void(__thiscall *)(void *this_world, int zone_id, char *buffer)>(fn_GetFullZoneName)(pWorld, zone_id,
                                                                                                        buffer);
  return std::string(buffer);
}

std::string get_zone_name_from_index(int zone_id) {
  const int fn_GetZoneNameFromIndex = 0x00523f73;
  void *pWorld = *reinterpret_cast<void **>(0x007F9494);
  char buffer[512];  // ShortName is only 0x20 in GAMEZONEINFO.
  if (!reinterpret_cast<bool(__thiscall *)(void *this_world, int zone_id, char *buffer)>(fn_GetZoneNameFromIndex)(
          pWorld, zone_id, buffer))
    buffer[0] = 0;  // Return an empty buffer if not found.
  return std::string(buffer);
}

int get_index_from_zone_name(const std::string &name) {
  const int fn_GetIndexFromZoneName = 0x00523fa4;  // Returns 0 if not found.
  void *pWorld = *reinterpret_cast<void **>(0x007F9494);
  return reinterpret_cast<int(__thiscall *)(void *this_world, const char *buffer)>(fn_GetIndexFromZoneName)(
      pWorld, name.c_str());
}

std::string get_class_desc(int class_id) { return std::string(Zeal::Game::get_game()->GetClassDesc(class_id)); }

std::string get_title_desc(int class_id, int aa_rank, int gender) {
  return std::string(Zeal::Game::get_game()->GetTitleDesc(class_id, aa_rank, gender));
}

std::string get_player_guild_name(short guild_id) {
  // Roughly equivalent to:
  // return (guild_id == -1) ? "Unknown" : Zeal::Game::guild_names->Guild[guildId].Name;
  const int fn_GetPlayerGuildName = 0x0054c7e1;
  auto desc = reinterpret_cast<const char *(*)(short)>(fn_GetPlayerGuildName)(guild_id);
  return std::string(desc);
}

bool is_in_game() { return get_gamestate() == GAMESTATE_INGAME; }

bool is_in_char_select() { return get_gamestate() == GAMESTATE_CHARSELECT; }

bool is_new_ui() { return *(BYTE *)0x8092D8; }

HWND get_game_window() {
  HMODULE dll = GetModuleHandleA("eqw.dll");
  if (!dll) return nullptr;

  // First check if we are using the newer eqw-takp.
  FARPROC fn = GetProcAddress(dll, "GetGameWindow");
  if (fn) return reinterpret_cast<HWND(__cdecl *)()>(fn)();

  // Backwards compatibility for the original EQW beta 2.32.
  return *(HWND *)((DWORD)dll + 0x97A4);
}

namespace Spells {
void OpenBook() {
  if (!Windows->SpellBook) return;
  Windows->SpellBook->Activate();
}

void StopSpellBookAction() {
  if (!Windows->SpellBook) return;
  Windows->SpellBook->StopSpellBookAction();
}

void Memorize(int book_index, int gem_index) {
  if (!Windows->SpellBook) return;
  if (!Windows->SpellBook->Activated) Windows->SpellBook->Activate();
  if (!Windows->SpellBook->Activated) return;
  ZealService::get_instance()->callbacks->AddDelayed(
      [book_index, gem_index]() {
        if (Windows->SpellBook && Windows->SpellBook->Activated &&
            (Zeal::Game::get_self()->StandingState == Stance::Sit || Zeal::Game::is_mounted()))
          Windows->SpellBook->BeginMemorize(book_index, gem_index, false);
      },
      25);
}

void Forget(int index) {
  if (!Windows->SpellGems) return;
  Windows->SpellGems->Forget(index);
}

void UpdateGems(int index) {
  if (!Windows->SpellGems) return;
  Windows->SpellGems->UpdateSpellGems(index);
}

bool IsValidSpellIndex(int spellid) { return reinterpret_cast<bool(__cdecl *)(int)>(0x004D79EA)(spellid); }
}  // namespace Spells

namespace OldUI {
bool spellbook_window_open() {
  // ISSUE: There is currently a small edge case where chat scrollbar usage can cause the value we're checking to
  // flicker. ISSUE: Spamming chat while in spellboolk ultimately causes chat to scroll which makes the value flicker
  // like the above issue.
  HMODULE dx8 = GetModuleHandleA("eqgfx_dx8.dll");
  // feedback/help window increase offset of pointer by 44, but they also get hit by game_wants_input(), so don't
  // bother checking them.
  if (dx8) {
    int offset = GAME_NUM_SPELL_GEMS * 88;
    for (size_t i = 0; i < GAME_NUM_SPELL_GEMS; ++i)
      if (Zeal::Game::get_char_info()->MemorizedSpell[i] == -1) offset -= 88;

    if (Zeal::Game::get_target()) {
      offset += 44;
    }
    bool view_button_clicked =
        *(DWORD *)((DWORD)dx8 + (0x3CD1C4 + offset)) != ULONG_MAX;  // weird offset edge case (view hotkey not included)
    if (view_button_clicked) {
      offset += 44;
    }
    return *(DWORD *)((DWORD)dx8 + (0x3CD1C4 + offset)) == ULONG_MAX;
  } else {
    return false;
  }
}
}  // namespace OldUI

// Primitive comparison with simple numeric string support.
static bool sort_list_wnd_compare(const std::vector<std::string> &a, const std::vector<std::string> &b,
                                  int sort_column) {
  int a_num = std::atoi(a[sort_column].c_str());
  int b_num = std::atoi(b[sort_column].c_str());
  if (a_num != 0 && b_num != 0)  // Both are non-zero numerics strings, so numeric comparison.
    return a_num < b_num;
  return a[sort_column] < b[sort_column];  // Else use default string comparison.
}

// Sorts the contents of a list window by the sort_column index and sort_type
void sort_list_wnd(Zeal::GameUI::ListWnd *list_wnd, int sort_column, SortType sort_type) {
  if (!list_wnd) return;

  const int num_rows = list_wnd->ItemCount;
  if (num_rows < 2) return;

  // The CListWnd::SetItemText() code showed that the number of columns was accessed
  // by a pointer at +0xfc that points to an array of 28 byte structures with the column
  // value in the second integer index.
  if (list_wnd->ColInfoArray == nullptr) return;
  const int num_cols = list_wnd->ColInfoArray[0].ColCount;
  if (sort_column < 0 || sort_column >= num_cols) return;

  // Copy data into a standard 2-D structure for sorting.
  std::vector<std::vector<std::string>> data;
  for (int r = 0; r < num_rows; ++r) {
    // Temporary logging / sanity check that there are equal number of columns for each row.
    if (list_wnd->ColInfoArray[r].ColCount != num_cols) {
      Zeal::Game::print_chat("Sorting column mismatch: %d vs %d", num_cols, list_wnd->ColInfoArray[r].ColCount);
      return;
    }
    data.push_back(std::vector<std::string>());
    for (int c = 0; c < num_cols; ++c) data[r].push_back(list_wnd->GetItemText(r, c));
  }

  bool ascending = (sort_type == SortType::Ascending) ||
                   (sort_type == SortType::Toggle && sort_list_wnd_compare(data.back(), data.front(), sort_column));
  if (ascending)
    std::sort(data.begin(), data.end(),
              [sort_column](const std::vector<std::string> &a, const std::vector<std::string> &b) {
                return sort_list_wnd_compare(a, b, sort_column);
              });
  else
    std::sort(data.rbegin(), data.rend(),
              [sort_column](const std::vector<std::string> &a, const std::vector<std::string> &b) {
                return sort_list_wnd_compare(a, b, sort_column);
              });

  for (int r = 0; r < num_rows; ++r) {
    for (int c = 0; c < num_cols; ++c) list_wnd->SetItemText(data[r][c], r, c);
  }
}

short total_spell_affects(Zeal::GameStructures::GAMECHARINFO *char_info, BYTE affect_type, BYTE a3,
                          int *per_buff_values) {
  return reinterpret_cast<short(__thiscall *)(Zeal::GameStructures::GAMECHARINFO *, BYTE, BYTE, int *)>(0x4C6B6D)(
      char_info, affect_type, a3, per_buff_values);
}

void sit()  // Using game ::Sit() logic here, but without the sit/stand toggle
{
  if (!Zeal::Game::is_in_game()) return;
  Zeal::GameStructures::Entity *entity = get_self();
  Zeal::GameStructures::GAMECHARINFO *char_info = get_char_info();
  if (!entity || !char_info) return;
  if (total_spell_affects(char_info, 31, 1, 0) != 0)  // SE_Mez (31)
    return;
  if (entity->StandingState == Stance::NotInControl) return;
  if (char_info->StandingState == Stance::Stand || char_info->StandingState == Stance::Duck)
    entity->ChangeStance(Stance::Sit);
}

void dismount()  // Same as /dismount
{
  reinterpret_cast<void(__cdecl *)(int)>(0x004ff5a6)(1);  // do_dismount(1).
}

bool is_mounted() {
  return Zeal::Game::get_self() && Zeal::Game::get_self()->ActorInfo && Zeal::Game::get_self()->ActorInfo->Mount;
}

bool is_a_mount(const Zeal::GameStructures::Entity *entity) {
  return entity && entity->ActorInfo && entity->ActorInfo->Rider &&
         entity->ActorInfo->Rider->Type == Zeal::GameEnums::EntityTypes::Player;
}

// game.dll patch support that expanded the number of available bank slots.
int get_num_personal_bank_slots() {
  // The following patch modification could be used to dynamically check the number
  // of available bank slots:
  // Displays contents of bank bags past 8
  // PatchT(0x423191 + 2, (BYTE)MAX_BANK_SLOTS); // CInvSlotMgr::UpdateSlots_423089

  // For the short-term, we are relying on the total call to check for the patch.
  if (get_num_total_bank_slots() == 60) return 30;
  return GAME_NUM_INVENTORY_BANK_SLOTS;  // Fallback to the safe default.
}

int get_num_shared_bank_slots() { return max(0, min(30, get_num_total_bank_slots() - get_num_personal_bank_slots())); }

int get_num_total_bank_slots() {
  // Sanity check the game charinfo structure is the patched version.
  if (sizeof(Zeal::GameStructures::GAMECHARINFO) != 0x21a4)
    return GAME_NUM_INVENTORY_BANK_SLOTS;  // Fallback to the safe default.

  // First read the patched total number of bank slots.
  // PatchT(0x4CE982 + 3, (int)TotalBagSlotsAtEndOfPlayerProfile); // GameCharInfo Destructor
  int total_number = *reinterpret_cast<int *>(0x4ce985);

  // And also sanity check the char_info_size allocation matches.
  // PatchT(0x40B036 + 1, (int)new_charinfo_size); // CCharacterCreation::CCharacterCreation_40AB77
  int char_info_size = *reinterpret_cast<int *>(0x40b037);

  if (total_number == 60 && char_info_size == sizeof(Zeal::GameStructures::GAMECHARINFO)) return 60;

  return GAME_NUM_INVENTORY_BANK_SLOTS;  // Fallback to the safe default.
}

// Counts up the number of open (empty)
int get_num_empty_inventory_slots() {
  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();
  if (!char_info) return 0;

  // Do not include gear slots.
  int empty = 0;
  for (int i = 0; i < GAME_NUM_INVENTORY_PACK_SLOTS; ++i) {
    const auto pack_slot = char_info->InventoryPackItem[i];
    if (!pack_slot)
      empty++;
    else if (pack_slot->Type == 1 && pack_slot->Container.Capacity <= GAME_NUM_CONTAINER_SLOTS)
      for (int j = 0; j < pack_slot->Container.Capacity; ++j)
        if (!pack_slot->Container.Item[j]) empty++;
  }
  return empty;
}

// Counts up the number of inventory slots.
int get_num_inventory_slots() {
  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();
  if (!char_info) return 0;

  // Do not include gear slots.
  int count = 0;
  for (int i = 0; i < GAME_NUM_INVENTORY_PACK_SLOTS; ++i) {
    const auto pack_slot = char_info->InventoryPackItem[i];
    if (!pack_slot || pack_slot->Type != 1)
      count++;
    else
      count += pack_slot->Container.Capacity;
  }
  return count;
}

// Returns the avoidance value used in combat calculations. The server includes the combat
// agility AA bonus for combat, while the displayed client AC does not.
int get_avoidance(bool include_combat_agility) {
  auto char_info = Zeal::Game::get_char_info();
  if (!char_info) return 0;
  int avoidance = char_info->compute_defense();  // Does not include CombatAgility AA.

  if (!include_combat_agility) return avoidance;

  auto self = char_info->SpawnInfo;
  auto actor_info = self ? self->ActorInfo : nullptr;
  if (!actor_info) return avoidance;

  // The server is summing CombatAgility, PhysicalEnhancement, and LightningReflexes as effect 172.
  BYTE combat_agility = actor_info->AAAbilities[0x22];  // Combat Agility AA skill = 0x22.
  int boost_percent = (combat_agility == 3) ? 10 : (combat_agility == 2) ? 5 : (combat_agility == 1) ? 2 : 0;
  if (actor_info->AAAbilities[120])                                      // Physical Enhancement AA skill.
    boost_percent += 2;                                                  // Extra 2%.
  BYTE lightning_reflexes = actor_info->AAAbilities[151];                // Lightning reflexes AA skill.
  if (lightning_reflexes <= 5) boost_percent += 3 * lightning_reflexes;  // 3% / level.
  int extra_avoidance = avoidance * boost_percent / 100;

  // The server code adds the extra avoidance before the drunk derating, so we have to
  // apply that here before adding.
  int drunk_factor = char_info->Drunkness / 2;
  if (drunk_factor > 20) extra_avoidance = extra_avoidance * (110 - drunk_factor) / 100;
  return avoidance + extra_avoidance;
}

// Mitigation has era dependence.
Era get_era() {
  auto char_info = Zeal::Game::get_char_info();
  BYTE expansions = char_info ? char_info->Expansions : 0;
  // TODO: Server is setting both the char_info field and global values so that all
  // expansions are active instead of the currently active expansion.
  expansions = 0x07;  // TODO: Hard-coded to Luclin. Check if server is now working.

  // Alternative: Use globals set by OP_ExpansionInfo to determine expansion:
  // expansions = 0;
  // if (*reinterpret_cast<DWORD*>(0x007cf1e8))  // gKunarkEnabled_007cf1e8
  //	expansions = expansions | 0x01;
  // if (*reinterpret_cast<DWORD*>(0x007cf1ec))  // gVeliousEnabled_007cf1ec
  //	expansions = expansions | 0x02;
  // if (*reinterpret_cast<DWORD*>(0x007cf1f0))  // gLuclinEnabled_007cf1f0
  //	expansions = expansions | 0x04;
  // if (*reinterpret_cast<DWORD*>(0x007cf1f4))  // gPlanesOfPowerEnabled_007cf1f4
  //	expansions = expansions | 0x08;

  if (expansions & 0x08) return Era::PlanesOfPower;
  if (expansions & 0x04) return Era::Luclin;
  if (expansions & 0x02) return Era::Velious;
  if (expansions & 0x01) return Era::Kunark;
  return Era::Classic;
}

// Helper function to calculate the softcap value based on class and era.
int get_mitigation_softcap() {
  int softcap = 350;  // AC cap is 350 for all classes in Classic era and for levels 50 and under

  auto char_info = Zeal::Game::get_char_info();
  if (!char_info) return 350;  // Return classic by default.

  if (char_info->Level > 50) {
    if (get_era() >= Era::Velious) {
      switch (char_info->Class) {
        case Zeal::GameEnums::ClassTypes::Warrior:
          softcap = 430;
          break;
        case Zeal::GameEnums::ClassTypes::Paladin:
        case Zeal::GameEnums::ClassTypes::Shadowknight:
        case Zeal::GameEnums::ClassTypes::Cleric:
        case Zeal::GameEnums::ClassTypes::Bard:
          softcap = 403;
          break;
        case Zeal::GameEnums::ClassTypes::Ranger:
        case Zeal::GameEnums::ClassTypes::Shaman:
          softcap = 375;
          break;
        case Zeal::GameEnums::ClassTypes::Monk:
          softcap = 350;  // Assuming RuleB(AlKabor, ReducedMonkAC) is false.
          break;
        default:
          softcap = 350;  // dru, rog, wiz, ench, nec, mag, bst
          break;
      }
    } else if (get_era() >= Era::Kunark && char_info->Class == Zeal::GameEnums::ClassTypes::Warrior)
      softcap = 405;
  }

  // Combat Stability related AA's that raise the softcap
  auto alt_adv_mgr = Zeal::GameStructures::AltAdvManager::get_manager();
  if (alt_adv_mgr) softcap += alt_adv_mgr->CalculateMitigationBoost(char_info, softcap);

  // Shield AC is not capped in Luclin. It is directly added w/out the 4/3 factor.
  if (get_era() >= Era::Luclin && char_info->InventoryItem[0xd] && char_info->InventoryItem[0xd]->Type == 0 &&
      char_info->InventoryItem[0xd]->Common.Skill == 8) {
    softcap += char_info->InventoryItem[0xd]->Common.ArmorClass;
  }

  return softcap;
}

// Applies the era dependent softcap and overcap calcs to the ac_sum for the final mitigation.
static int apply_mitigation_softcap(Zeal::GameStructures::GAMECHARINFO *char_info, int ac_sum, int softcap) {
  if (ac_sum <= softcap) return ac_sum;

  if (char_info->Level <= 50 || get_era() < Era::Luclin) return softcap;  // Hard-cap <= level 50 and pre-luclin.

  int overcap = ac_sum - softcap;
  int returns = 20;

  using Zeal::GameEnums::ClassTypes;
  int cls = char_info->Class;
  int level = char_info->Level;

  if (get_era() < Era::PlanesOfPower) {
    returns = 12;  // Fixed /12 for all melee in Luclin.

    if (cls == ClassTypes::Cleric || cls == ClassTypes::Druid || cls == ClassTypes::Shaman ||
        cls == ClassTypes::Wizard || cls == ClassTypes::Magician || cls == ClassTypes::Enchanter ||
        cls == ClassTypes::Necromancer)
      overcap = 0;  // melee only until PoP
  } else {
    if (cls == ClassTypes::Warrior)
      returns = (level <= 61) ? 5 : (level <= 63) ? 4 : 3;
    else if (cls == ClassTypes::Paladin || cls == ClassTypes::Shadowknight)
      returns = (level <= 61) ? 6 : (level <= 63) ? 5 : 4;
    else if (cls == ClassTypes::Bard)
      returns = (level <= 61) ? 8 : (level <= 63) ? 7 : 6;
    else if (cls == ClassTypes::Monk || cls == ClassTypes::Rogue)
      returns = 20 - max(0, (level - 61) * 2);
    else if (cls == ClassTypes::Ranger || cls == ClassTypes::Beastlord)
      returns = (level <= 61) ? 10 : (level <= 62) ? 9 : (level <= 63) ? 8 : 7;
  }
  return softcap + overcap / returns;
}

int get_mitigation(bool include_cap) {
  // The client ac call is hard-coded with planes_of_power soft-capping. Instead of trying
  // to patch that, we just call it with the cap ignored and then duplicate the server
  // code for the appropriate era.
  //
  // Note: There may be some anti-twink discrepancy for levels < 50. The client relies on a
  // per item cap using AntiTwinkLevel and AntiTwinkSkill while the server has it's own
  // formula.

  auto char_info = Zeal::Game::get_char_info();
  if (!char_info) return 0;
  int ac_sum = char_info->ac(false);  // Called w/out applying any cap.

  if (!include_cap) return ac_sum;

  int softcap = get_mitigation_softcap();
  int mitigation = apply_mitigation_softcap(char_info, ac_sum, softcap);
  return mitigation;
}

// This is the value displayed in the UI (ignores the softcap) and is not used in combat calcs.
int get_display_AC() {
  // Equivalent to game Main::CalcDefense() at 0x004712bc and is just here for reference.
  return (get_avoidance() + get_mitigation()) * 1000 / 847;
}

// Returns the level vs recommended level derating factor (typically negative "bonus").
static int calc_recommended_level_bonus(BYTE level, BYTE reclevel, int basestat) {
  if ((reclevel > 0) && (level < reclevel)) {
    int statmod = (level * 10000 / reclevel) * basestat;
    int round = (statmod < 0) ? -5000 : 5000;
    return (statmod + round) / 10000;
  }

  return 0;
}

// Returns the skill associated with the weapon. Defaults to hand to hand.
Zeal::GameEnums::SkillType get_weapon_skill(const Zeal::GameStructures::GAMEITEMINFO *weapon) {
  if (weapon && weapon->Type == 0) {
    switch (weapon->Common.Skill) {
      case Zeal::GameEnums::ItemTypes::ItemType1HSlash:
        return Zeal::GameEnums::SkillType::Skill1HSlashing;
      case Zeal::GameEnums::ItemTypes::ItemType2HSlash:
        return Zeal::GameEnums::SkillType::Skill2HSlashing;
      case Zeal::GameEnums::ItemTypes::ItemType1HPiercing:
        return Zeal::GameEnums::SkillType::Skill1HPiercing;
      case Zeal::GameEnums::ItemTypes::ItemType1HBlunt:
        return Zeal::GameEnums::SkillType::Skill1HBlunt;
      case Zeal::GameEnums::ItemTypes::ItemType2HBlunt:
        return Zeal::GameEnums::SkillType::Skill2HBlunt;
      case Zeal::GameEnums::ItemTypes::ItemType2HPiercing:
        return Zeal::GameEnums::SkillType::Skill1HPiercing;  // change to Skill2HPiercing once activated
      case Zeal::GameEnums::ItemTypes::ItemTypeMartial:
        return Zeal::GameEnums::SkillType::SkillHandtoHand;
      default:
        break;
    }
  }
  return Zeal::GameEnums::SkillType::SkillHandtoHand;
}

// Returns the default weaponless base damage.
static int get_hand_to_hand_damage(const Zeal::GameStructures::GAMECHARINFO &char_info) {
  static BYTE mnk_dmg[] = {99, 4,  4,  4,  4,  5,  5,  5,  5,  5,  6,  // 1-10
                           6,  6,  6,  6,  7,  7,  7,  7,  7,  8,      // 11-20
                           8,  8,  8,  8,  9,  9,  9,  9,  9,  10,     // 21-30
                           10, 10, 10, 10, 11, 11, 11, 11, 11, 12,     // 31-40
                           12, 12, 12, 12, 13, 13, 13, 13, 13, 14,     // 41-50
                           14, 14, 14, 14, 14, 14, 14, 14, 14, 14,     // 51-60
                           14, 14};                                    // 61-62
  static BYTE bst_dmg[] = {99, 4,  4,  4,  4,  4,  5,  5,  5,  5,  5,  // 1-10
                           5,  6,  6,  6,  6,  6,  6,  7,  7,  7,      // 11-20
                           7,  7,  7,  8,  8,  8,  8,  8,  8,  9,      // 21-30
                           9,  9,  9,  9,  9,  10, 10, 10, 10, 10,     // 31-40
                           10, 11, 11, 11, 11, 11, 11, 12, 12};        // 41-49

  if (char_info.Class == Zeal::GameEnums::ClassTypes::Monk) {
    auto hands = char_info.InventoryItem[Zeal::GameEnums::EquipSlot::Hands];
    if (hands && hands->ID == 10652 && char_info.Level > 50)  // Celestial Fists, monk epic
      return 9;
    if (char_info.Level > 62) return 15;
    return mnk_dmg[char_info.Level];
  } else if (char_info.Class == Zeal::GameEnums::ClassTypes::Beastlord) {
    if (char_info.Level > 49) return 13;
    return bst_dmg[char_info.Level];
  }
  return 2;
}

// returns the client's weapon (or hand to hand) damage
// calling this with SlotRange will also add the arrow damage
// Note: No target specific damage in this calc (bane or the like).
static int get_base_damage(int slot, const Zeal::GameStructures::GAMEITEMINFO *weapon) {
  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();
  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  if (!char_info || !self) {
    Zeal::Game::print_chat("Error: invalid self or char_info. Output will be wrong.");
    return 0;
  }

  using Zeal::GameEnums::EquipSlot::EquipSlot;
  if (slot != EquipSlot::Secondary && slot != EquipSlot::Range && slot != EquipSlot::Ammo) slot = EquipSlot::Primary;

  int dmg = 0;

  if (weapon) {
    if (char_info->Level < weapon->Common.RecLevel)
      dmg = calc_recommended_level_bonus(char_info->Level, weapon->Common.RecLevel, weapon->Common.Damage);
    else
      dmg = weapon->Common.Damage;

    if (slot == EquipSlot::Range && char_info->InventoryItem[EquipSlot::Ammo])
      dmg += get_base_damage(EquipSlot::Ammo, char_info->InventoryItem[EquipSlot::Ammo]);
  } else if (slot == EquipSlot::Primary || slot == EquipSlot::Secondary)
    dmg = get_hand_to_hand_damage(*char_info);

  return dmg;
}

// Returns true if the item is a weapon capable of generating damage.
static bool is_weapon(const Zeal::GameStructures::GAMEITEMINFO *weapon) {
  if (!weapon || weapon->Type != 0) {
    return false;
  }

  if (weapon->Common.Skill == Zeal::GameEnums::ItemTypes::ItemTypeArrow && weapon->Common.Damage != 0) {
    return true;
  } else {
    return ((weapon->Common.Damage != 0) && (weapon->Common.AttackDelay != 0));
  }
}

static bool is_melee_class(BYTE game_class) {
  switch (game_class) {
    case Zeal::GameEnums::ClassTypes::Warrior:
    case Zeal::GameEnums::ClassTypes::Paladin:
    case Zeal::GameEnums::ClassTypes::Ranger:
    case Zeal::GameEnums::ClassTypes::Shadowknight:
    case Zeal::GameEnums::ClassTypes::Monk:
    case Zeal::GameEnums::ClassTypes::Bard:
    case Zeal::GameEnums::ClassTypes::Rogue:
    case Zeal::GameEnums::ClassTypes::Beastlord:
      return true;
    default:
      return false;
  }
}

static int get_damage_bonus(const Zeal::GameStructures::GAMEITEMINFO *weapon) {
  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();
  if (!char_info) return 0;

  if (char_info->Level < 28 || !is_melee_class(char_info->Class)) return 0;

  int bonus = 1 + (char_info->Level - 28) / 3;

  const BYTE *item_type = weapon ? &(weapon->Common.Skill) : nullptr;
  if (item_type && (*item_type == Zeal::GameEnums::ItemType2HSlash || *item_type == Zeal::GameEnums::ItemType2HBlunt ||
                    *item_type == Zeal::GameEnums::ItemType2HPiercing)) {
    int delay = weapon->Common.AttackDelay;
    if (delay <= 27) return bonus + 1;

    int level = char_info->Level;
    if (level > 29) {
      int level_bonus = (level - 30) / 5 + 1;
      if (level > 50) {
        level_bonus++;
        int level_bonus2 = level - 50;
        if (level > 67)
          level_bonus2 += 5;
        else if (level > 59)
          level_bonus2 += 4;
        else if (level > 58)
          level_bonus2 += 3;
        else if (level > 56)
          level_bonus2 += 2;
        else if (level > 54)
          level_bonus2++;
        level_bonus += level_bonus2 * delay / 40;
      }
      bonus += level_bonus;
    }
    if (delay >= 40) {
      int delay_bonus = (delay - 40) / 3 + 1;
      if (delay >= 45)
        delay_bonus += 2;
      else if (delay >= 43)
        delay_bonus++;
      bonus += delay_bonus;
    }
    return bonus;
  }
  return bonus;
}

int get_anti_twink_damage(int base_damage) {
  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();
  if (!char_info) return base_damage;

  // anti-twink damage caps.  Taken from decompiles
  int level = char_info->Level;
  if (level < 10) {
    switch (char_info->Class) {
      case Zeal::GameEnums::ClassTypes::Druid:
      case Zeal::GameEnums::ClassTypes::Cleric:
      case Zeal::GameEnums::ClassTypes::Shaman:
        if (base_damage > 9) base_damage = 9;
        break;
      case Zeal::GameEnums::ClassTypes::Wizard:
      case Zeal::GameEnums::ClassTypes::Magician:
      case Zeal::GameEnums::ClassTypes::Necromancer:
      case Zeal::GameEnums::ClassTypes::Enchanter:
        if (base_damage > 6) base_damage = 6;
        break;
      default:
        if (base_damage > 10) base_damage = 10;
    }
  } else if (level < 20) {
    switch (char_info->Class) {
      case Zeal::GameEnums::ClassTypes::Druid:
      case Zeal::GameEnums::ClassTypes::Cleric:
      case Zeal::GameEnums::ClassTypes::Shaman:
        if (base_damage > 12) base_damage = 12;
        break;
      case Zeal::GameEnums::ClassTypes::Wizard:
      case Zeal::GameEnums::ClassTypes::Magician:
      case Zeal::GameEnums::ClassTypes::Necromancer:
      case Zeal::GameEnums::ClassTypes::Enchanter:
        if (base_damage > 10) base_damage = 10;
        break;
      default:
        if (base_damage > 14) base_damage = 14;
    }
  } else if (level < 30) {
    switch (char_info->Class) {
      case Zeal::GameEnums::ClassTypes::Druid:
      case Zeal::GameEnums::ClassTypes::Cleric:
      case Zeal::GameEnums::ClassTypes::Shaman:
        if (base_damage > 20) base_damage = 20;
        break;
      case Zeal::GameEnums::ClassTypes::Wizard:
      case Zeal::GameEnums::ClassTypes::Magician:
      case Zeal::GameEnums::ClassTypes::Necromancer:
      case Zeal::GameEnums::ClassTypes::Enchanter:
        if (base_damage > 12) base_damage = 12;
        break;
      default:
        if (base_damage > 30) base_damage = 30;
    }
  } else if (level < 40) {
    switch (char_info->Class) {
      case Zeal::GameEnums::ClassTypes::Druid:
      case Zeal::GameEnums::ClassTypes::Cleric:
      case Zeal::GameEnums::ClassTypes::Shaman:
        if (base_damage > 26) base_damage = 26;
        break;
      case Zeal::GameEnums::ClassTypes::Wizard:
      case Zeal::GameEnums::ClassTypes::Magician:
      case Zeal::GameEnums::ClassTypes::Necromancer:
      case Zeal::GameEnums::ClassTypes::Enchanter:
        if (base_damage > 18) base_damage = 18;
        break;
      default:
        if (base_damage > 60) base_damage = 60;
    }
  }
  return base_damage;
}

static int get_offense(const Zeal::GameEnums::SkillType skill, bool verbose = false, short color = USERCOLOR_DEFAULT) {
  auto char_info = Zeal::Game::get_char_info();
  if (!char_info) return 0;

  bool ranged = (skill == Zeal::GameEnums::SkillArchery || skill == Zeal::GameEnums::SkillThrowing);
  int stat_for_bonus = ranged ? char_info->dex() : char_info->str();
  int stat_bonus = max(0, (stat_for_bonus - 75) * 2 / 3);
  const int SE_ATK = 2;  // Spell Effect ID for attack boosts.
  int spell_atk = char_info->total_spell_affects(SE_ATK, false, nullptr);
  int item_atk = char_info->total_item_spell_affects(SE_ATK);
  int skill_value = char_info->i_have_skill(skill) ? char_info->skill(skill) : 0;

  // Client (and server) transfers the primal avatar +100 from spell atk to item atk.
  if (char_info->is_spell_affecting_pc(0x982)) {
    spell_atk -= 100;
    item_atk += 100;
    item_atk = min(250, item_atk);  //  RuleI(Character, ItemATKCap), client == 250.
  }

  int offense = skill_value + spell_atk + item_atk + stat_bonus;

  int class_bonus =
      (char_info->Class == Zeal::GameEnums::ClassTypes::Ranger) ? (max(0, (char_info->Level - 54) * 4)) : 0;
  offense += class_bonus;
  offense = max(1, offense);

  if (verbose)
    Zeal::Game::print_chat(color, "Offense: %d (Skill %d + Stat %d + SpellAtk %d + ItemAtk %d + Class %d)", offense,
                           skill_value, stat_bonus, spell_atk, item_atk, class_bonus);

  int client_offense = char_info->offense(skill);
  if (client_offense != offense)
    Zeal::Game::print_chat(color, "--- Issue: Client offense %d does not match Zeal offense %d ----", client_offense,
                           offense);

  return offense;
}

float get_damage_multiplier(int offense, Zeal::GameEnums::SkillType skill, bool max_value) {
  auto char_info = Zeal::Game::get_char_info();
  if (!char_info) return 1;

  int level = char_info->Level;
  int game_class = char_info->Class;
  int roll_chance = 51;
  int max_extra = 210;
  int minus_factor = 105;

  if (game_class == Zeal::GameEnums::ClassTypes::Monk && level >= 65) {
    roll_chance = 83;
    max_extra = 300;
    minus_factor = 50;
  } else if (level >= 65 || (game_class == Zeal::GameEnums::ClassTypes::Monk && level >= 63)) {
    roll_chance = 81;
    max_extra = 295;
    minus_factor = 55;
  } else if (level >= 63 || (game_class == Zeal::GameEnums::ClassTypes::Monk && level >= 60)) {
    roll_chance = 79;
    max_extra = 290;
    minus_factor = 60;
  } else if (level >= 60 || (game_class == Zeal::GameEnums::ClassTypes::Monk && level >= 56)) {
    roll_chance = 77;
    max_extra = 285;
    minus_factor = 65;
  } else if (level >= 56) {
    roll_chance = 72;
    max_extra = 265;
    minus_factor = 70;
  } else if (level >= 51 || game_class == Zeal::GameEnums::ClassTypes::Monk) {
    roll_chance = 65;
    max_extra = 245;
    minus_factor = 80;
  }

  int base_bonus = (static_cast<int>(offense) - minus_factor) / 2;
  if (base_bonus < 10) base_bonus = 10;

  // Either max value or the average value = the chance of a bonus * average bonus.
  int roll = (max_value) ? base_bonus : (roll_chance * base_bonus / 2 / 100);
  roll = min(max_extra, roll + 100);
  return static_cast<float>(roll) * (1.f / 100);
}

int get_hand_to_hand_delay() {
  const int default_delay = 35;
  auto char_info = Zeal::Game::get_char_info();
  if (!char_info)  // Unlikely so just bail to prevent corner-case crash.
    return default_delay;

  static BYTE mnk_hum_delay[] = {99, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35,  // 1-10
                                 35, 35, 35, 35, 35, 35, 35, 35, 35, 35,      // 11-20
                                 35, 35, 35, 35, 35, 35, 35, 34, 34, 34,      // 21-30
                                 34, 33, 33, 33, 33, 32, 32, 32, 32, 31,      // 31-40
                                 31, 31, 31, 30, 30, 30, 30, 29, 29, 29,      // 41-50
                                 29, 28, 28, 28, 28, 27, 27, 27, 27, 26,      // 51-60
                                 24, 22};                                     // 61-62
  static BYTE mnk_iks_delay[] = {99, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35,  // 1-10
                                 35, 35, 35, 35, 35, 35, 35, 35, 35, 35,      // 11-20
                                 35, 35, 35, 35, 35, 35, 35, 35, 35, 34,      // 21-30
                                 34, 34, 34, 34, 34, 33, 33, 33, 33, 33,      // 31-40
                                 33, 32, 32, 32, 32, 32, 32, 31, 31, 31,      // 41-50
                                 31, 31, 31, 30, 30, 30, 30, 30, 30, 29,      // 51-60
                                 25, 23};                                     // 61-62
  static BYTE bst_delay[] = {99, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35,      // 1-10
                             35, 35, 35, 35, 35, 35, 35, 35, 35, 35,          // 11-20
                             35, 35, 35, 35, 35, 35, 35, 35, 34, 34,          // 21-30
                             34, 34, 34, 33, 33, 33, 33, 33, 32, 32,          // 31-40
                             32, 32, 32, 31, 31, 31, 31, 31, 30, 30,          // 41-50
                             30, 30, 30, 29, 29, 29, 29, 29, 28, 28,          // 51-60
                             28, 28, 28};                                     // 61-63

  const BYTE iksar = 128;
  int level = char_info->Level;
  if (char_info->Class == Zeal::GameEnums::ClassTypes::Monk) {
    auto hands = char_info->InventoryItem[Zeal::GameEnums::EquipSlot::Hands];
    if (hands && hands->ID == 10652 && level > 50)  // Celestial Fists, monk epic
      return 16;

    if (level > 62) return char_info->Race == iksar ? 21 : 20;

    return char_info->Race == iksar ? mnk_iks_delay[level] : mnk_hum_delay[level];
  } else if (char_info->Class == Zeal::GameEnums::ClassTypes::Beastlord) {
    if (level > 63) return 27;
    return bst_delay[level];
  }
  return default_delay;
}

void print_melee_attack_stats(bool primary, const Zeal::GameStructures::GAMEITEMINFO *weapon, short color) {
  auto char_info = Zeal::Game::get_char_info();
  if (!char_info)  // Unlikely so just bail to prevent corner-case crash.
    return;

  if (!primary && (char_info->Skills[Zeal::GameEnums::SkillDualWield] == 255))
    return;  // Secondary can't attack w/out dual wield.

  const Zeal::GameEnums::EquipSlot::EquipSlot slot =
      primary ? Zeal::GameEnums::EquipSlot::Primary : Zeal::GameEnums::EquipSlot::Secondary;

  if (!weapon) weapon = char_info->InventoryItem[slot];

  Zeal::Game::print_chat(color, "---- Melee %s: %s ----", primary ? "Primary" : "Secondary",
                         weapon ? weapon->Name : "HandToHand");

  if (weapon && !Zeal::Game::can_item_equip_in_slot(char_info, weapon, slot + 1)) {
    Zeal::Game::print_chat(color, "Can not use weapon in this slot.");
    return;
  }

  // Now figure out damage
  int base_damage_raw = get_base_damage(slot, weapon);
  int base_damage = get_anti_twink_damage(base_damage_raw);
  if (base_damage != base_damage_raw)
    Zeal::Game::print_chat(color, "Anti-twink: base_damage reduced from %d to %d", base_damage_raw, base_damage);

  int bonus_damage = primary ? get_damage_bonus(weapon) : 0;

  Zeal::GameEnums::SkillType skill = get_weapon_skill(weapon);
  int to_hit = char_info->compute_to_hit(skill);
  int offense = get_offense(skill, true, color);  // Print out offense.
  Zeal::Game::print_chat(color, "To Hit: %d", to_hit);
  float dmg_multiplier_max = get_damage_multiplier(offense, skill, true);
  float dmg_multiplier_ave = get_damage_multiplier(offense, skill, false);
  float min_damage = bonus_damage + base_damage * 0.1f * 1;
  float max_damage = bonus_damage + base_damage * 2.0f * dmg_multiplier_max +
                     is_melee_class(char_info->Class);  // dmg_multiplier has a +1 bonus for melee classes.
  float ave_damage = bonus_damage + base_damage * 1.0f * dmg_multiplier_ave;
  int delay = weapon ? weapon->Common.AttackDelay : get_hand_to_hand_delay();
  if (primary) {  // Reduce log spam.
    Zeal::Game::print_chat(color, "Display ATK: %d = (offense + to hit) * 1000 / 744", (offense + to_hit) * 1000 / 744);
    Zeal::Game::print_chat(color, "Dmg = BonusDmg + BaseDmg * MitFactor * DmgMultiplier");
  }
  Zeal::Game::print_chat(color, "Dmg = %d + %d * (0.1 to 2.0x) * (1 to %.2f, ave = %.2f)", bonus_damage, base_damage,
                         dmg_multiplier_max, dmg_multiplier_ave);
  Zeal::Game::print_chat(color, "Dmg = %.2f to %.2f, ave = %.2f", min_damage, max_damage, ave_damage);
  if (delay > 0) {
    Zeal::Game::print_chat(color, "DPS = %.2f to %.2f, ave = %.2f", 10 * min_damage / delay, 10 * max_damage / delay,
                           10 * ave_damage / delay);
    const int SE_AttackSpeed = 11;  // Spell Effect ID for haste (> 100).
    int haste = char_info->total_spell_affects(SE_AttackSpeed, true, nullptr);
    auto self = Zeal::Game::get_self();
    if (self && haste > 100) {
      int haste_delay = self->ModifyAttackSpeed(delay, false);
      if (haste_delay > 0)
        Zeal::Game::print_chat(color, "DPS = %.2f to %.2f, ave = %.2f (%d%% haste)", 10 * min_damage / haste_delay,
                               10 * max_damage / haste_delay, 10 * ave_damage / haste_delay, haste - 100);
    }
  }
}
}  // namespace Game
}  // namespace Zeal

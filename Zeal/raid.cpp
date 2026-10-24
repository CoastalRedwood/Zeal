#include "raid.h"

#include "callbacks.h"
#include "game_structures.h"

#include <algorithm>
#include <cstring>
#include "commands.h"
#include "game_functions.h"
#include "hook_wrapper.h"
#include "memory.h"
#include "string_util.h"
#include "zeal.h"

void Raid::callback_main() {
  if (!autorejoin_.enabled() || !Zeal::Game::is_in_game()) return;
  const uint64_t now = GetTickCount64();
  if (last_ari_check_ && now - last_ari_check_ < 1000) return;
  last_ari_check_ = now;

  const auto* info = Zeal::Game::RaidInfo;
  if (!info) return;
  std::vector<RaidAutoRejoin::Member> roster;
  if (info->is_in_raid()) {
    roster.reserve(Zeal::GameStructures::RaidInfo::kRaidMaxMembers);
    for (int i = 0; i < Zeal::GameStructures::RaidInfo::kRaidMaxMembers; ++i) {
      const auto& member = info->MemberList[i];
      if (!member.Name[0]) continue;
      const auto* end = std::find(member.Name, member.Name + sizeof(member.Name), '\0');
      if (end == member.Name + sizeof(member.Name)) continue;
      const int group = member.GroupNumber == Zeal::GameStructures::RaidMember::kRaidUngrouped
                            ? 0 : static_cast<int>(member.GroupNumber) + 1;
      roster.push_back({std::string(member.Name, end), group});
    }
  }

  const auto moves = autorejoin_.observe(info->Id, roster, info->IsLeader != 0, now);
  for (const auto& move : moves) {
    Zeal::Game::print_chat("ARI: restoring %s to raid group %d", move.name.c_str(), move.group);
    // Existing Zeal raid-management operation. Does not send keyboard events.
    Zeal::Game::do_say(true, "#raidmove %s %d", move.name.c_str(), move.group);
  }
}

bool Raid::handle_ari(std::vector<std::string>& args) {
  if (args.size() == 3 && args[1] == "rejoin" &&
      (args[2] == "on" || args[2] == "off")) {
    autorejoin_.enable(args[2] == "on");
    last_ari_check_ = 0;
    Zeal::Game::print_chat("ARI: raid auto-rejoin %s", autorejoin_.enabled() ? "ON" : "OFF");
    if (autorejoin_.enabled())
      Zeal::Game::print_chat("ARI: learning current raid groups; only the raid leader can auto-restore members.");
  } else {
    Zeal::Game::print_chat("Usage: /ari rejoin on | /ari rejoin off");
    Zeal::Game::print_chat("ARI: raid auto-rejoin is %s", autorejoin_.enabled() ? "ON" : "OFF");
  }
  return true;
}

Raid::~Raid() {}

void __fastcall SetLootTypeResponse(void *t, int unused, int p1) {
  ZealService *zeal = ZealService::get_instance();
  int new_loot_type = *(int *)(p1 + 0x84);
  if (new_loot_type == 4) {
    Zeal::Game::print_chat("The loot type is now - free for all");
    return;
  } else {
    zeal->hooks->hook_map["SetLootTypeResponse"]->original(SetLootTypeResponse)(t, unused, p1);
  }
}

static bool handle_raidmove(std::vector<std::string> &args) {
  auto target = Zeal::Game::get_target();
  if (!target) {
    Zeal::Game::print_chat("/raidmove requires a target.");
    return true;
  }  

  int group_number = 0;
  if (args.size() == 2 && Zeal::String::tryParse(args[1], &group_number, true)) {
      Zeal::Game::do_say(true, "#raidmove %s %d", target->Name, group_number);
  } else {
    Zeal::Game::print_chat("Usage: /raidmove [groupnumber]");
  }

  return true;
}

static bool handle_raidpromote() {
  auto target = Zeal::Game::get_target();
  if (!target) {
    Zeal::Game::print_chat("/raidpromote requires a target.");
    return true;
  }
  Zeal::Game::do_say(true, "#raidpromote %s", target->Name);
  return true;
}

Raid::Raid(ZealService *zeal) {
  mem::write<BYTE>(0x49E182, 4);  // allow for 4 types in setloottype
  mem::write<BYTE>(0x42FAB3, 4);  // allow for 4 types being set from the options window
  zeal->hooks->Add("SetLootTypeResponse", 0x49dbc1, SetLootTypeResponse,
                   hook_type_detour);  // add extra prints for new loot types
  zeal->callbacks->AddGeneric([this]() { callback_main(); });
  zeal->commands_hook->Add("/ari", {}, "Raid auto-rejoin: /ari rejoin on | off",
                           [this](std::vector<std::string>& args) { return handle_ari(args); });
  zeal->commands_hook->Add("/raidmove", {"/rm"}, "Moves your current target in the raid. Usage: /raidmove [groupnumber]",
                           [](std::vector<std::string> &args) { return handle_raidmove(args); });
  zeal->commands_hook->Add("/raidpromote", {"/rp"}, "Promotes your current target to raid leader. Usage: /raidpromote",
                           [](std::vector<std::string> &args) { return handle_raidpromote(); });
}

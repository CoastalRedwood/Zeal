#include "raid.h"

#include "callbacks.h"
#include "commands.h"
#include "game_functions.h"
#include "hook_wrapper.h"
#include "memory.h"
#include "string_util.h"
#include "zeal.h"

void Raid::callback_main() {}

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

  if (args.size() == 2) {
    int group_number = 0;
    if (Zeal::String::tryParse(args[1], &group_number)) {
      Zeal::Game::do_say(true, "#raidmove %s %d", target->Name, group_number);
    } else {
      Zeal::Game::print_chat("Usage: /raidmove [groupnumber]");
    }
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
  zeal->commands_hook->Add("/raidmove", {"/rm"}, "Moves your current target in the raid. Usage: /raidmove [groupnumber]",
                           [](std::vector<std::string> &args) { return handle_raidmove(args); });
  zeal->commands_hook->Add("/raidpromote", {"/rp"}, "Promotes your current target to raid leader. Usage: /raidpromote",
                           [](std::vector<std::string> &args) { return handle_raidpromote(); });
}

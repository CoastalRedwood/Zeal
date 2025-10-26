#include "ui_group.h"

#include <algorithm>
#include <cctype>

#include "callbacks.h"
#include "commands.h"
#include "game_addresses.h"
#include "game_functions.h"
#include "game_packets.h"
#include "game_structures.h"
#include "game_ui.h"
#include "hook_wrapper.h"
#include "memory.h"
#include "string_util.h"
#include "ui_manager.h"
#include "zeal.h"

void ui_group::InitUI() {}

void ui_group::swap(int index1, int index2) {
  auto *group_info = Zeal::Game::GroupInfo;
  if (index1 < 5 && index2 < 5) {
    std::pair<std::string, Zeal::GameStructures::Entity *> Ent1 =
        std::make_pair(group_info->Names[index1], group_info->EntityList[index1]);
    std::pair<std::string, Zeal::GameStructures::Entity *> Ent2 =
        std::make_pair(group_info->Names[index2], group_info->EntityList[index2]);

    // move the strings
    mem::copy((DWORD)(group_info->Names[index1]), (DWORD)Ent2.first.c_str(),
              Ent2.first.length() + 1);  //+1 don't forget string terminator
    mem::copy((DWORD)(group_info->Names[index2]), (DWORD)Ent1.first.c_str(), Ent1.first.length() + 1);
    group_info->EntityList[index1] = Ent2.second;
    group_info->EntityList[index2] = Ent1.second;
  } else {
    Zeal::Game::print_chat("Error moving group members: %i to %i", index1, index2);
  }
  handle_group_colors(true);
}

void ui_group::sort() {
  auto *group_info = Zeal::Game::GroupInfo;
  std::vector<std::pair<std::string, Zeal::GameStructures::Entity *>> group_members_sorted;
  for (int i = 0; i < 5; i++) {
    std::pair<std::string, Zeal::GameStructures::Entity *> ent_pair =
        std::make_pair(group_info->Names[i], group_info->EntityList[i]);
    group_members_sorted.push_back({group_info->Names[i], group_info->EntityList[i]});
  }
  std::sort(group_members_sorted.begin(), group_members_sorted.end(),
            [](const std::pair<std::string, Zeal::GameStructures::Entity *> &a,
               const std::pair<std::string, Zeal::GameStructures::Entity *> &b) {
              // If 'a' is empty and 'b' is not, 'b' should come first (return true)
              if (a.first.empty() && !b.first.empty()) {
                return false;
              }
              // If 'b' is empty and 'a' is not, 'a' should come first (return true)
              if (!a.first.empty() && b.first.empty()) {
                return true;
              }

              std::string lower_a = a.first;
              std::string lower_b = b.first;

              std::transform(lower_a.begin(), lower_a.end(), lower_a.begin(), ::tolower);
              std::transform(lower_b.begin(), lower_b.end(), lower_b.begin(), ::tolower);

              return lower_a < lower_b;
            });
  mem::set((uint32_t)group_info->EntityList, 0, sizeof(group_info->EntityList));  // erase the pointer array
  mem::set((uint32_t)group_info->Names, 0, sizeof(group_info->Names));            // erase the pointer array
  for (int i = 0; i < group_members_sorted.size(); ++i) {
    mem::copy((DWORD)(group_info->Names[i]), (DWORD)group_members_sorted[i].first.c_str(),
              group_members_sorted[i].first.length());
    group_info->EntityList[i] = group_members_sorted[i].second;
  }
  handle_group_colors(true);
}

void ui_group::handle_group_colors(bool log_error) {
  bool success = Zeal::Game::update_group_window_colors(setting_add_group_colors.get());

  if (!success && log_error && setting_add_group_colors.get())
    Zeal::Game::print_chat("Error: Failed to update group window colors");
}

// Update the group window if a raid color changes.
static void __fastcall CRaidWnd_SetClassColor(void *raid_wnd, int unused_edx, int index, DWORD argb) {
  auto zeal = ZealService::get_instance();
  zeal->hooks->hook_map["CRaidWnd_SetClassColor"]->original(CRaidWnd_SetClassColor)(raid_wnd, unused_edx, index, argb);
  if (zeal->ui && zeal->ui->group && zeal->ui->group->setting_add_group_colors.get())
    Zeal::Game::update_group_window_colors(true);
}

ui_group::~ui_group() {}

ui_group::ui_group(ZealService *zeal, UIManager *mgr) {
  ui = mgr;
  // zeal->callbacks->AddGeneric([this]() { InitUI(); }, callback_type::InitUI);
  zeal->commands_hook->Add(
      "/sortgroup", {"/sg"},
      "Sort your group members example usages: /sg or /sg 1 2 where /sg alpha sorts the group and /sg 1 2 switches "
      "players 1 and 2 in your group window ",
      [this](std::vector<std::string> &args) {
        int index1 = 0;
        int index2 = 0;
        if (args.size() > 2 && Zeal::String::tryParse(args[1], &index1) && Zeal::String::tryParse(args[2], &index2) &&
            index1 > 0 && index1 <= 6 && index2 > 0 && index2 <= 6)
          swap(
              index1 - 1,
              index2 - 1);  // makes this easier for end users so /sortgroup 1 2 will swap players 1 and 2 in your group
        else
          sort();
        return true;
      });

  // Add a callback to cache the default group window colors before any modifications are made.
  zeal->callbacks->AddGeneric(
      [this]() { Zeal::Game::update_group_window_colors(setting_add_group_colors.get(), true); },
      callback_type::InitUI);

  zeal->callbacks->AddPacket(
      [this](UINT opcode, char *buffer, UINT len) {
        if (opcode == Zeal::Packets::GroupUpdate || opcode == Zeal::Packets::PlayerProfile) handle_group_colors();

        // This call is used to handle group members zoning in. Since it is called frequently, only enable
        // it when the optional add colors is enabled. Might be able to future optimize this by just
        // checking the top of the entity list (which was just added) to see if it is in the group list
        // before performing the full group color update.
        if (opcode == Zeal::Packets::ZoneSpawns && setting_add_group_colors.get()) handle_group_colors();

        return false;  // continue processing
      },
      callback_type::WorldMessagePost);

  // Keep the group window text colors up to date when raid colors are changed.
  zeal->hooks->Add("CRaidWnd_SetClassColor", 0x00431af5, CRaidWnd_SetClassColor, hook_type_detour);
}

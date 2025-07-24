#include "ui_guild.h"

#include <algorithm>

#include "game_addresses.h"
#include "game_functions.h"
#include "game_structures.h"
#include "zeal.h"

void ui_guild::CleanUI() {
  Zeal::Game::print_debug("Clean UI GUILD");
  if (guild) {
    guild->IsVisible = false;
    delete guild;
  }
  members = nullptr;
}

void ui_guild::InitUI() {
  guild = new Zeal::GameUI::BasicWnd();
  guild->vtbl = (Zeal::GameUI::BaseVTable *)0x5e60f0;
  Zeal::GameUI::CXSTR name_cxstr("GuildManagementWnd");  // Constructor calls FreeRep() internally.
  reinterpret_cast<int *(__thiscall *)(Zeal::GameUI::BasicWnd *, Zeal::GameUI::BasicWnd *, Zeal::GameUI::CXSTR name,
                                       int, int)>(0x56e1e0)(guild, 0, name_cxstr, -1, 0);
  guild->CreateChildren();
  members = (Zeal::GameUI::ListWnd *)guild->GetChildItem("MemberList");
}

ui_guild::ui_guild(ZealService *zeal, IO_ini *ini, UIManager *mgr) {
  ui = mgr;
  guild = nullptr;
  zeal->callbacks->AddGeneric([this]() { CleanUI(); }, callback_type::CleanUI);
  zeal->callbacks->AddGeneric([this]() { InitUI(); }, callback_type::InitUI);
  zeal->commands_hook->Add("/read", {}, "", [this](std::vector<std::string> &args) {
    if (members) {
      for (int i = 1; i < members->ItemCount; i++) {
        auto name = members->GetItemText(i, 0);
        auto level = members->GetItemText(i, 1);
        auto _class = members->GetItemText(i, 2);
        Zeal::Game::print_chat("%s %s %s", name.c_str(), level.c_str(), _class.c_str());
      }
    }
    return true;
  });
  zeal->commands_hook->Add(
      "/guildwindow", {}, "Toggle guild management window", [this, mgr](std::vector<std::string> &args) {
        if (guild) {
          Zeal::Game::print_chat("Attempting to show guild window");
          guild->IsVisible = true;
          // some mock data -->
          std::vector<std::vector<std::string>> players;
          Zeal::GameStructures::Entity *ent = Zeal::Game::get_entity_list();
          while (ent->Next) {
            if (ent->Type == 0) {
              players.push_back({ent->Name, std::to_string(ent->Level), Zeal::Game::class_name_short(ent->Class),
                                 "Peasant", "5/7/2024", "Commons", ""});
            }
            ent = ent->Next;
          }
          mgr->AddListItems(members, players);
        } else {
          Zeal::Game::print_chat("Guild window not found!");
        }
        return true;
      });
  // if (Zeal::Game::is_in_game()) InitUI();
}

ui_guild::~ui_guild() {}

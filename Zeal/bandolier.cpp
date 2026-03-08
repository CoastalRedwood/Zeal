#include <algorithm>
#include <filesystem>
#include <format>

#include "callbacks.h"
#include "commands.h"
#include "game_addresses.h"
#include "game_functions.h"
#include "hook_wrapper.h"
#include "bandolier.h"
#include "string_util.h"
#include "ui_manager.h"
#include "zeal.h"

// Helper function to verify the bandolier name is valid.
static bool is_valid_name(const std::string &name) {
  if (name.empty() || name.size() > 32) {
    Zeal::Game::print_chat("Invalid bandolier name");
    return false;
  }
  return true;
}

// Saves currently equiped primary, secondary, range and ammo slots.
void Bandolier::save(const std::string &name) {
  if (!is_valid_name(name)) return;

  initialize_ini_filename();
  Zeal::Game::print_chat("Saving bandolier [%s]", name.c_str());

  // Get item id from equipped slots: primary, secondary, range and ammo.
  // Store them in the file as:
  // [Set Name]
  // primary=idprimary
  // secondary=idsecondary
  // range=idrange
  // ammo=idammo
  auto *char_info = Zeal::Game::get_char_info();
  if (!char_info) {
    Zeal::Game::print_chat("Error: could not retrieve character info");
    return;
  }

  auto get_item_id = [&](int slot) -> int {
    auto *item = char_info->InventoryItem[slot];
    return item ? item->ID : 0;
  };

  ini.setValue(name, "primary",   std::to_string(get_item_id(0)));
  ini.setValue(name, "secondary", std::to_string(get_item_id(1)));
  ini.setValue(name, "range",     std::to_string(get_item_id(2)));
  ini.setValue(name, "ammo",      std::to_string(get_item_id(3)));
}

// Removes (deletes) the bandolier set from the ini file.
void Bandolier::remove(const std::string &name) {
  initialize_ini_filename();
  Zeal::Game::print_chat("Removing bandolier [%s]", name.c_str());
  if (!ini.deleteSection(name)) Zeal::Game::print_chat("Error removing bandolier [%s]", name.c_str());
}

// Loads a bandolier set from the ini file and initiates memorization.
void Bandolier::load(const std::string &name) {

  if (!is_valid_name(name)) return;

  initialize_ini_filename();
  if (!ini.exists(name, "0")) {
    Zeal::Game::print_chat("The bandolier set [%s] does not exist", name.c_str());
    return;
  }
  Zeal::Game::print_chat("Loading bandolier set [%s]", name.c_str());
}

// Initializes the character dependent filename useed to store bandolier sets.
void Bandolier::initialize_ini_filename() {
  const char *name = Zeal::Game::get_char_info() ? Zeal::Game::get_char_info()->Name : "unknown";
  std::string filename = std::string(name) + "_bandolier.ini";
  std::filesystem::path file_path = Zeal::Game::get_game_path() / std::filesystem::path(filename);
  ini.set(file_path.string());
}

Bandolier::Bandolier(ZealService *zeal) {

  zeal->commands_hook->Add("/bandolier", {"/bd"}, "Load, save, delete or list your bandolier sets.",
                           [this, zeal](std::vector<std::string> &args) {
                             if (args.size() == 2 && Zeal::String::compare_insensitive(args[1], "list")) {
                               initialize_ini_filename();
                               std::vector<std::string> sets = ini.getSectionNames();
                               Zeal::Game::print_chat("--- bandolier sets (%i) ---", sets.size());
                               for (auto &set : sets) {
                                 Zeal::Game::print_chat(set);
                               }
                               Zeal::Game::print_chat("--- end of bandolier sets ---", sets.size());
                               return true;
                             }
                             if (args.size() == 3 && Zeal::Game::get_self() && Zeal::Game::get_char_info()) {
                               if (Zeal::String::compare_insensitive(args[1], "test")) {
                                 return true;
                               }
                               if (Zeal::String::compare_insensitive(args[1], "save")) {
                                 save(args[2]);
                                 return true;
                               }
                               if (Zeal::String::compare_insensitive(args[1], "delete") ||
                                   Zeal::String::compare_insensitive(args[1], "remove")) {
                                 remove(args[2]);
                                 return true;
                               }
                               if (Zeal::String::compare_insensitive(args[1], "load")) {
                                 load(args[2]);
                                 return true;
                               }
                             }
                             Zeal::Game::print_chat("usage: /bandolier save/load/delete [name], /bandolier list");
                             return true;
                           });
}

Bandolier::~Bandolier() {}

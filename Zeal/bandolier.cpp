#include "bandolier.h"

#include <algorithm>
#include <filesystem>
#include <format>

#include "callbacks.h"
#include "commands.h"
#include "game_addresses.h"
#include "game_functions.h"
#include "hook_wrapper.h"
#include "string_util.h"
#include "ui_manager.h"
#include "zeal.h"

// Verify the bandolier name is valid.
static bool is_valid_name(const std::string &name) {
  if (name.empty() || name.size() > 32) {
    Zeal::Game::print_chat("Invalid bandolier name");
    return false;
  }
  return true;
}

// Saves currently equipped primary, secondary, range and ammo slots.
// Store them in the file as:
// [Set Name]
// 0=idprimary
// 1=idsecondary
// 2=idrange
// 3=idammo
void Bandolier::save(const std::string &name) {
  if (!is_valid_name(name)) return;

  initialize_ini_filename();
  Zeal::Game::print_chat("Saving bandolier set [%s]", name.c_str());

  auto *char_info = Zeal::Game::get_char_info();
  if (!char_info) return;

  for (size_t i = 0; i < BANDOLIER_SLOTS.size(); ++i) {
    auto *item = Zeal::Game::get_inventory_item_from_global_slot_id(BANDOLIER_SLOTS[i]);
    ini.setValue(name, std::to_string(i), std::to_string(item ? item->ID : 0));
  }
}

// Removes (deletes) the bandolier set from the ini file.
void Bandolier::remove(const std::string &name) {
  initialize_ini_filename();
  Zeal::Game::print_chat("Removing bandolier set [%s]", name.c_str());
  if (!ini.deleteSection(name)) Zeal::Game::print_chat("Error removing bandolier set [%s]", name.c_str());
}

// Loads a bandolier set from the ini file and initiates memorization.
void Bandolier::load(const std::string &name) {
  if (!steps.empty()) {
    Zeal::Game::print_chat("Already swapping bandolier sets, please wait");
    return;
  }

  if (!is_valid_name(name)) return;

  initialize_ini_filename();
  if (!ini.exists(name, "0")) {
    Zeal::Game::print_chat("The bandolier set [%s] does not exist", name.c_str());
    return;
  }
  Zeal::Game::print_chat("Loading bandolier set [%s]", name.c_str());

  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();
  if (!char_info) return;

  // Validate character is ready to swap items
  if (!check_player_can_swap(char_info)) {
    return;
  }

  // We want to allow swapping within the bando set (like primary to secondary), so we track
  // the state of items stored to inventory as we make swaps so subsequent steps are correct.
  struct MovedItem {
    int slot_id;
    Zeal::GameStructures::GAMEITEMINFO *item;
  };

  std::vector<MovedItem> moved_items;

  // We may want to source the items from some bando slots, so we track that as well and
  // also retrieve all of the target load item_ids.
  std::array<Zeal::GameStructures::GAMEITEMINFO *, BANDOLIER_SLOTS.size()> equipped{};
  std::array<int, BANDOLIER_SLOTS.size()> item_ids{};
  for (size_t i = 0; i < BANDOLIER_SLOTS.size(); ++i) {
    equipped[i] = Zeal::Game::get_inventory_item_from_global_slot_id(BANDOLIER_SLOTS[i]);
    item_ids[i] = ini.getValue<int>(name, std::to_string(i));
  }

  // Support an initial unload of primary/secondary to support 2h and secondary instruments.
  // Just look and see if there are any empty slots to determine if the current or loading
  // set is a 2h-like gearset.
  bool empty_slot = (!item_ids[0] || !item_ids[1] || !equipped[0] || !equipped[1]);
  bool unload = (empty_slot &&
                 ((equipped[0] && equipped[0]->ID != item_ids[0]) || (equipped[1] && equipped[1]->ID != item_ids[1])));
  for (auto i = 0; i < BANDOLIER_SLOTS.size(); ++i) {
    if (unload && equipped[i] && (BANDOLIER_SLOTS[i] == kPrimarySlot || BANDOLIER_SLOTS[i] == kSecondarySlot)) {
      std::vector<int> reserved_slots;
      for (const auto &moved_item : moved_items) reserved_slots.push_back(moved_item.slot_id);
      int store_slot = find_empty_inventory_slot(char_info, equipped[i], reserved_slots);
      if (store_slot == -1) {
        Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "No empty inventory slot to unequip [%s], canceling set load.",
                               equipped[i]->Name);
        steps.clear();
        return;
      }
      steps.push_back({.from_slot_id = BANDOLIER_SLOTS[i], .to_slot_id = store_slot});
      moved_items.push_back({store_slot, equipped[i]});  // Track where we stashed the item.
      equipped[i] = nullptr;  // Update our state so the code below doesn't bother unloading later.
    }
  }

  // We go in reverse order so the final appearance update is typically the primary weapon
  for (auto i = BANDOLIER_SLOTS.size(); i-- > 0; /*comparison post-dec*/) {
    int item_id = item_ids[i];

    // Do nothing for this slot if no items, item already loaded, or a no-load for ammo or range.
    if ((item_id == 0) && (BANDOLIER_SLOTS[i] == kRangeSlot || BANDOLIER_SLOTS[i] == kAmmoSlot) ||
        (item_id == 0 && !equipped[i]) || (equipped[i] && equipped[i]->ID == item_id)) {
      continue;
    }

    // Identify the slot that currently contains the item (if any) we want to load.
    int load_slot = -1;
    if (item_id > 0) {
      // First check if the item was already moved to another non-bandolier slot.
      for (auto it = moved_items.begin(); it < moved_items.end() && load_slot < 0; ++it) {
        if (it->item && it->item->ID == item_id) {
          load_slot = it->slot_id;
          moved_items.erase(it);  // Slot is now empty unless it is loaded below.
        }
      }

      // Then check if the item is in some other equipped slot we haven't yet processed.
      for (auto j = i; i != 0 && j-- > 0 && load_slot < 0; /*comparison post-dec*/) {
        if (equipped[j] && equipped[j]->ID == item_id) {
          load_slot = BANDOLIER_SLOTS[j];
          equipped[j] = nullptr;  // It will be empty as we don't directly swap back into equipped.
        }
      }

      // And if still not found then try to find the source item in the non-equipped inventory
      if (load_slot < 0) {
        load_slot = Zeal::Game::find_item_in_inventory(item_id, false);

        // Store it's original position (if in inventory) in case we want to swap it back later
        if (load_slot > 0) original_position[item_id] = load_slot;
      }
      if (load_slot == -1) {
        Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE,
                               "Item with ID [%d] not found in inventory for bandolier set [%s], canceling set load.",
                               item_id, name.c_str());
        steps.clear();
        return;
      }
    }

    int store_slot = -1;
    if (equipped[i]) {
      // We only allow direct single-step swap exchanges to and from bag storage (not between equipped slot).
      bool can_swap =
          (load_slot > GAME_EQUIPMENT_SLOTS_END && Zeal::Game::can_go_in_inventory_slot_id(equipped[i], load_slot));
      std::vector<int> reserved_slots;
      for (const auto &moved_item : moved_items) reserved_slots.push_back(moved_item.slot_id);
      store_slot = can_swap ? load_slot : find_empty_inventory_slot(char_info, equipped[i], reserved_slots);
      if (store_slot == -1) {
        Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "No empty inventory slot to unequip [%s], canceling set load.",
                               equipped[i]->Name);
        steps.clear();
        return;
      }

      // Update our state of where items will be for future steps.
      if (equipped[i]) moved_items.push_back({store_slot, equipped[i]});
      equipped[i] = nullptr;  // We won't access this again, so just null it instead of tracking the loaded item.
    }

    if (store_slot != load_slot && store_slot != -1)
      steps.push_back({.from_slot_id = BANDOLIER_SLOTS[i], .to_slot_id = store_slot});

    if (load_slot != -1) steps.push_back({.from_slot_id = load_slot, .to_slot_id = BANDOLIER_SLOTS[i]});
  }

  if (steps.empty()) {
    Zeal::Game::print_chat("Bandolier set [%s] is already equipped", name.c_str());
    return;
  }
}

void Bandolier::tick() {
  if (steps.empty()) return;

  auto step = steps.begin();

  const char *error = Zeal::Game::swap_inventory_slot_items_through_cursor(step->from_slot_id, step->to_slot_id, true);
  if (error) {
    Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, error);
    steps.clear();
    return;
  }

  steps.erase(step);

  if (steps.empty()) {
    Zeal::Game::print_chat("Bandolier set swap complete");
  }
}

int Bandolier::find_empty_inventory_slot(Zeal::GameStructures::GAMECHARINFO *char_info,
                                         Zeal::GameStructures::GAMEITEMINFO *item, std::vector<int> &reserved_slots) {
  // Check first if we have items original position stored from previous bandolier set swaps
  if (original_position.contains(item->ID)) {
    int slot = original_position[item->ID];
    bool is_reserved = std::find(reserved_slots.begin(), reserved_slots.end(), slot) != reserved_slots.end();
    if (!is_reserved && Zeal::Game::can_go_in_inventory_slot_id(item, slot, true)) return slot;
  }

  // Then check for space inside our "preferred bag" if set to a valid value
  if (setting_bag_slot.get() > 0 && setting_bag_slot.get() <= GAME_NUM_INVENTORY_PACK_SLOTS) {
    int start_index = GAME_CONTAINER_SLOTS_START + GAME_NUM_CONTAINER_SLOTS * (setting_bag_slot.get() - 1);
    for (int slot = start_index; slot < start_index + GAME_NUM_CONTAINER_SLOTS; ++slot) {
      bool is_reserved = std::find(reserved_slots.begin(), reserved_slots.end(), slot) != reserved_slots.end();
      if (!is_reserved && Zeal::Game::can_go_in_inventory_slot_id(item, slot, true)) return slot;
    }
  }

  // Next look for a spot inside any bag, starting from last to first to keep gear separate from auto-inventory loot
  for (int slot = GAME_CONTAINER_SLOTS_END; slot >= GAME_CONTAINER_SLOTS_START; --slot) {
    bool is_reserved = std::find(reserved_slots.begin(), reserved_slots.end(), slot) != reserved_slots.end();
    if (!is_reserved && Zeal::Game::can_go_in_inventory_slot_id(item, slot, true)) return slot;
  }

  // If no empty bag can hold the unequipped item, look for an empty primary pack slot
  for (int slot = GAME_PACKS_SLOTS_END; slot >= GAME_PACKS_SLOTS_START; --slot) {
    bool is_reserved = std::find(reserved_slots.begin(), reserved_slots.end(), slot) != reserved_slots.end();
    if (!is_reserved && Zeal::Game::can_go_in_inventory_slot_id(item, slot, true)) return slot;
  }

  return -1;
}

// These checks are mostly redundant with the ones in swap_inventory_slot_items_through_cursor() but
// we can customize the messaging here for bandolier.
bool Bandolier::check_player_can_swap(Zeal::GameStructures::GAMECHARINFO *char_info) {
  auto *self = Zeal::Game::get_self();

  if (!Zeal::Game::Windows->Quantity || Zeal::Game::Windows->Quantity->Activated) {
    Zeal::Game::print_chat("You are too busy to swap items!");
    return false;
  }

  if (!self || !self->ActorInfo || !char_info || char_info->StunnedState || !Zeal::Game::get_game() ||
      !Zeal::Game::get_game()->IsOkToTransact() || !Zeal::Game::get_display()) {
    Zeal::Game::print_chat("You are too busy to equip bandolier set!");
    return false;
  }

  // Block swapping when casting unless it's a bard singing a song.
  if ((self->ActorInfo->CastingSpellId != kInvalidSpellId) &&
      !Zeal::Game::GameInternal::IsPlayerABardAndSingingASong()) {
    Zeal::Game::print_chat("You cannot swap items when casting!");
    return false;
  }

  // We swap through the cursor to ensure proper server synchronization, so it must be empty.
  if (char_info->CursorItem || char_info->CursorCopper || char_info->CursorGold || char_info->CursorPlatinum ||
      char_info->CursorSilver) {
    Zeal::Game::print_chat("You cannot swap items when holding something in the cursor!");
    return false;
  }

  // The InvSlot::HandleLButtonUp() also blocked moves in some cases when CursorAttachment was active.
  if (!Zeal::Game::Windows->CursorAttachment || Zeal::Game::Windows->CursorAttachment->Activated) {
    Zeal::Game::print_chat("You cannot swap items when the cursor is busy!");
    return false;
  }

  return true;
}

// Initializes the character dependent filename useed to store bandolier sets.
void Bandolier::initialize_ini_filename() {
  const char *name = Zeal::Game::get_char_info() ? Zeal::Game::get_char_info()->Name : "unknown";
  std::string filename = std::string(name) + "_bandolier.ini";
  std::filesystem::path file_path = Zeal::Game::get_game_path() / std::filesystem::path(filename);
  ini.set(file_path.string());
}

Bandolier::Bandolier(ZealService *zeal) {
  zeal->callbacks->AddGeneric([this]() { tick(); });
  zeal->callbacks->AddGeneric(
      [this]() {
        steps.clear();
        original_position.clear();
      },
      callback_type::InitUI);

  std::vector<SetStep> steps;            // Sequence of steps to perform to swap to a new set (non-empty when active)
  std::map<int, int> original_position;  // List of original positions for items for swap back. Map {itemID, Slot}

  zeal->commands_hook->Add(
      "/bandolier", {"/bd", "/band"}, "Load, save, delete or list your bandolier sets.",
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
          int slot = 0;
          if (Zeal::String::compare_insensitive(args[1], "bag") && Zeal::String::tryParse(args[2], &slot, true) &&
              slot > 0 && slot <= GAME_NUM_INVENTORY_PACK_SLOTS) {
            Zeal::Game::print_chat("Preferred Bandolier bag set to pack slot %d", slot);
            setting_bag_slot.set(slot);
          }
        }
        Zeal::Game::print_chat("usage: /band save/load/delete <name>, /band list, /band bag <1 to 8>");
        return true;
      });
}

Bandolier::~Bandolier() {}

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

// Verify the bandolier name is valid.
static bool is_valid_name(const std::string &name) {
  if (name.empty() || name.size() > 32) {
    Zeal::Game::print_chat("Invalid bandolier name");
    return false;
  }
  return true;
}

// Saves currently equiped primary, secondary, range and ammo slots.
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
    auto *item = char_info->InventoryItem[BANDOLIER_SLOTS[i]];
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

 if (is_swapping) {
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

  steps.clear();
  for (size_t i = 0; i < BANDOLIER_SLOTS.size(); ++i) {
    int item_id = ini.getValue<int>(name, std::to_string(i));
    Zeal::GameStructures::GAMEITEMINFO *equipped = char_info->InventoryItem[BANDOLIER_SLOTS[i]];
    int equip_slot = BANDOLIER_SLOTS[i] + 1;  // Convert to global

    if (item_id == 0) {
      // No item to equip in this slot, but if there is an item currently equiped we need to unequip it.
      if (equipped) {
        
        // Find an empty slot
        int dst_slot = find_empty_inventory_slot(char_info, equipped);
        if (dst_slot == -1) {
          Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "No empty inventory slot to unequip [%s], canceling set load.", equipped->Name);
          return;
        }
        if(dst_slot < GAME_EQUIPMENT_SLOTS_END) dst_slot++;  // Convert to global

        // Unequip steps should be perfmored first, so insert at the beginning of the list.
        steps.insert(steps.begin(), {.itemID = equipped->ID, .first_slot = equip_slot, .second_slot = dst_slot, .action = Unequip});
      }
    } else {
      // If there is an already an item in the equipment slot, we need to swap it or unequip it first

      // The item to equip is already equiped, skip.
      if (equipped && equipped->ID == item_id) {
        continue;
      }

      // Find the source item in the inventory
      int src_slot = find_item_in_inventory(char_info, item_id);
      if (src_slot == -1) {
        Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "Item with ID [%d] not found in inventory for bandolier set [%s], canceling set load.", item_id, name.c_str());
        return;
      }
      if (src_slot < GAME_EQUIPMENT_SLOTS_END) src_slot++; // Convert to global

      // Store it's original position in case we need to swap it back later (Do not store slots from our bandolier set)
      if (std::find(BANDOLIER_SLOTS.begin(), BANDOLIER_SLOTS.end(), src_slot) == BANDOLIER_SLOTS.end()) {
        original_position[item_id] = src_slot;
      }

      if (!equipped) {
        // If there is no item currently equiped, we can just equip the new item without needing to swap or unequip anything first.
        steps.push_back({.itemID = item_id, .first_slot = src_slot, .second_slot = equip_slot, .action = Equip});
        continue;
      }

      // Can we swap both items? Check if equipped item fits on source item container
      if (Zeal::Game::can_go_in_inventory_slot_id(equipped, src_slot) && !original_position.contains(item_id)) {

        // If we can swap, add it to the steps list as a single ste
        // Equip steps should be done last, so insert at the end of the list.
        steps.push_back({.itemID = item_id, .first_slot = src_slot, .second_slot = equip_slot, .action = Equip});
      } else {
        // Otherwise, we need to unequip the currently equipped item first, then equip the new item. Two different steps
      
        // Find an empty slot
        int dst_slot = find_empty_inventory_slot(char_info, equipped);
        if (dst_slot == -1) {
          Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "No empty inventory slot to unequip [%s], canceling set load.", equipped->Name);
          return;
        }

        steps.push_back({.itemID = equipped->ID, .first_slot = equip_slot, .second_slot = dst_slot, .action = Unequip});
        steps.push_back({.itemID = item_id, .first_slot = src_slot, .second_slot = equip_slot, .action = Equip});
      }  
    }      
  }

  if (steps.empty()) {
    Zeal::Game::print_chat("Bandolier set [%s] is already equiped", name.c_str());
    return;
  }

  // Set flags to start the equipping process in the tick function.
  is_swapping = true;
}

void Bandolier::tick() {
  if (!is_swapping) return;
  if (steps.empty()) return;

  // Process unequip steps first to avoid locking items in the cursor (i.e. equipping a two hander on a dualweild setup)
  auto it = std::find_if(steps.begin(), steps.end(), [](const SetStep &s) { return s.action == Unequip; });
  if (it == steps.end()) {
    it = steps.begin();
  }

  const char *error = Zeal::Game::swap_inventory_slot_items_through_cursor(it->first_slot, it->second_slot, true);
  if (error) {
    Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, error);
    steps.clear();
    is_swapping = false;
    return;
  }

  steps.erase(it);

  if (steps.empty()) {
    is_swapping = false;
    Zeal::Game::print_chat("Bandolier set swap complete");
  }
}


// Returns the inventory slot ID of the item if found in bags, otherwise returns -1
int Bandolier::find_item_in_inventory(Zeal::GameStructures::GAMECHARINFO *char_info, int item_id) {
  if (item_id <= 0) return -1;

  // Look through each inventory pack slot for the item
  // Slot ID for bagged items is 250 + (bag_i*10) + (contents_i) = [250...329]
  for (int pack_slot = 0; pack_slot < GAME_NUM_INVENTORY_PACK_SLOTS; pack_slot++) {
    auto *slot_info = char_info->InventoryPackItem[pack_slot];

    if (!slot_info) continue;

    // Check if the item is directly in the pack slot (not inside a bag)
    if (slot_info->ID == item_id) {
      return GAME_PACKS_SLOTS_START + pack_slot;
    }

    if (slot_info->Type != 1) continue;
    // if it's a container, check inside it for the item
    for (int slot = 0; slot < slot_info->Container.Capacity; slot++) {
      Zeal::GameStructures::GAMEITEMINFO *item = slot_info->Container.Item[slot];
      if (item && item->ID == item_id) {
        return GAME_CONTAINER_SLOTS_START + (pack_slot * GAME_NUM_CONTAINER_SLOTS) + slot;
      }
    }
  }

  // Look through equipped inventory slots for the item
  // Equipped slot IDs are 1-22
  for (int i = 0; i < GAME_NUM_INVENTORY_SLOTS; i++) {
    if (char_info->InventoryItem[i] && char_info->InventoryItem[i]->ID == item_id) {
      return i;
    }
  }

  return -1;
}

int Bandolier::find_empty_inventory_slot(Zeal::GameStructures::GAMECHARINFO *char_info,
                                         Zeal::GameStructures::GAMEITEMINFO *item) {

  // Check first if we have items original position stored from previous bandolier set swaps
  if (original_position.contains(item->ID)) {
    int slot = original_position[item->ID];
    original_position.erase(item->ID);

    if (Zeal::Game::can_go_in_inventory_slot_id(item, slot)) {
      return slot;
    }
  } 

  // Priorize finding an empty slot in bags first, if none found then look for empty regular slot

  // Look through each inventory pack slot for the item
  // Slot ID for bagged items is 250 + (bag_i*10) + (contents_i) = [250...329]
  for (int pack_slot = 0; pack_slot < GAME_NUM_INVENTORY_PACK_SLOTS; pack_slot++) {
    auto *slot_info = char_info->InventoryPackItem[pack_slot];

    // If it's not a container, skip
    if (!slot_info || slot_info->Type != 1) continue;

    // if the container can't fit the item, skip
    if (slot_info->Container.SizeCapacity < item->Size) continue;
    
    for (int slot = 0; slot < slot_info->Container.Capacity; slot++) {
      // Check if slot is empty and is not reserved for a previously unequipped item
      auto is_reserved = std::any_of(steps.begin(), steps.end(), [&](const SetStep &s) {
          return s.second_slot == GAME_CONTAINER_SLOTS_START + (pack_slot * GAME_NUM_CONTAINER_SLOTS) + slot;
      });
      if (!slot_info->Container.Item[slot] && !is_reserved) {
        return GAME_CONTAINER_SLOTS_START + (pack_slot * GAME_NUM_CONTAINER_SLOTS) + slot;
      }
    }
  }

  // If no empty bag can hold the unequipped item, look for an empty slot without container
  for (int pack_slot = 0; pack_slot < GAME_NUM_INVENTORY_PACK_SLOTS; pack_slot++) {
    if (!char_info->InventoryPackItem[pack_slot]) {
      return GAME_CONTAINER_SLOTS_START + pack_slot;
    }
  }

  return -1;
}

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
  if ((self->ActorInfo->CastingSpellId != kInvalidSpellId) && !Zeal::Game::GameInternal::IsPlayerABardAndSingingASong()) {
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

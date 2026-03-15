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

  auto *char_info = Zeal::Game::get_char_info();
  if (!char_info) return;

  steps.clear();
  for (size_t i = 0; i < BANDOLIER_SLOTS.size(); ++i) {
    int item_id = ini.getValue<int>(name, std::to_string(i));
    auto *equipped = char_info->InventoryItem[BANDOLIER_SLOTS[i]];

    if (item_id == 0) {
      // No item to equip in this slot, but if there is an item currently equiped we need to unequip it.
      if (equipped) {
        // Unequip steps should be perfmored first, so insert at the beginning of the list.
        steps.insert(steps.begin(), {BANDOLIER_SLOTS[i], equipped->ID, Unequip});
      }
    } else {
      if (equipped && equipped->ID == item_id) {
        // The item to equip is already equiped, skip.
        continue;
      }
      // Equip steps should be done last, so insert at the end of the list.
      steps.push_back({BANDOLIER_SLOTS[i], item_id, Equip});
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

  // Wait for the delay to expire before processing the next step.
  if (tick_delay > 0) {
    --tick_delay;
    return;
  }

  SetStep step = steps.front();
  if (step.action == Unequip) {
    unequip_set(step.slot);
  } else {
    equip_set(step.itemID, step.slot);
  } 

  steps.erase(steps.begin());
  if (steps.empty()) {
    Zeal::Game::print_chat("Finished equipping bandolier set.");
    is_swapping = false;
  }

  tick_delay = kStepDelayTicks;
}

bool Bandolier::unequip_set(int slot_to_unequip) { 
  auto *char_info = Zeal::Game::get_char_info(); 

  // If the slot is already empty, skip.
  if (!char_info->InventoryItem[slot_to_unequip]) {
    return true;
  }

  Zeal::GameUI::CInvSlotMgr *inv_slot_mgr = Zeal::Game::Windows->InvSlotMgr;
  if (!inv_slot_mgr) return false;

  // If the cursor is holding an item or coins, we can't swap items. skip
  if (char_info->CursorItem || char_info->CursorCopper || char_info->CursorGold || char_info->CursorPlatinum || char_info->CursorSilver) 
    return false;

  auto item_to_unequip = char_info->InventoryItem[slot_to_unequip];

  // Check if the item has the original position stored, if not find a empty inventory slot
  auto it = original_position.find(item_to_unequip->ID);
  int dst_slot = (it != original_position.end()) ? it->second 
      : find_empty_inventory_slot(char_info, slot_to_unequip);
  if (dst_slot == -1) {
    return false;
  }

  auto slots_data = get_inventory_slots(char_info, slot_to_unequip + 1, dst_slot);
  if (!slots_data.source_slot || !slots_data.dest_slot) {
    return false;
  }

  swap_items(char_info, slots_data);
  return true;

  // Zeal::Game::move_item(int from_slot, int to_slot, int print_error, int a3)
  //Zeal::Game::move_item(slot_to_unequip, empty_slot, 0, 1);
  //return true;
}

void Bandolier::equip_set(int item_to_equip, int dest_slot) { 

  auto *char_info = Zeal::Game::get_char_info();

  // If the cursor is holding an item or coins, we can't swap items. skip
  if (char_info->CursorItem || char_info->CursorCopper || char_info->CursorGold || char_info->CursorPlatinum || char_info->CursorSilver) return;

  Zeal::GameUI::CInvSlotMgr *inv_slot_mgr = Zeal::Game::Windows->InvSlotMgr;
  if (!inv_slot_mgr) return;

  // Posible scenarios:
  // 1. The item to equip is already equipped in the destination slot, Skip.
  // 2. The destination slot is empty, equip directly.
  // 3. The item currently equiped is different than the item we want to equip. Try to swap if possible, if not move the
  // currently equipped item to an empty slot and then equip the new item.

  // 1. The item to equip is already equipped in the destination slot, Skip.
  if (char_info->InventoryItem[dest_slot] && item_to_equip == char_info->InventoryItem[dest_slot]->ID) {
    return;
  }

  // Look for the item to equip in the inventory, if it's not there we can't equip it, skip.
  auto item_to_equip_position = find_item_in_inventory(char_info, item_to_equip);
  if (item_to_equip_position == -1) {
    return;
  }

  // Store the original position of the item to equip for swap back.
  original_position[item_to_equip] = item_to_equip_position;

  int inv_index = dest_slot + 1;  // Pack slots are index + 1 on FindInvSlot().
  auto slots_data = get_inventory_slots(char_info, item_to_equip_position, inv_index);
  if (!slots_data.source_slot || !slots_data.dest_slot) {
    return;
  }

  // Check if we can actually equip the item
  if (!Zeal::Game::can_use_item(char_info, slots_data.source_item)) {
    return;
  }

  // 2. The destination slot is empty, equip directly.
  if (!slots_data.dest_item) {
    swap_items(char_info, slots_data);
    return;
  }

  // 3. The item currently equiped is different than the item we want to equip. Try to swap if possible
  // Before swapping, check if the currently equiped item can be stored in the same slot where the item to equip is located.
  // If not, we need to move the currently equiped item to an empty slot first.
  if (!can_be_stored(char_info, item_to_equip_position, slots_data.dest_item)) {
    unequip_set(dest_slot);
    swap_items(char_info, slots_data);
  } else {
    swap_items(char_info, slots_data, true);
  }
}

Bandolier::ItemSwapData Bandolier::get_inventory_slots(Zeal::GameStructures::GAMECHARINFO *char_info, int source_idx, int dst_idx) {
  
  Zeal::GameUI::CInvSlotMgr *inv_slot_mgr = Zeal::Game::Windows->InvSlotMgr;
  if (!inv_slot_mgr) return {};

  Zeal::GameUI::InvSlot *src_slot = inv_slot_mgr->FindInvSlot(source_idx);
  if (!src_slot) {
    return {};
  }

  Zeal::GameUI::InvSlot *dst_slot = inv_slot_mgr->FindInvSlot(dst_idx);
  if (!dst_slot) {
    return {};
  }

  Zeal::GameStructures::GAMEITEMINFO *src_item = (Zeal::GameStructures::GAMEITEMINFO *)src_slot->Item;
  if (!src_item) {
    return {};
  }

  Zeal::GameStructures::GAMEITEMINFO *dst_item = (Zeal::GameStructures::GAMEITEMINFO *)dst_slot->Item;

  return {.source_slot = src_slot, .dest_slot = dst_slot, .source_item = src_item, .dest_item = dst_item};
}

bool Bandolier::swap_items(Zeal::GameStructures::GAMECHARINFO *char_info, ItemSwapData swap_data, bool swap) {

  Zeal::GameUI::CXWndManager *wnd_mgr = Zeal::Game::get_wnd_manager();
  if (!wnd_mgr) return false;

  // Pre-move:
  // - Force the keyboard modifiers 'SHIFT'.
  // - This ensure we pick up stacked items when sending LeftClick events.
  wnd_mgr->ShiftKeyState = 1;
  wnd_mgr->ControlKeyState = 0;
  wnd_mgr->AltKeyState = 0;

  if (swap) {
    // (1) Pickup the bagged item.
    swap_data.source_slot->HandleLButtonUp();
    // Now we should be holding the item to swap on our Cursor
    if (char_info->CursorItem == swap_data.source_item) {
      // (2) Equip it in the destination slot.
      swap_data.dest_slot->HandleLButtonUp();
      // Now we should be holding the swapped-out item instead (if there was one equipped).
      if (swap_data.dest_item && char_info->CursorItem && char_info->CursorItem->ID == swap_data.dest_item->ID) {
        // (3) Put the swapped-out item in the bag slot we just emptied.
        swap_data.source_slot->HandleLButtonUp();
      }
    }

  } else {
    // (1) Pickup the item in the source slot.
    swap_data.source_slot->HandleLButtonUp();
    // Now we should be holding the item on our Cursor
    if (char_info->CursorItem && char_info->CursorItem == swap_data.source_item) {
      // (2) Place it in the destination slot.
      swap_data.dest_slot->HandleLButtonUp();
      // Done. The item should now be in the destination slot.
    } 
  }

  // Done. Restore the keyboard flags.
  wnd_mgr->ShiftKeyState = wnd_mgr->ShiftKeyState;
  wnd_mgr->ControlKeyState = wnd_mgr->ControlKeyState;
  wnd_mgr->AltKeyState = wnd_mgr->AltKeyState;

  return true;
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
      return i + 1;
    }
  }

  return -1;
}

int Bandolier::find_empty_inventory_slot(Zeal::GameStructures::GAMECHARINFO *char_info, int inv_slot) {

  // Priorize finding an empty slot in bags first, if none found then look for empty regular slot
  Zeal::GameStructures::GAMEITEMINFO *item = char_info->InventoryItem[inv_slot];

  // Look through each inventory pack slot for the item
  // Slot ID for bagged items is 250 + (bag_i*10) + (contents_i) = [250...329]
  for (int pack_slot = 0; pack_slot < GAME_NUM_INVENTORY_PACK_SLOTS; pack_slot++) {
    auto *slot_info = char_info->InventoryPackItem[pack_slot];

    // If it's not a container, skip
    if (!slot_info || slot_info->Type != 1) continue;

    // if the container can't fit the item, skip
    if (slot_info->Container.SizeCapacity < item->Size) continue;
    
    for (int slot = 0; slot < slot_info->Container.Capacity; slot++) {
      if (!slot_info->Container.Item[slot]) {
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

bool Bandolier::can_be_stored(Zeal::GameStructures::GAMECHARINFO *char_info, int inv_slot,
                              Zeal::GameStructures::GAMEITEMINFO *item) {
  // If slot is from equipment, do not swap
  if (inv_slot >= GAME_EQUIPMENT_SLOTS_START && inv_slot <= GAME_EQUIPMENT_SLOTS_END) {
    return false;
  }

  // If slot is from regular inventory, not a container. Swap allowed
  if (inv_slot >= GAME_PACKS_SLOTS_START && inv_slot <= GAME_PACKS_SLOTS_END) {
    return true;
  }

  // If slot is from a container, check if the item can fit in the container
   auto container_index = (inv_slot - GAME_CONTAINER_SLOTS_START) / GAME_NUM_CONTAINER_SLOTS;
   auto *container_info = char_info->InventoryPackItem[container_index];
   if (!container_info || container_info->Type != 1) {
     return false;
   }

   // If the container can't fit the item, skip
   if (container_info->Container.SizeCapacity < item->Size) {
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

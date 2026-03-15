#pragma once

#include <Windows.h>

#include <array>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "game_ui.h"
#include "io_ini.h"
#include "zeal_settings.h"

class Bandolier {
 public:
  Bandolier(class ZealService *zeal);
  ~Bandolier();

 private:
  static constexpr int kPrimarySlot = 12;
  static constexpr int kSecondarySlot = 13;
  static constexpr int kRangeSlot = 10;
  static constexpr int kAmmoSlot = 20;
  static constexpr int kManagedSlots = 4;
  static constexpr int kStepDelayTicks = 10;    // Allow some ticks between each equip/unequip action to allow the client to process the change
  static constexpr std::array<int, kManagedSlots> BANDOLIER_SLOTS = {kPrimarySlot, kSecondarySlot, kRangeSlot, kAmmoSlot};

  enum ActionType { None, Unequip, Equip };  // Type of action to perform

  struct SetStep {
    int slot = -1;
    int itemID = 0;
    ActionType action = None;
  };

  // Stores swap items data
  struct ItemSwapData {
    Zeal::GameUI::InvSlot *source_slot;
    Zeal::GameUI::InvSlot *dest_slot;
    Zeal::GameStructures::GAMEITEMINFO *source_item;
    Zeal::GameStructures::GAMEITEMINFO *dest_item;
  };

  int tick_delay = 0;
  bool is_swapping = false;                     // Flag to indicate if a swap is currently in progress
  std::vector<SetStep> steps;                   // List of steps to perform for loading the new set
  std::map<int, int> original_position;         // List of original positions for items for swap back. Map {itemID, Slot}

  void tick();
  bool unequip_set(int slot_to_unequip);
  void equip_set(int item_to_equip, int dest_slot);
  bool swap_items(Zeal::GameStructures::GAMECHARINFO *char_info, ItemSwapData swap_data, bool swap = false);

  ItemSwapData get_inventory_slots(Zeal::GameStructures::GAMECHARINFO *char_info, int source_idx, int dst_idx);
  bool can_be_stored(Zeal::GameStructures::GAMECHARINFO *char_info, int inv_slot, Zeal::GameStructures::GAMEITEMINFO *item);
  int find_empty_inventory_slot(Zeal::GameStructures::GAMECHARINFO *char_info, int inv_slot);
  int find_item_in_inventory(Zeal::GameStructures::GAMECHARINFO *char_info, int item_id);

  // File system storage of bandolier sets.
  void initialize_ini_filename();
  void save(const std::string &name);
  void load(const std::string &name);
  void remove(const std::string &name);

  IO_ini ini = IO_ini(".\\bandolier.ini");
};

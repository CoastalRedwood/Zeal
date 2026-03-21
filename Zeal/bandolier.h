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
  static constexpr int kPrimarySlot = 13;  // Global slot IDs
  static constexpr int kSecondarySlot = 14;
  static constexpr int kRangeSlot = 11;
  static constexpr int kAmmoSlot = 21;
  static constexpr int kManagedSlots = 4;
  static constexpr std::array<int, kManagedSlots> BANDOLIER_SLOTS = {kPrimarySlot, kSecondarySlot, kRangeSlot,
                                                                     kAmmoSlot};

  struct SetStep {
    int from_slot_id = -1;
    int to_slot_id = -1;
  };

  std::vector<SetStep> steps;            // Sequence of steps to perform to swap to a new set (non-empty when active)
  std::map<int, int> original_position;  // List of original positions for items for swap back. Map {itemID, Slot}
  ZealSetting<int> setting_bag_slot = {0, "Zeal", "BandolierBagSlot", true};  // Preferred swap to bag.

  void tick();
  bool check_player_can_swap(Zeal::GameStructures::GAMECHARINFO *char_info);
  int find_empty_inventory_slot(Zeal::GameStructures::GAMECHARINFO *char_info, Zeal::GameStructures::GAMEITEMINFO *item,
                                std::vector<int> &reserved_slots);

  // File system storage of bandolier sets.
  void initialize_ini_filename();
  void save(const std::string &name);
  void load(const std::string &name);
  void remove(const std::string &name);

  IO_ini ini = IO_ini(".\\bandolier.ini");  // Filename updated later to per character.
};

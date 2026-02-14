#pragma once

#include <string>
#include <vector>

#include "game_structures.h"
#include "io_ini.h"

// Bandoleer integrates with Melody to automatically swap instruments for each song gem.
// During the last second of a song's cast, it swaps the configured instrument in so the
// song lands with the instrument bonus. After the song lands and before the next song
// starts casting, it swaps the original weapons back.
class Bandoleer {
 public:
  Bandoleer(class ZealService *zeal);
  ~Bandoleer();

  // Called by Melody right BEFORE casting a new song to restore weapons from any previous swap.
  void restore_if_swapped();

  // Called by Melody right AFTER a new song starts casting to begin monitoring the cast timer.
  void notify_song_start(int gem_index);

  // Called by Melody when it ends to ensure weapons are restored.
  void notify_melody_stop();

 private:
  // Equipment slot indices managed by the bandoleer (0-based InventoryItem indices).
  static constexpr int kPrimarySlot = 12;
  static constexpr int kSecondarySlot = 13;
  static constexpr int kRangeSlot = 10;
  static constexpr int kNumManagedSlots = 3;
  static constexpr int kManagedSlots[kNumManagedSlots] = {kPrimarySlot, kSecondarySlot, kRangeSlot};

  // Threshold in milliseconds before song finishes to swap instruments in.
  static constexpr DWORD kSwapThresholdMs = 1000;

  enum class State { Idle, Monitoring, Swapped };

  // Tracks a single equipment slot that was swapped.
  struct SwapRecord {
    int equip_slot = -1;      // Equipment slot index (0-based).
    std::string orig_name;    // Name of the item that was displaced.
    int orig_id = 0;          // ID of the item that was displaced (0 if slot was empty).
  };

  void tick();
  void swap_instruments_in();
  void swap_weapons_back();

  void initialize_ini_filename();
  void save(int gem_number);
  void clear(int gem_number);
  void list();

  // Searches inventory bags for an item matching the id and name. Returns bag slot id (250+) or -1.
  int find_item_in_bags(int item_id, const std::string &item_name);

  // Swaps an item from a bag slot into an equipment slot using the InvSlot click mechanism.
  bool swap_item(int bag_slot_id, int equip_slot_index);

  // Returns a display label for an equipment slot index.
  static const char *get_slot_label(int slot_index);

  // Returns true if the given gem (0-based) has any bandoleer items configured.
  bool has_config_for_gem(int gem_index);

  State state = State::Idle;
  int active_gem = -1;                    // 0-based gem index being monitored (-1 if none).
  std::vector<SwapRecord> active_swaps;   // Tracks what was swapped for restoration.

  IO_ini ini = IO_ini(".\\bandoleer.ini");  // Filename updated later to per character.
};

#pragma once
#include <Windows.h>

#include <string>
#include <vector>

#include "game_structures.h"

// One step of a melody: a song gem or an item with a bard click effect
struct MelodyStep {
  int gem = -1;           // 0-based spell gem, or -1 for an item step.
  int item_slot_id = -1;  // Global inventory slot id for an item step, or -1.
  std::string key;        // Token as typed ("2", "0.10", "2.3", "Breath") for print messages.

  bool is_gem() const { return gem >= 0; }  // Returns true if this step points to a gem slot.

  bool is_item() const { return item_slot_id >= 0; }  // Returns true if this step points to an item slot_id.

  bool is_item_valid(
      bool do_print = false) const;  // Returns true if a valid bard clicky is currently in the item slot_id.

  bool is_valid(bool do_print = false) const { return is_gem() || is_item_valid(do_print); }

  explicit MelodyStep(const std::string &token);
};

class Melody {
 public:
  bool start(const std::vector<MelodyStep> &new_steps, bool resume = false);  // returns true if no errors
  void resume();  // continues a stopped melody where it was interrupted (if valid)
  void end(bool do_print = false, bool full_stop = true);
  void handle_stop_cast_callback(BYTE reason, WORD spell_id);

  void handle_deactivate_ui();
  bool use_item(int item_index);  // Returns true if melody queued /useitem(item_index).
  Melody(class ZealService *pHookWrapper);
  ~Melody();

 private:
  void tick();
  int get_next_gem_index();
  bool is_gem_ready(int gem_index);
  void stop_current_cast();
  bool handle_opcode(int opcode, char *buffer, UINT len);
  bool activate_use_item();
  void dump() const;

  bool is_active = false;                          // Set when melody is actively running.
  bool is_verbose = false;                         // Emits extra chat spam messages (debug).
  int current_index = 0;                           // Active step index. -1 if not started yet.
  std::vector<MelodyStep> songs;                   // The rotation: gem steps and item steps.
  int retry_count = 0;                             // Tracks unsuccessful song casts.
  WORD casting_melody_spell_id = kInvalidSpellId;  // Current melody song being cast. Is only a valid id while cast
                                                   // window is visible (actively casting).
  WORD retry_spell_id = kInvalidSpellId;           // Song failed (fizzled or otherwise, retry).
  WORD deferred_spell_id = kInvalidSpellId;        // Song wasn't ready so deferred to next opportunity.
  int use_item_index = -1;                         // The pending use_item() to try.
  int use_item_ack_state = 0;                      // Tracks special server ack case for use items.
  ULONGLONG use_item_timeout = 0;                  // The max timestamp until the pending use_item() gives up.
  ULONGLONG enter_zone_time = 0;                   // Timestamp of the most recent enter zone callback.
  ULONGLONG casting_visible_timestamp = 0;         // Timestamps to add minimum delay after casting ends
  ULONGLONG start_of_cast_timestamp = 0;           // and to detect excessive retries.
};

#pragma once
#include <Windows.h>

#include <string>
#include <vector>

#include "game_structures.h"

// One step of a melody: a song gem, or an item to click (an instant bard clicky such as Breath of
// Harmony, or any clicky). `key` is the token as typed ("i22", "i2.3", "iBreath"), for messages.
struct MelodyStep {
  int gem = -1;        // 0-based spell gem, or -1 for an item step.
  int item_slot = -1;  // Global inventory slot id for an item step, or -1.
  std::string key;
  bool is_item() const { return item_slot >= 0; }
};

class Melody {
 public:
  bool start(const std::vector<MelodyStep> &new_steps, bool resume = false);  // returns true if no errors
  void resume();  // continues a stopped melody where it was interrupted (if valid)
  void end(bool do_print = false);
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
  bool handle_opcode(int opcode);
  // Parses one /melody token into a step: "3" is gem 3, "i22" the /useitem slot 22, "i2.3" bag 2
  // slot 3, "iBreath" a case-sensitive partial item name. Prints and returns false on a bad token.
  static bool parse_step(const std::string &token, MelodyStep *out);
  // If the step after the current one is an item, queues it as the pending use_item() and advances.
  void queue_item_step_if_next();
  // After a failed cast, holds the next attempt back longer the more attempts have failed in a row.
  void back_off_after_failure();
  bool is_active = false;                          // Set when melody is actively running.
  int current_index = 0;                           // Active step index. -1 if not started yet.
  std::vector<MelodyStep> songs;                   // The rotation: gem steps and item steps.
  int retry_count = 0;                             // Tracks unsuccessful song casts.
  WORD casting_melody_spell_id = kInvalidSpellId;  // Current melody song being cast. Is only a valid id while cast
                                                   // window is visible (actively casting).
  WORD retry_spell_id = kInvalidSpellId;           // Song failed (fizzled or otherwise, retry).
  WORD deferred_spell_id = kInvalidSpellId;        // Song wasn't ready so deferred to next opportunity.
  int use_item_index = -1;                         // The pending use_item() to try.
  ULONGLONG retry_not_before = 0;                  // After repeated failures: the earliest next cast or click.
  int use_item_ack_state = 0;                      // Tracks special server ack case for use items.
  ULONGLONG use_item_timeout = 0;                  // The max timestamp until the pending use_item() gives up.
  ULONGLONG enter_zone_time = 0;                   // Timestamp of the most recent enter zone callback.
};
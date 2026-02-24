#pragma once
#include <Windows.h>

#include <vector>

#include "game_structures.h"

class Melody {
 public:
  bool start(const std::vector<int> &new_songs, bool resume = false);  // returns true if no errors
  void resume();  // continues a stopped melody where it was interrupted (if valid)
  void end(bool do_print = false);
  void handle_stop_cast_callback(BYTE reason, WORD spell_id);
  void handle_deactivate_ui();
  bool use_item(int item_index);  // asks Melody to handle /useitem command. Returns true if melody handled the command.
  Melody(class ZealService *pHookWrapper);
  ~Melody();

 private:
  void tick();
  int get_next_gem_index();
  bool is_gem_ready(int gem_index);
  void stop_current_cast();
  bool is_active = false;                          // Set when melody is actively running.
  int current_index = 0;                           // Active song index. -1 if not started yet.
  std::vector<int> songs;                          // Gem indices (base 0) for melody.
  int retry_count = 0;                             // Tracks unsuccessful song casts.
  WORD casting_melody_spell_id = kInvalidSpellId;  // Current melody song being cast. Is only a valid id while cast
                                                   // window is visible (actively casting).
  WORD retry_spell_id = kInvalidSpellId;           // Song failed (fizzled or otherwise, retry).
  WORD deferred_spell_id = kInvalidSpellId;        // Song wasn't ready so deferred to next opportunity.
  int use_item_index = -1;                         // The pending use_item() to try.
  ULONGLONG use_item_timeout = 0;                  // The max timestamp until the pending use_item() gives up.
  ULONGLONG enter_zone_time = 0;                   // Timestamp of the most recent enter zone callback.
};
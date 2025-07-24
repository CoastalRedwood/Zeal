#pragma once
#include <stdint.h>

#include "game_structures.h"
#include "game_ui.h"
#include "hook_wrapper.h"
#include "memory.h"

class Physics {
 public:
  Physics(class ZealService *zeal);
  ~Physics();
  bool can_move(short spawn_id);

 private:
  bool did_physics = false;
  ULONGLONG last_physic_calc = 0;
  std::unordered_map<short, UINT> move_timers;
};

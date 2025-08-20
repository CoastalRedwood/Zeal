#pragma once
#include <Windows.h>

#include <unordered_map>

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

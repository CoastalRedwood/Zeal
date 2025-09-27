#pragma once
#include <Windows.h>

#include <vector>

class Physics {
 public:
  Physics(class ZealService *zeal);
  ~Physics();
  bool can_move(short spawn_id);

 private:
  void clear_timers();

  std::vector<UINT> move_timers;  // Per spawn ID timestamp of last movement.
};

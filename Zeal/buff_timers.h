#pragma once
#include "game_structures.h"

class BuffDetails {
 public:
  BuffDetails(size_t index, Zeal::GameStructures::_GAMEBUFFINFO buff);
  ~BuffDetails(){};
  size_t BuffSlot;
  Zeal::GameStructures::_GAMEBUFFINFO Buff;
};

class BuffTimers {
 public:
  BuffTimers(class ZealService *zeal);
  ~BuffTimers(){};

 private:
  void print_timers(void);
};

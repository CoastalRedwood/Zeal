#pragma once
#include "game_structures.h"
#include "hook_wrapper.h"
#include "memory.h"

class CycleTarget {
 public:
  CycleTarget(class ZealService *zeal);
  ~CycleTarget();
  Zeal::GameStructures::Entity *get_next_ent(float dist, BYTE type);
  Zeal::GameStructures::Entity *get_nearest_ent(float dist, BYTE type);

 private:
  // hook* hook;
};

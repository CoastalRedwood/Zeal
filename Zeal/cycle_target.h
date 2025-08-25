#pragma once

#include <utility>
#include <vector>

#include "game_structures.h"

class CycleTarget {
 public:
  CycleTarget(class ZealService *zeal);
  Zeal::GameStructures::Entity *get_next_ent(float dist, BYTE type);
  Zeal::GameStructures::Entity *get_nearest_ent(float dist, BYTE type);
  void handle_next_target(int key_down, Zeal::GameEnums::EntityTypes type);
  void handle_toggle_last_two(int key_down);

 private:
  void main_loop();
  void on_zone();
  void add_index();

  std::pair<int, int> last_targets;
  size_t last_index = -1;
  std::vector<Zeal::GameStructures::Entity *> near_ents;
};

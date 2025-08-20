#pragma once
#include "game_structures.h"

class AutoFire {
 public:
  bool autofire = false;
  // bool HandleDoAttack(Zeal::GameStructures::Entity *player, uint8_t type, uint8_t p2,
  //                     Zeal::GameStructures::Entity *target);
  void SetAutoFire(bool enabled, bool do_print = false);
  void Main();
  AutoFire(class ZealService *zeal);
  ~AutoFire();

 private:
  bool was_autoattacking = false;
  bool do_autofire = false;
};

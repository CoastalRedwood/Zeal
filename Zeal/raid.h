#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "raid_autorejoin.h"

class Raid {
 public:
  Raid(class ZealService *zeal);
  ~Raid();

 private:
  void callback_main();
  bool handle_ari(std::vector<std::string>& args);
  RaidAutoRejoin autorejoin_;
  uint64_t last_ari_check_ = 0;
};

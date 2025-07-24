#pragma once
#include <stdint.h>

#include "game_ui.h"
#include "hook_wrapper.h"
#include "zeal_settings.h"

class Tick {
 public:
  DWORD GetTimeUntilTick();
  DWORD GetTickGauge(struct Zeal::GameUI::CXSTR *str);
  Tick(class ZealService *zeal);
  ~Tick();

  ZealSetting<bool> ReverseDirection = {false, "ServerTick", "ReverseDirection", false};

 private:
};

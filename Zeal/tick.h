#pragma once
#include <Windows.h>

#include <functional>
#include <vector>

#include "game_ui.h"
#include "zeal_settings.h"

class Tick {
 public:
  DWORD GetTimeUntilTick();
  DWORD GetTickGauge(struct Zeal::GameUI::CXSTR *str);

  void OnServerTick();

  void AddTickCallback(std::function<void()> callback) { tick_callbacks.push_back(callback); }

  Tick(class ZealService *zeal);
  ~Tick();

  ZealSetting<bool> ReverseDirection = {false, "ServerTick", "ReverseDirection", false};

 private:
  std::vector<std::function<void()>> tick_callbacks;
};

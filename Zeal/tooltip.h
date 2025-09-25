#pragma once
#include "zeal_settings.h"

class Tooltip {
 public:
  ZealSetting<bool> all_containers = {false, "Zeal", "alt_all_containers", false,
                                      [this](const bool& val) { synchronize_alt_all_containers(); }};
  ZealSetting<int> hover_timeout = {500, "Zeal", "TooltipTime", false,
                                    [this](const int& val) { synchronize_hover_timeout(); }};
  Tooltip(class ZealService* pHookWrapper);
  ~Tooltip();

 private:
  void synchronize_hover_timeout();
  void synchronize_alt_all_containers();
};

#pragma once
#include <string>

#include "zeal_settings.h"

class Labels {
 public:
  std::string debug_info;
  void print_debug_info(std::string);
  void print_debug_info(const char *format, ...);
  bool GetLabel(int type, std::string &str);
  int GetGauge(int type, std::string &str);
  Labels(class ZealService *zeal);
  ~Labels();
  void callback_main();

  ZealSetting<bool> setting_show_target_spawn_id = {false, "Labels", "ShowTargetSpawnId"};

 private:
};

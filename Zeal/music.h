#pragma once
#include "zeal_settings.h"

class MusicManager {
 public:
  ZealSetting<bool> ClassicMusic = {false, "Zeal", "ClassicMusic", false};
  MusicManager(class ZealService *pZeal);
  ~MusicManager();

 private:
};

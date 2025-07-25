#pragma once
#include "zeal_settings.h"

class Patches {
 public:
  ZealSetting<bool> BrownSkeletons = {false, "Zeal", "BrownSkeletons", false,
                                      [this](bool val) { SetBrownSkeletons(); }};
  ZealSetting<bool> AutoFollowEnable = {false, "AutoFollow", "Enable", false, [this](bool val) { SyncAutoFollow(); }};
  ZealSetting<float> AutoFollowDistance = {15.f, "AutoFollow", "Distance", false,
                                           [this](bool val) { SyncAutoFollow(); }};
  Patches();
  void fonts();

 private:
  void SetBrownSkeletons();
  void SyncAutoFollow(bool first_boot = false);
};

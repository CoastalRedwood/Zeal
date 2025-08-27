#pragma once
#include "zeal_settings.h"

class Patches {
 public:
  ZealSetting<bool> BrownSkeletons = {false, "Zeal", "BrownSkeletons", false,
                                      [this](bool val) { SetBrownSkeletons(); }};
  Patches();

 private:
  void SetBrownSkeletons();
};

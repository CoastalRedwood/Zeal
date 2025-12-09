#pragma once

#include <string>
#include <vector>

#include "zeal_settings.h"

class Patches {
 public:
  ZealSetting<bool> BrownSkeletons = {false, "Zeal", "BrownSkeletons", false,
                                      [this](bool val) { SetBrownSkeletons(); }};

  ZealSetting<bool> setting_DisableSprites = {false, "SpellEffects", "DisableSprites", false,
                                              [this](bool val) { SyncDisableSprites(); }};

  ZealSetting<int> setting_BardEffects = {0, "SpellEffects", "BardEffects", false,
                                          [this](bool val) { SyncBardEffects(); }};

  Patches();

 private:
  void SetBrownSkeletons();
  void SyncDisableSprites();
  bool SyncBardEffects();
  bool HandleSpellEffectsCommand(const std::vector<std::string>& args);
};

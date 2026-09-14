#pragma once

#include <string>
#include <unordered_map>
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

  ZealSetting<int> setting_BuffEffects = {0, "SpellEffects", "BuffEffects", false,
                                          [this](const int& val) { SyncBuffEffects(); }};

  ZealSetting<std::string> setting_SpellEffectReplacements = {
      "", "SpellEffects", "Replacements", false, [this](const std::string& val) { SyncSpellEffectReplacements(); }};

  Patches();

 private:
  std::unordered_map<int, DWORD> originalSpellEffects;
  std::unordered_map<int, DWORD> individualSpellEffects;

  void SetBrownSkeletons();
  void SyncDisableSprites();
  bool SyncBardEffects();
  bool SyncBuffEffects();
  bool SyncSpellEffects(bool classic);
  void SyncSpellEffectReplacements();
  bool HandleSpellEffectsCommand(const std::vector<std::string>& args);
};
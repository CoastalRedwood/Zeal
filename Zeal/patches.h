#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "game_structures.h"
#include "zeal_settings.h"

class Patches {
 public:
  ZealSetting<bool> BrownSkeletons = {false, "Zeal", "BrownSkeletons", false,
                                      [this](bool val) { SetBrownSkeletons(); }};

  ZealSetting<bool> setting_DisableSprites = {false, "SpellEffects", "DisableSprites", false,
                                              [this](bool val) { SyncDisableSprites(); }};

  ZealSetting<int> setting_BardEffects = {0, "SpellEffects", "BardEffects", false,
                                          [this](bool val) { SyncBardEffects(); }};

  ZealSetting<int> setting_BuffEffects = {-1, "SpellEffects", "BuffEffects", false,
                                          [this](const int& val) { SyncBuffEffects(); }};

  ZealSetting<bool> setting_SpellEffectsClassic = {false, "SpellEffects", "Classic", false,
                                                   [this](bool val) { SyncSpellEffects(val); }};

  ZealSetting<std::string> setting_SpellEffectOverrides = {
      "", "SpellEffects", "Overrides", false, [this](const std::string& val) { LoadSpellEffectOverrides(); }};

  Patches();

 private:
  enum class SpellEffectOverrideType { Classic, ClientDefault, Replacement };

  struct OriginalSpellEffect {
    DWORD new_particle_effect = 0;
  };

  struct SpellEffectOverride {
    SpellEffectOverrideType type;
    DWORD effect = 0;
    int source_spell_id = -1;
  };

  std::unordered_map<int, OriginalSpellEffect> originalSpellEffects;
  std::unordered_map<int, SpellEffectOverride> individualSpellEffects;

  void SetBrownSkeletons();
  void SyncDisableSprites();
  bool SyncBardEffects();
  bool SyncBuffEffects();
  bool SyncSpellEffects(bool classic);
  void LoadSpellEffectOverrides();
  bool HandleSpellEffectsCommand(const std::vector<std::string>& args);
};
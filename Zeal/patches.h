#pragma once

#include <string>
#include <unordered_map>
#include <optional>
#include <mutex>
#include <vector>

#include "game_structures.h"
#include "zeal_settings.h"

// Forward declare old particle record type (defined in game_structures.h)
namespace Zeal { namespace GameStructures { struct SpellEffectRecord; } }

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

  ZealSetting<int> setting_HealingEffects = {-1, "SpellEffects", "HealingEffects", false,
                                             [this](const int& val) { SyncHealingEffects(); }};

  ZealSetting<bool> setting_SpellEffectsClassic = {false, "SpellEffects", "Classic", false,
                                                   [this](bool val) { SyncSpellEffects(val); }};

  ZealSetting<std::string> setting_SpellEffectOverrides = {
      "", "SpellEffects", "Overrides", false, [this](const std::string& val) { LoadSpellEffectOverrides(); }};

  Patches();

 private:
  enum class SpellEffectOverrideType { Classic, ClientDefault, Replacement };

  struct OriginalSpellEffect {
    DWORD new_particle_effect = 0;
    Zeal::GameStructures::SpellEffectRecord* old_particle_effect = nullptr;
  };

  struct SpellEffectOverride {
    SpellEffectOverrideType type;
    DWORD effect = 0;
    int source_spell_id = -1;
    bool use_old_effect = false;
  };

  // Use dense vector indexed by spell id for fast lookups and predictable memory
  // footprint. std::optional indicates presence.
  std::vector<std::optional<OriginalSpellEffect>> originalSpellEffects;
  std::vector<std::optional<SpellEffectOverride>> individualSpellEffects;
  // Per-target allocated copies of old particle records when we clone classic effects.

  // Protect concurrent access to spell-effect structures.
  mutable std::mutex spell_effects_mutex;

  void SetBrownSkeletons();
  void SyncDisableSprites();
  bool SyncBardEffects();
  bool SyncBuffEffects();
  bool SyncHealingEffects();
  bool SyncSpellEffects(bool classic);
  void LoadSpellEffectOverrides();
  bool HandleSpellEffectsCommand(const std::vector<std::string>& args);
  // (No heap-allocated copies retained in this build.)
};
#pragma once
// Supports patching the dpvs.dll sprites crash and the customization of spell effects.

#include <optional>
#include <string>
#include <vector>

#include "game_structures.h"
#include "zeal_settings.h"

class SpellEffects {
 public:
  SpellEffects();

 private:
  enum class SpellEffectOverrideType { ClientDefault, Classic, Replacement };

  struct OriginalSpellEffect {
    // Copied field members from struct SPELL.
    Zeal::GameStructures::SpellEffectRecord* OldParticleEffect = nullptr;
    int SpellAffectIndex = 0;
    DWORD NewParticleEffect = 0;
    int SpellAnim = 0;
  };

  struct SpellEffectOverride {
    SpellEffectOverrideType type;
    int source_spell_id = -1;
    bool use_old_effect = false;
  };

  void Initialize();
  void SyncDisableSprites();
  void CacheOriginalEffects();
  bool SyncBardEffects();
  bool SyncSpellEffects();
  void SyncSpellEffect(Zeal::GameStructures::SPELLMGR* spell_mgr, int spell_id);
  void LoadSpellEffectOverrides();
  bool ParseSpellEffectOverrideLine(const std::string& line);
  void SaveSpellEffectOverrides();
  bool HandleSpellEffectsCommand(const std::vector<std::string>& args);
  void HandleListCommand() const;
  bool HandleSpellLookupCommand(const std::vector<std::string>& args);
  bool HandleSpellOverrideCommand(const std::vector<std::string>& args);
  void PrintUsage() const;

  ZealSetting<bool> setting_disable_sprites = {false, "SpellEffects", "DisableSprites", false};
  ZealSetting<int> setting_bard_effects = {0, "SpellEffects", "BardEffects", false};
  ZealSetting<bool> setting_spell_effects_classic = {false, "SpellEffects", "Classic", false};
  ZealSetting<int> setting_buff_effects = {-1, "SpellEffects", "BuffEffects", false};
  ZealSetting<int> setting_healing_effects = {-1, "SpellEffects", "HealingEffects", false};

  bool initialized = false;
  std::vector<std::optional<OriginalSpellEffect>> original_spell_effects;    // Cached original spell effects.
  std::vector<std::optional<SpellEffectOverride>> individual_spell_effects;  // Individual (per spell) overrides.
};
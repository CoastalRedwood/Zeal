#include "patches.h"

#include "commands.h"
#include "game_functions.h"
#include "game_packets.h"
#include "game_structures.h"
#include "hook_wrapper.h"
#include "memory.h"
#include "string_util.h"
#include "zeal.h"

void __fastcall GetZoneInfoFromNetwork(int *t, int unused, char *p1) {
  int *backup_this = t;

  ZealService::get_instance()->hooks->hook_map["GetZoneInfoFromNetwork"]->original(GetZoneInfoFromNetwork)(t, unused,
                                                                                                           p1);
  int retry_count = 0;
  while (!t) {
    retry_count++;
    Sleep(100);
    t = backup_this;
    ZealService::get_instance()->hooks->hook_map["GetZoneInfoFromNetwork"]->original(GetZoneInfoFromNetwork)(t, unused,
                                                                                                             p1);
    if (retry_count >= 15 && !t) {
      MessageBoxA(NULL, "Zeal attempted to retry GetZoneInfoFromNetwork but has failed", "Crash", 0);
      break;
    }
  }
}

// Base client is tagging other players that die as Type = 2 (NPCCorpse) while tagging a self death
// as Type = 3. Upon camping and rejoining, other player corpses are tagged as Type = 3, so this
// looks like a client bug to patch (impacts corpse nameplates and targeting).
static void __fastcall ProcessDeath(uint32_t passthruECX, uint32_t unusedEDX,
                                    Zeal::Packets::Death_Struct *death_struct) {
  auto *ent = Zeal::Game::get_entity_by_id(death_struct->spawn_id);
  bool player_death = (ent != nullptr && ent->Type == Zeal::GameEnums::Player);
  ZealService::get_instance()->hooks->hook_map["ProcessDeath"]->original(ProcessDeath)(passthruECX, unusedEDX,
                                                                                       death_struct);
  if (player_death && ent->Type == Zeal::GameEnums::NPCCorpse) ent->Type = Zeal::GameEnums::PlayerCorpse;
}

// There is a client startup crash issue where it looks like the CBreathWnd::OnProcessFrame or
// game _3DView::DisplaySpells is calling CanIBreathe with a GAMECHARINFO that has a null SpawnInfo entry.
// The other path calling CanIBreathe protects against this.
static int32_t __fastcall CanIBreathe(Zeal::GameStructures::GAMECHARINFO *self_char_info, uint32_t unusedEDX) {
  if (!self_char_info) return 1;  // Not expected to happen, so just default to true, can breathe.

  // Patch the crashing case (null SpawnInfo) here.
  if (!self_char_info->SpawnInfo) {
    self_char_info->IsSwimmingUnderwater = 0;  // Match the updating behavior of CanIBreathe with an assumption.
    return 1;                                  // And just respond that yes can breathe (for now).
  }

  return ZealService::get_instance()->hooks->hook_map["CanIBreathe"]->original(CanIBreathe)(self_char_info, unusedEDX);
}

void Patches::SetBrownSkeletons() {
  if (BrownSkeletons.get()) {
    mem::write<BYTE>(0x49f297, 0xEB);
  } else {
    mem::write<BYTE>(0x49f297, 0x75);
  }
}

// Support a utility to find a spell by name and return the spell id (or -1 if not found).
static int FindSpellByName(const std::string& name) {
  const auto* spell_mgr = Zeal::Game::get_spell_mgr();
  if (!spell_mgr) return -1;

  for (int spell_id = 0; spell_id < GAME_NUM_SPELLS; ++spell_id) {
    const auto spell = spell_mgr->Spells[spell_id];

    if (spell && spell->Name && _stricmp(spell->Name, name.c_str()) == 0) return spell_id;
  }

  return -1;
}

// Support an option to simply no-op the DoSpriteEffects() call to avoid the dvps.dll crash.
void Patches::SyncDisableSprites() {
  const BYTE kOpcodeNop = 0x90;
  const int kDoSpriteEffectAddr = 0x0052cbb1;
  bool disable = setting_DisableSprites.get();
  bool currently_disabled = (*reinterpret_cast<BYTE *>(kDoSpriteEffectAddr) == kOpcodeNop);
  if (disable == currently_disabled) return;

  if (disable) {
    mem::set(kDoSpriteEffectAddr, kOpcodeNop, 5);  // No-op out the call.
  } else {
    const BYTE orig_code[5] = {0xe8, 0x89, 0xeb, 0xff, 0xff};  // Restore call.
    mem::write(kDoSpriteEffectAddr, orig_code);
  }
}

static constexpr int kNumBardEffects = 3;

static constexpr int kReferenceSongIds[kNumBardEffects] = {717, 2050, 7};

// Option 1: Use the pulsing sphere from Selo's (SpellAnim 98).
// Option 2: Use the more subtle floating notes animation from Nature's Melody (SpellAnim 45).
// Option 3: Use the even more subtle snowflake-like from Hymn/Cantata's (SpellAnim 69).
static const char* kReferenceSongNames[kNumBardEffects] = {"Selo`s Accelerando", "Nature's Melody",
                                                           "Hymn of Restoration"};

static bool IsBardSongId(int spell_id) {
  for (int i = 0; i < kNumBardEffects; ++i) {
    if (kReferenceSongIds[i] == spell_id) return true;
  }

  return false;
}

// These songs are by default all using old particle effects with SpellAffectIndex = 6.
static constexpr std::pair<int, const char*> kSongsToModify[] = {
    {709, "Guardian Rhythms"},
    {710, "Elemental Rhythms"},
    {711, "Purifying Rhythms"},
    {712, "Psalm of Warmth"},
    {713, "Psalm of Cooling"},
    {714, "Psalm of Mystic Shielding"},
    {715, "Psalm of Vitality"},
    {716, "Psalm of Purity"},
    {722, "Jaxan`s Jig o` Vigor"},
    {723, "Cassindra`s Chorus of Clarity"},
    {1287, "Cassindra`s Chant of Clarity"},
    {2607, "Elemental Chorus"},
    {2608, "Purifying Chorus"},
    {3368, "Psalm of Veeshan"},
};

static bool IsBardEffectSpell(int spell_id) {
  for (const auto& pair : kSongsToModify) {
    if (pair.first == spell_id) return true;
  }

  return false;
}

static bool IsBardEffectSpellOrReference(int spell_id) { return IsBardSongId(spell_id) || IsBardEffectSpell(spell_id); }

// Optional bard effects modes to reduce the super bright blue sparklies of constant bard song refreshes.
bool Patches::SyncBardEffects() {
  // First check we have access to the spell list and the reference song is a match.
  const auto *spell_mgr = Zeal::Game::get_spell_mgr();
  if (!spell_mgr) return false;

  int mode = setting_BardEffects.get();
  DWORD new_effect = (DWORD) nullptr;  // Default uses the old particle effect.
  if (mode > 0 && mode <= kNumBardEffects) {
    int index = mode - 1;
    const auto ref_spell = spell_mgr->Spells[kReferenceSongIds[index]];
    if (!ref_spell || !ref_spell->Name || !ref_spell->NewParticleEffect ||
        strcmp(ref_spell->Name, kReferenceSongNames[index]))
      return false;
    new_effect = ref_spell->NewParticleEffect;
  }

  // For every song, double-check it's a match (maybe the db gets updated) and apply the target
  // effect only if there's a match.
  bool no_errors = true;
  for (auto &pair : kSongsToModify) {
    auto spell = spell_mgr->Spells[pair.first];
    if (!spell || !spell->Name || spell->SpellAffectIndex != 6 || strcmp(spell->Name, pair.second))
      no_errors = false;
    else
      spell->NewParticleEffect = new_effect;
  }
  return no_errors;
}

bool Patches::SyncSpellEffects(bool classic) {
  const auto* spell_mgr = Zeal::Game::get_spell_mgr();
  if (!spell_mgr) return false;

  // Capture the client's default effects before applying any overrides.
  for (int spell_id = 0; spell_id < GAME_NUM_SPELLS; ++spell_id) {
    auto spell = spell_mgr->Spells[spell_id];
    if (!spell) continue;

    if (IsBardEffectSpellOrReference(spell_id)) continue;

    if (!originalSpellEffects.contains(spell_id)) {
      originalSpellEffects[spell_id] = {spell->NewParticleEffect};
    }
  }

  // Apply the global Classic/Default setting.
  for (int spell_id = 0; spell_id < GAME_NUM_SPELLS; ++spell_id) {
    auto spell = spell_mgr->Spells[spell_id];
    if (!spell) continue;

    if (IsBardEffectSpellOrReference(spell_id)) continue;

    spell->NewParticleEffect = classic ? (DWORD) nullptr : originalSpellEffects[spell_id].new_particle_effect;
  }


  // Apply the existing Bard preference.
  SyncBardEffects();

  // Individual overrides always take precedence over family/global settings.
  for (const auto& [spell_id, override] : individualSpellEffects) {
    auto spell = spell_mgr->Spells[spell_id];
    if (!spell) continue;

    if (IsBardSongId(spell_id)) continue;

    switch (override.type) {
      case SpellEffectOverrideType::Classic:
        spell->NewParticleEffect = (DWORD) nullptr;
        break;

      case SpellEffectOverrideType::ClientDefault:
        spell->NewParticleEffect = originalSpellEffects[spell_id].new_particle_effect;
        break;

      case SpellEffectOverrideType::Replacement: {
        auto source = originalSpellEffects.find(override.source_spell_id);

        if (source != originalSpellEffects.end() && source->second.new_particle_effect) {
          spell->NewParticleEffect = source->second.new_particle_effect;
        } else if (override.effect) {
          // Runtime replacement fallback.
          spell->NewParticleEffect = override.effect;
        }

        break;
      }
    }
  }

  return true;
}

bool Patches::SyncBuffEffects() {
  static constexpr int kBuffSpellAffectIndex = 2;

  const auto* spell_mgr = Zeal::Game::get_spell_mgr();
  if (!spell_mgr) return false;

  for (int spell_id = 0; spell_id < GAME_NUM_SPELLS; ++spell_id) {
    auto spell = spell_mgr->Spells[spell_id];

    if (!spell || spell->SpellAffectIndex != kBuffSpellAffectIndex) continue;

    if (!originalSpellEffects.contains(spell_id)) {
      originalSpellEffects[spell_id] = {spell->NewParticleEffect};
    }

    auto individual = individualSpellEffects.find(spell_id);

    if (individual != individualSpellEffects.end()) {
      switch (individual->second.type) {
        case SpellEffectOverrideType::Classic:
          spell->NewParticleEffect = (DWORD) nullptr;
          break;

        case SpellEffectOverrideType::ClientDefault:
          spell->NewParticleEffect = originalSpellEffects[spell_id].new_particle_effect;
          break;

        case SpellEffectOverrideType::Replacement: {
          auto source = originalSpellEffects.find(individual->second.source_spell_id);

          if (source != originalSpellEffects.end() && source->second.new_particle_effect) {
            spell->NewParticleEffect = source->second.new_particle_effect;
          } else if (individual->second.effect) {
            spell->NewParticleEffect = individual->second.effect;
          }

          break;
        }
      }
    } else if (setting_BuffEffects.get() == 1) {
      spell->NewParticleEffect = (DWORD) nullptr;
    } else if (setting_BuffEffects.get() == 0) {
      spell->NewParticleEffect = originalSpellEffects[spell_id].new_particle_effect;
    }
  }

  return true;
}

static std::string SetSpellEffectOverride(const std::string& overrides, int target_id, const std::string& value) {
  std::string result;
  size_t start = 0;
  bool found = false;

  while (start < overrides.size()) {
    size_t end = overrides.find(';', start);
    if (end == std::string::npos) end = overrides.size();

    std::string entry = overrides.substr(start, end - start);
    size_t separator = entry.find('=');

    if (separator != std::string::npos) {
      int existing_target = 0;

      if (Zeal::String::tryParse(entry.substr(0, separator), &existing_target, true)) {
        if (existing_target == target_id) {
          if (!found) {
            result += std::to_string(target_id);
            result += "=";
            result += value;
            found = true;
          }
        } else {
          if (!result.empty()) result += ";";
          result += entry;
        }
      }
    }

    start = end + 1;
  }

  if (!found) {
    if (!result.empty()) result += ";";

    result += std::to_string(target_id);
    result += "=";
    result += value;
  }

  return result;
}

void Patches::LoadSpellEffectOverrides() {
  const auto* spell_mgr = Zeal::Game::get_spell_mgr();
  if (!spell_mgr) return;

  individualSpellEffects.clear();

  const std::string& overrides = setting_SpellEffectOverrides.get();

  if (overrides.empty()) return;

  size_t start = 0;

  while (start < overrides.size()) {
    size_t end = overrides.find(';', start);
    if (end == std::string::npos) end = overrides.size();

    std::string entry = overrides.substr(start, end - start);
    size_t separator = entry.find('=');

    if (separator != std::string::npos) {
      int target_id = 0;

      std::string target = entry.substr(0, separator);
      std::string value = entry.substr(separator + 1);

      if (Zeal::String::tryParse(target, &target_id, true) && target_id >= 0 && target_id < GAME_NUM_SPELLS) {
        auto target_spell = spell_mgr->Spells[target_id];

        if (target_spell) {
          if (_stricmp(value.c_str(), "classic") == 0) {
            individualSpellEffects[target_id] = {SpellEffectOverrideType::Classic, 0};
          } else if (_stricmp(value.c_str(), "default") == 0) {
            individualSpellEffects[target_id] = {SpellEffectOverrideType::ClientDefault, 0};
          } else if (value.size() > 8 && _strnicmp(value.c_str(), "replace:", 8) == 0) {
            int source_id = 0;

            if (Zeal::String::tryParse(value.substr(8), &source_id, true) && source_id >= 0 &&
                source_id < GAME_NUM_SPELLS) {
              auto source_spell = spell_mgr->Spells[source_id];

              if (source_spell) {
                individualSpellEffects[target_id] = {SpellEffectOverrideType::Replacement, 0, source_id};
              }
            }
          }
        }
      }
      start = end + 1;
    }

    SyncSpellEffects(setting_SpellEffectsClassic.get());
    SyncBuffEffects();
  }
}

bool Patches::HandleSpellEffectsCommand(const std::vector<std::string>& args) {
  if (args.size() == 2 && args[1] == "classic") {
    setting_SpellEffectsClassic.set(true);
    Zeal::Game::print_chat("Spell effects: Classic");
  } else if (args.size() == 2 && args[1] == "default") {
    setting_SpellEffectsClassic.set(false);
    Zeal::Game::print_chat("Spell effects: Default");
  } else if (args.size() == 3 && args[1] == "buff") {
    int mode = 0;

    if (args[2] == "classic") {
      mode = 1;
    } else if (args[2] == "default") {
      mode = 0;
    } else {
      Zeal::Game::print_chat("Error: buff effects mode must be 'classic' or 'default'");
      return true;
    }

    setting_BuffEffects.set(mode);

    if (!SyncBuffEffects()) {
      Zeal::Game::print_chat("Unable to modify buff effects (spell db unavailable)");
    } else {
      Zeal::Game::print_chat("Buff spell effects: %s", mode == 1 ? "Classic" : "Default");
    }

  } else if (args.size() == 2 && args[1] == "reset") {
    individualSpellEffects.clear();
    setting_SpellEffectOverrides.set("");

    if (!SyncSpellEffects(setting_SpellEffectsClassic.get()) || !SyncBuffEffects()) {
      Zeal::Game::print_chat("Unable to reset individual spell effects (spell db unavailable)");
    } else {
      Zeal::Game::print_chat("All individual spell effect overrides reset");
    }

  } else if (args.size() >= 3 && args[1] != "buff" && args[1] != "bard" && args[1] != "replace" &&
             (args.back() == "classic" || args.back() == "default")) {
    const std::string mode = args.back();

  // Reconstruct the spell name from all arguments between the command and the mode.
  std::string spell_name = args[1];
  for (size_t i = 2; i < args.size() - 1; ++i) {
    spell_name += " " + args[i];
  }

  int spell_id = -1;

  // A numeric ID is accepted as shorthand; otherwise resolve the spell by name.
  if (args.size() == 3 && Zeal::String::tryParse(args[1], &spell_id, true)) {
    if (spell_id < 0 || spell_id >= GAME_NUM_SPELLS) {
      Zeal::Game::print_chat("Error: invalid spell ID");
      return true;
    }
  } else {
    spell_id = FindSpellByName(spell_name);

    if (spell_id < 0) {
      Zeal::Game::print_chat("Error: could not find spell '%s'", spell_name.c_str());
      return true;
    }
  }

  const auto* spell_mgr = Zeal::Game::get_spell_mgr();
  if (!spell_mgr) {
    Zeal::Game::print_chat("Unable to modify spell effect (spell db unavailable)");
    return true;
  }

  auto spell = spell_mgr->Spells[spell_id];

  if (!spell) {
    Zeal::Game::print_chat("Error: spell %d does not exist", spell_id);
    return true;
  }

  if (mode == "classic") {
    setting_SpellEffectOverrides.set(SetSpellEffectOverride(setting_SpellEffectOverrides.get(), spell_id, "classic"));

    Zeal::Game::print_chat("Spell %d effect set to Classic", spell_id);
  } else {
    // "default" means explicitly use the client's original effect.
    setting_SpellEffectOverrides.set(SetSpellEffectOverride(setting_SpellEffectOverrides.get(), spell_id, "default"));

    Zeal::Game::print_chat("Spell %d effect set to Default", spell_id);
  }

  return true;

  } else if (args.size() >= 4 && args[1] == "replace") {
        const auto* spell_mgr = Zeal::Game::get_spell_mgr();
        if (!spell_mgr) {
          Zeal::Game::print_chat("Unable to modify spell effect (spell db unavailable)");
          return true;
        }

        // "replace <target> default" explicitly restores the target's client-default effect.
        if (args.size() >= 4 && args.back() == "default") {
          std::string target_text = args[2];

          for (size_t i = 3; i < args.size() - 1; ++i) {
            target_text += " " + args[i];
          }

          int target_id = -1;

          if (!Zeal::String::tryParse(target_text, &target_id, true)) {
            target_id = FindSpellByName(target_text);
          }

          if (target_id < 0 || target_id >= GAME_NUM_SPELLS) {
            Zeal::Game::print_chat("Error: could not find target spell");
            return true;
          }

          auto target_spell = spell_mgr->Spells[target_id];

          if (!target_spell) {
            Zeal::Game::print_chat("Error: spell %d does not exist", target_id);
            return true;
          }

          individualSpellEffects[target_id] = {SpellEffectOverrideType::ClientDefault, 0};

          setting_SpellEffectOverrides.set(
              SetSpellEffectOverride(setting_SpellEffectOverrides.get(), target_id, "default"));

          Zeal::Game::print_chat("Spell %d effect set to Default", target_id);

          return true;
        }

        // Resolve the target and source as either spell IDs or spell names.
        // Multi-word names are handled by trying each possible split point.
        int target_id = -1;
        int source_id = -1;

        for (size_t split = 3; split < args.size(); ++split) {
          std::string target_text = args[2];

          for (size_t i = 3; i < split; ++i) {
            target_text += " " + args[i];
          }

          std::string source_text = args[split];

          for (size_t i = split + 1; i < args.size(); ++i) {
            source_text += " " + args[i];
          }

          int candidate_target = -1;
          int candidate_source = -1;

          if (!Zeal::String::tryParse(target_text, &candidate_target, true)) {
            candidate_target = FindSpellByName(target_text);
          }

          if (!Zeal::String::tryParse(source_text, &candidate_source, true)) {
            candidate_source = FindSpellByName(source_text);
          }

          if (candidate_target >= 0 && candidate_target < GAME_NUM_SPELLS && candidate_source >= 0 &&
              candidate_source < GAME_NUM_SPELLS) {
            target_id = candidate_target;
            source_id = candidate_source;
            break;
          }
        }

        if (target_id < 0 || target_id >= GAME_NUM_SPELLS) {
          Zeal::Game::print_chat("Error: could not find target spell");
          return true;
        }

        if (source_id < 0 || source_id >= GAME_NUM_SPELLS) {
          Zeal::Game::print_chat("Error: could not find source spell");
          return true;
        }

        auto target_spell = spell_mgr->Spells[target_id];

        if (!target_spell) {
          Zeal::Game::print_chat("Error: spell %d does not exist", target_id);
          return true;
        }

        auto original_source = originalSpellEffects.find(source_id);

        if (original_source == originalSpellEffects.end() || !original_source->second.new_particle_effect) {
          Zeal::Game::print_chat("Error: source spell %d has no default particle effect", source_id);
          return true;
        }

        setting_SpellEffectOverrides.set(SetSpellEffectOverride(setting_SpellEffectOverrides.get(), target_id,
                                                                "replace:" + std::to_string(source_id)));

        Zeal::Game::print_chat("Spell %d effect replaced with spell %d effect", target_id, source_id);

        return true;

  } else if (args.size() == 2 && args[1] == "nosprites") {
    setting_DisableSprites.toggle();
    Zeal::Game::print_chat("No sprites: %s", setting_DisableSprites.get() ? "True" : "False");
  } else if (args.size() == 3 && args[1] == "bard") {
    int mode = 0;
    if (!Zeal::String::tryParse(args[2], &mode, true) || mode < 0 || mode > kNumBardEffects) {
      Zeal::Game::print_chat("Error: bard effects mode must be between 0 and %d", kNumBardEffects);
      return true;
    }
    setting_BardEffects.set(mode);
    Zeal::Game::print_chat("Bard effects mode: %d", setting_BardEffects.get());
    // This sync happens in the set above but call again to see if there was an error.
    if (!SyncBardEffects()) {
      Zeal::Game::print_chat("Unable to modify bard effects (spell db change?)");
    }
  } else {
    Zeal::Game::print_chat("Usage:");

    Zeal::Game::print_chat("  /spelleffects nosprites");
    Zeal::Game::print_chat("  /spelleffects bard <0, 1, 2, 3>");
    Zeal::Game::print_chat("  /spelleffects classic");
    Zeal::Game::print_chat("  /spelleffects default");
    Zeal::Game::print_chat("  /spelleffects buff classic");
    Zeal::Game::print_chat("  /spelleffects buff default");
    Zeal::Game::print_chat("  /spelleffects replace <target> <source>");
    Zeal::Game::print_chat("  /spelleffects replace <target> default");
    Zeal::Game::print_chat("  /spelleffects reset");

    Zeal::Game::print_chat(
        "nosprites: Disables the minor sprite enhancement of the 180 songs (out of 4000) that can cause a crash"
        " when `/showspelleffects on` is enabled");

    Zeal::Game::print_chat(
        "bard: Sets the effects mode (0 = default, 1, 2, 3 = alternatives) of 14 bard songs to optionally"
        " be more subtle (0 is invisible with /showspelleffects off)");

    Zeal::Game::print_chat(
        "classic: Changes all spell effects to the classic/old style");

    Zeal::Game::print_chat(
        "default: Reverts all spell effects to the modern/new style used by default by the client");

    Zeal::Game::print_chat("buff classic: Changes spell effects only for the effects commonly associated with the shielding"
        " buff category (e.g. Minor Shielding) to the classic style complete with level milestone particle effects"
        " at levels 1 (green), 24 (green + orange), and 39 (green + orange + blue)");

    Zeal::Game::print_chat("buff default: Reverts spell effects only for the effects commonly associated with the shielding buff "
        " category (e.g. Minor Shielding)to the default used by the client. Level milestones set to 24 regardless of level.");

    Zeal::Game::print_chat(
        "replace <target ID> <source ID>: Changes the spell effects for the Target Spell ID to use the effects of the Source Spell ID"
        " Example: `/spellfx replace 2116 732` would change the effects of Ancient: Destruction of Ice (2116) to use the effects of Ice Comet (732)");

    Zeal::Game::print_chat(
        "replace <target ID> default: Reverts the changes made for the target spell to the default effects used by the client");

    Zeal::Game::print_chat(
        "reset: Reverts all individual spell changes made with the `/spellfx replace` command to the default effects used by the client.");
  }
  return true;
}

// Returns the class / level / monk-epic dependent hand to hand delay in milliseconds.
static int get_hand_to_hand_delay_ms() { return Zeal::Game::get_hand_to_hand_delay() * 100; }

Patches::Patches() {
  const char sit_stand_patch[] = {(char)0xEB, (char)0x1A};
  mem::write(0x42d14d, sit_stand_patch);  // fix pet sit shortcut crash (makes default return of function the sit/stand
                                          // button not sure why its passing in 0)

  // disable client sided hp ticking
  // mem::set(0x4b9141, 0x90, 6);
  SetBrownSkeletons();

  // fix attack delay in DoPassageOfTime() for ItemTypeMartial (0x2d) by replacing unused type 0xd.
  mem::write<BYTE>(0x004c1d97 + 2, 0x2d);  // 004c1d97 80 f9 0d

  // fix hand2hand delay calculation in DoPassageOfTime() for monks and bst
  // replace a load from the fixed 3500 ms in skill dict with a call to our calculation.
  const int h2h_addr = 0x004c1dad;  // 10-byte long (7-byte + 3-byte opcodes) load to EAX sequence.
  unsigned char h2h_patch[10] = {0xe8, 0, 0, 0, 0, 0x90, 0x90, 0x90, 0x90, 0x90};  // call + nops.
  *reinterpret_cast<int *>(&h2h_patch[1]) = reinterpret_cast<int>(&get_hand_to_hand_delay_ms) - (h2h_addr + 5);
  mem::write(h2h_addr, h2h_patch);

  // disable client sided mana ticking
  mem::set(0x4C3F93, 0x90, 7);
  mem::set(0x4C7642, 0x90, 7);

  // disable client sided health ticking
  mem::set(0x4C28B5, 0x90, 9);
  mem::set(0x4C28EF, 0x90, 1);
  mem::set(0x4C28EF + 1, 0xE9, 1);
  mem::set(0x4C298B, 0x90, 2);
  mem::set(0x4C2991, 0x90, 5);
  mem::set(0x4C2BB4, 0x90, 9);

  mem::write<BYTE>(0x40f07a, 0);     // disable character select rotation by default
  mem::write<BYTE>(0x40f07d, 0xEB);  // uncheck rotate button defaultly

  // Replace "Spawning_Your_Characters01" with exact size string.
  const char zeal_patch[27] = "Patching_random_Zeal_crash";
  mem::write<char[27]>(0x005ff96c, zeal_patch);

  // the following does not work entirely needs more effort
  // mem::write<BYTE>(0x4A594B, 15); //load font sizes 1 to 14 (default is 6)
  // mem::write<BYTE>(0X4FDB6A, 15); //allow /chatfontsize to be larger than 5

  mem::write<BYTE>(0x4A14CF,
                   0xEB);  // don't print Your XML files are not compatible with current client files, certain windows
                           // may not perform correctly.  Use "/loadskin Default 1" to load the default game skin.

  ZealService::get_instance()->hooks->Add("GetZoneInfoFromNetwork", 0x53D026, GetZoneInfoFromNetwork, hook_type_detour);

  ZealService::get_instance()->hooks->Add("ProcessDeath", 0x00528E16, ProcessDeath, hook_type_detour);
  ZealService::get_instance()->hooks->Add("CanIBreathe", 0x004C0DAB, CanIBreathe, hook_type_detour);

  ZealService::get_instance()->commands_hook->Add(
      "/spelleffects", {"/spellfx"}, "Modify spell effects (prevent crashes, make less flashy, etc).",
      [this](std::vector<std::string>& args) { return HandleSpellEffectsCommand(args); });
}
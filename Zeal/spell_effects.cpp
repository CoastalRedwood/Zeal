#include "spell_effects.h"

#include <array>
#include <fstream>

#include "callbacks.h"
#include "commands.h"
#include "game_functions.h"
#include "hook_wrapper.h"
#include "memory.h"
#include "string_util.h"
#include "zeal.h"

// Internal linkage helper methods and constants.
namespace {

// Persistent file storage support.
static const char* DELIMITER = "^";

// Returns the full file path and name for storing the individual spell override effects.
std::filesystem::path get_spell_effects_overrides_filename() {
  return Zeal::Game::get_game_path() / std::filesystem::path("zeal_spell_effects_override.ini");
}

// Stores the spell id and name for re-mapping bard songs.
struct IdName {
  int id = 0;
  const char* name = "";
};

// Support three simple alternative bard effects for the ones that lack effects with nosprites active.
// Option 1: Use the pulsing sphere from Selo's (SpellAnim 98).
// Option 2: Use the more subtle floating notes animation from Nature's Melody (SpellAnim 45).
// Option 3: Use the even more subtle snowflake-like from Hymn/Cantata's (SpellAnim 69).
static constexpr std::array<IdName, 3> kBardReferenceSongs = {{
    {717, "Selo`s Accelerando"},
    {2050, "Nature's Melody"},
    {7, "Hymn of Restoration"},
}};

// Returns true if the spell_id matches a song in the bard reference songs list.
bool IsBardReferenceSongId(int spell_id) {
  for (const auto& song : kBardReferenceSongs) {
    if (song.id == spell_id) return true;
  }
  return false;
}

// These songs are by default all using old particle effects with SpellAffectIndex = 6.
static constexpr std::array<IdName, 14> kSongsToModify = {{
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
}};

// Returns true if the spell_id matches a song in the bard songs to modify list.
bool IsBardModifyEffectSpell(int spell_id) {
  for (const auto& song : kSongsToModify) {
    if (song.id == spell_id) return true;
  }

  return false;
}

// Returns true if the spell_id matches a song in either the bard reference or the bard songs to modify lists.
bool IsBardModifyEffectSpellOrReference(int spell_id) {
  return IsBardReferenceSongId(spell_id) || IsBardModifyEffectSpell(spell_id);
}

std::string NormalizeSpellName(const std::string& input_name) {
  std::string out;
  out.reserve(input_name.size());
  bool in_whitespace = false;
  for (unsigned char c : input_name) {
    // normalize apostrophes/backticks to backtick
    if (c == '\'' || c == '`') c = '`';
    if (std::isspace(c)) {
      if (!in_whitespace) {
        out.push_back(' ');
        in_whitespace = true;
      }
    } else {
      out.push_back(static_cast<char>(std::tolower(c)));
      in_whitespace = false;
    }
  }
  // trim
  if (!out.empty() && out.front() == ' ') out.erase(out.begin());
  if (!out.empty() && out.back() == ' ') out.pop_back();
  // remove surrounding quotes
  if (out.size() >= 2 && ((out.front() == '"' && out.back() == '"') || (out.front() == '\'' && out.back() == '\''))) {
    out = out.substr(1, out.size() - 2);
  }
  return out;
}

}  // namespace

SpellEffects::SpellEffects() {
  std::fill(original_spell_effects.begin(), original_spell_effects.end(), std::nullopt);
  std::fill(individual_spell_effects.begin(), individual_spell_effects.end(), std::nullopt);

  // First set the critical patch for the dpvs.dll crash. This can happen immediately.
  SyncDisableSprites();

  // The spell manager data isn't loaded yet, so the remaining initialization is deferred until entering a zone.
  ZealService::get_instance()->callbacks->AddGeneric([this]() { Initialize(); }, callback_type::EnterZone);

  ZealService::get_instance()->commands_hook->Add(
      "/spelleffects", {"/spellfx"}, "Modify spell effects (prevent crashes, make less flashy, etc).",
      [this](std::vector<std::string>& args) { return HandleSpellEffectsCommand(args); });
}

void SpellEffects::Initialize() {
  if (initialized) return;  // Only execute once.
  initialized = true;

  // Store the original spell effects in our restoration cache.
  CacheOriginalEffects();

  // Initialize the individual spell effect overrides.
  LoadSpellEffectOverrides();

  // And then apply all of the configured effects.
  SyncSpellEffects();
}

// Support an option to simply no-op the DoSpriteEffects() call to avoid the dvps.dll crash.
void SpellEffects::SyncDisableSprites() {
  const BYTE kOpcodeNop = 0x90;
  const int kDoSpriteEffectAddr = 0x0052cbb1;
  bool disable = setting_disable_sprites.get();
  bool currently_disabled = (*reinterpret_cast<BYTE*>(kDoSpriteEffectAddr) == kOpcodeNop);
  if (disable == currently_disabled) return;

  if (disable) {
    mem::set(kDoSpriteEffectAddr, kOpcodeNop, 5);  // No-op out the call.
  } else {
    const BYTE orig_code[5] = {0xe8, 0x89, 0xeb, 0xff, 0xff};  // Restore call.
    mem::write(kDoSpriteEffectAddr, orig_code);
  }
}

// Optional bard effects modes to reduce the super bright blue sparklies of constant bard song refreshes.
bool SpellEffects::SyncBardEffects() {
  // First check we have access to the spell list and the reference song is a match.
  const auto* spell_mgr = Zeal::Game::get_spell_mgr();
  if (!spell_mgr) return false;  // Something went wrong, do nothing.

  int mode = setting_bard_effects.get();
  DWORD new_effect = (DWORD) nullptr;  // Default uses the old particle effect.
  if (mode > 0 && mode <= kBardReferenceSongs.size()) {
    int index = mode - 1;
    const auto ref_spell = spell_mgr->Spells[kBardReferenceSongs[index].id];
    if (!ref_spell || !ref_spell->Name || !ref_spell->NewParticleEffect ||
        strcmp(ref_spell->Name, kBardReferenceSongs[index].name))
      return false;  // Something went wrong, do nothing.
    new_effect = ref_spell->NewParticleEffect;
  }

  // For every song, double-check it's a match (maybe the db gets updated) and apply the target
  // effect only if there's a match.
  bool no_errors = true;
  for (auto& song : kSongsToModify) {
    auto spell = spell_mgr->Spells[song.id];
    if (!spell || !spell->Name || spell->SpellAffectIndex != 6 || strcmp(spell->Name, song.name))
      no_errors = false;  // Skip this unrecognized song data.
    else
      spell->NewParticleEffect = new_effect;
  }
  return no_errors;
}

// Create a local copy of the original unmodified spell effects for later restoration during SyncSpellEffects() call.
void SpellEffects::CacheOriginalEffects() {
  std::fill(original_spell_effects.begin(), original_spell_effects.end(), std::nullopt);

  const auto* spell_mgr = Zeal::Game::get_spell_mgr();
  if (!spell_mgr) return;

  // Capture the client's default effects before applying any overrides.
  for (int spell_id = 0; spell_id < GAME_NUM_SPELLS; ++spell_id) {
    auto spell = spell_mgr->Spells[spell_id];
    if (spell)
      original_spell_effects[spell_id] = OriginalSpellEffect{spell->OldParticleEffect, spell->SpellAffectIndex,
                                                             spell->NewParticleEffect, spell->SpellAnim};
  }
}

// Loops through entire spell database updating to the current settings.
bool SpellEffects::SyncSpellEffects() {
  auto* spell_mgr = Zeal::Game::get_spell_mgr();
  if (!spell_mgr) return false;

  // Apply the legacy Bard preference first.
  SyncBardEffects();

  // Go through every spell synchronizing as required.
  for (int spell_id = 0; spell_id < GAME_NUM_SPELLS; ++spell_id) {
    auto spell = spell_mgr->Spells[spell_id];
    if (!spell) continue;
    if (IsBardModifyEffectSpellOrReference(spell_id) && !individual_spell_effects[spell_id].has_value())
      continue;  // Skip bard modifications unless overridden with specific individual requests.

    SyncSpellEffect(spell_mgr, spell_id);
  }
  return true;
}

// Synchronizes a single spell id to the current settings.
void SpellEffects::SyncSpellEffect(Zeal::GameStructures::SPELLMGR* spell_mgr, int spell_id) {
  static constexpr int kHealingSpellAffectIndex = 1;
  static constexpr int kBuffSpellAffectIndex = 2;

  auto spell = spell_mgr ? spell_mgr->Spells[spell_id] : nullptr;
  if (!spell || !original_spell_effects[spell_id].has_value()) return;

  // First restore the spell back to the global settings before possibly applying individual updates.
  // Healing and buff spells have their own optional overrides to allow classic effects. -1 = use global,
  // 0 = use default, 1 = use classic (old effects).
  bool classic = setting_spell_effects_classic.get();  // Global use classic setting.
  if (spell->SpellAffectIndex == kHealingSpellAffectIndex && (setting_healing_effects.get() >= 0))
    classic = (setting_healing_effects.get() != 0);
  else if (spell->SpellAffectIndex == kBuffSpellAffectIndex && (setting_buff_effects.get() >= 0))
    classic = (setting_buff_effects.get() != 0);

  const auto& original_effect = *original_spell_effects[spell_id];
  spell->OldParticleEffect = original_effect.OldParticleEffect;
  spell->SpellAffectIndex = original_effect.SpellAffectIndex;
  spell->NewParticleEffect = !classic ? original_effect.NewParticleEffect : 0;
  spell->SpellAnim = original_effect.SpellAnim;

  // Apply individual overrides (if present) to replace the global setting.
  if (individual_spell_effects[spell_id].has_value()) {
    auto& spell_override = *individual_spell_effects[spell_id];
    switch (spell_override.type) {
      case SpellEffectOverrideType::Classic:
        spell->NewParticleEffect = 0;  // Disable new particle effects to force use of old classic (if any).
        break;

      case SpellEffectOverrideType::ClientDefault:
        if (original_spell_effects[spell_id].has_value())
          spell->NewParticleEffect = original_spell_effects[spell_id]->NewParticleEffect;  // Restore from cache.
        break;

      case SpellEffectOverrideType::Replacement: {
        int src_spell_id = spell_override.source_spell_id;
        if (src_spell_id >= 0 && src_spell_id < GAME_NUM_SPELLS && original_spell_effects[src_spell_id].has_value()) {
          const auto& override_effect = *original_spell_effects[src_spell_id];
          if (spell_override.use_old_effect && override_effect.OldParticleEffect) {
            spell->NewParticleEffect = 0;
            spell->OldParticleEffect = override_effect.OldParticleEffect;
            spell->SpellAnim = override_effect.SpellAnim;
            spell->SpellAffectIndex = override_effect.SpellAffectIndex;
          } else if (override_effect.NewParticleEffect) {
            spell->NewParticleEffect = override_effect.NewParticleEffect;
          }
        }
      } break;
      default:
        break;  // Shouldn't happen so just don't apply any unrecognized type.
    }
  }
}

// Save spell effect overrides to a persistent storage file.
void SpellEffects::SaveSpellEffectOverrides() {
  auto file_path = get_spell_effects_overrides_filename();
  std::ofstream output_file(file_path);
  if (!output_file.is_open()) {
    Zeal::Game::print_chat("Error: Failed saving spell effect overrides to %s", file_path.string().c_str());
    return;
  }

  int count = 0;
  for (int spell_id = 0; spell_id < GAME_NUM_SPELLS; ++spell_id) {
    if (individual_spell_effects[spell_id].has_value()) {
      count++;
      const SpellEffectOverride& value = *individual_spell_effects[spell_id];
      output_file << spell_id << DELIMITER << static_cast<int>(value.type) << DELIMITER << value.source_spell_id
                  << DELIMITER << value.use_old_effect << "\n";
    }
  }

  Zeal::Game::print_chat("Saved %d spell effect overrides to %s", count, file_path.string().c_str());
}

// Returns true if it successfully parses a single saved line and populates the individual spell effect override.
bool SpellEffects::ParseSpellEffectOverrideLine(const std::string& line) {
  auto fields = Zeal::String::split_text(line, DELIMITER);
  if (fields.size() != 4) return false;

  int spell_id = std::stoi(fields[0]);
  if ((spell_id < 0) || (spell_id > GAME_NUM_SPELLS)) return false;

  SpellEffectOverrideType override_type = static_cast<SpellEffectOverrideType>(std::stoi(fields[1]));
  switch (override_type) {
    case SpellEffectOverrideType::Classic:
    case SpellEffectOverrideType::ClientDefault:
    case SpellEffectOverrideType::Replacement:
      break;  // Valid so continue.
    default:
      return false;  // Unrecognized cast to enum type, so bail out.
  }

  int source_spell_id = -1;
  bool use_old_effect = false;
  if (override_type == SpellEffectOverrideType::Replacement) {
    source_spell_id = std::stoi(fields[2]);
    if ((source_spell_id < 0) || (source_spell_id >= GAME_NUM_SPELLS) ||
        !original_spell_effects[source_spell_id].has_value())
      return false;

    use_old_effect = std::stoi(fields[3]);
  }

  individual_spell_effects[spell_id] = SpellEffectOverride{override_type, source_spell_id, use_old_effect};
  return true;
}

// Retrieves spell effects from the persistent storage file.
void SpellEffects::LoadSpellEffectOverrides() {
  std::fill(individual_spell_effects.begin(), individual_spell_effects.end(), std::nullopt);

  std::filesystem::path file_path = get_spell_effects_overrides_filename();
  if (file_path.empty() || !std::filesystem::exists(file_path)) return;  // Nothing to load so bail out.

  std::ifstream input_file(file_path);
  if (!input_file.is_open()) return;  // Still just bail out silently if any random opening error.

  std::string line;
  int error_count = 0;
  while (std::getline(input_file, line)) {
    if (!ParseSpellEffectOverrideLine(line)) {
      error_count++;
      if (error_count == 1)
        Zeal::Game::print_chat(USERCOLOR_SHOUT, "Zeal error parsing spell effects override line: %s", line.c_str());
    }
  }
  if (error_count)
    Zeal::Game::print_chat(USERCOLOR_SHOUT, "Zeal failed to fully parse %d spell effect overrides lines", error_count);
}

bool SpellEffects::HandleSpellEffectsCommand(const std::vector<std::string>& args) {
  if (args.size() == 2 && args[1] == "nosprites") {
    setting_disable_sprites.toggle();
    Zeal::Game::print_chat("No sprites: %s",
                           setting_disable_sprites.get() ? "True (disabled)" : "False(enabled, dpvs.dll may crash)");
    SyncDisableSprites();
    return true;
  }

  if (args.size() == 2 && args[1] == "list") {
    HandleListCommand();
    return true;
  }

  if (args.size() == 3 && args[1] == "bard") {
    int mode = 0;
    if (!Zeal::String::tryParse(args[2], &mode, true) || mode < 0 || mode > kBardReferenceSongs.size()) {
      Zeal::Game::print_chat("Error: bard effects mode must be between 0 and %d", kBardReferenceSongs.size());
      return true;
    }
    setting_bard_effects.set(mode);
    Zeal::Game::print_chat("Bard effects mode: %d", setting_bard_effects.get());
    if (!SyncBardEffects()) {
      Zeal::Game::print_chat("Unable to modify bard effects (spell db change?)");
    }
    return true;
  }

  if (args.size() == 2 && args[1] == "classic") {
    setting_spell_effects_classic.set(true);
    Zeal::Game::print_chat("Global spell effects: Classic");
    SyncSpellEffects();
    return true;
  }

  if (args.size() == 2 && args[1] == "default") {
    setting_spell_effects_classic.set(false);
    Zeal::Game::print_chat("Global spell effects: Default");
    SyncSpellEffects();
    return true;
  }

  if (args.size() == 3 && args[1] == "buff") {
    if (args[2] == "classic") {
      setting_buff_effects.set(1);
      Zeal::Game::print_chat("Buff spell effects: Classic");
    } else if (args[2] == "default") {
      setting_buff_effects.set(0);
      Zeal::Game::print_chat("Buff spell effects: Default");
    } else if (args[2] == "global") {
      setting_buff_effects.set(-1);
      Zeal::Game::print_chat("Buff spell effects: Use global");
    } else {
      PrintUsage();
    }
    SyncSpellEffects();
    return true;
  }

  if (args.size() == 3 && args[1] == "heal") {
    if (args[2] == "classic") {
      setting_healing_effects.set(1);
      Zeal::Game::print_chat("Heal spell effects: Classic");
    } else if (args[2] == "default") {
      setting_healing_effects.set(0);
      Zeal::Game::print_chat("Heal spell effects: Default");
    } else if (args[2] == "global") {
      setting_healing_effects.set(-1);
      Zeal::Game::print_chat("Heal spell effects: Use global");
    } else {
      PrintUsage();
    }
    SyncSpellEffects();
    return true;
  }

  if (args.size() > 2 && args[1] == "lookup") {
    HandleSpellLookupCommand(args);
    return true;
  }

  if (args.size() > 2 && args[1] == "override") {
    if (HandleSpellOverrideCommand(args)) {
      SaveSpellEffectOverrides();
      SyncSpellEffects();
      return true;
    }
  }

  PrintUsage();
  return true;
}

void SpellEffects::HandleListCommand() const {
  Zeal::Game::print_chat("/spelleffect settings:");
  Zeal::Game::print_chat("No sprites: %s", setting_disable_sprites.get() ? "True (crash source disabled)"
                                                                         : "False (may crash with /showspelleffects");
  Zeal::Game::print_chat("Bard spells: %d (0 = default invisible, 1-3 = visible)", setting_bard_effects.get());
  Zeal::Game::print_chat("Global spells: %s", setting_spell_effects_classic.get() ? "Use classic" : "Use default");
  Zeal::Game::print_chat("Buff spells: %s", (setting_buff_effects.get() == 1)   ? "Use classic"
                                            : (setting_buff_effects.get() == 0) ? "Use default"
                                                                                : "Use global");
  Zeal::Game::print_chat("Heal spells: %s", (setting_healing_effects.get() == 1)   ? "Use classic"
                                            : (setting_healing_effects.get() == 0) ? "Use default"
                                                                                   : "Use global");

  // Simple list dump to verify loading / reset worked.
  for (int spell_id = 0; spell_id < GAME_NUM_SPELLS; ++spell_id) {
    if (individual_spell_effects[spell_id].has_value()) {
      const auto& value = *individual_spell_effects[spell_id];
      switch (value.type) {
        case SpellEffectOverrideType::ClientDefault:
          Zeal::Game::print_chat("[%d]: Use client default spell effects", spell_id);
          break;
        case SpellEffectOverrideType::Classic:
          Zeal::Game::print_chat("[%d]: Use classic spell effects", spell_id);
          break;
        case SpellEffectOverrideType::Replacement:
          Zeal::Game::print_chat("[%d]: Copy %s effects from spell id: %d", spell_id,
                                 value.use_old_effect ? "classic" : "default", value.source_spell_id);
          break;
      }
    }
  }
}

// Prints the spell ids for all spells that start with the args string.
bool SpellEffects::HandleSpellLookupCommand(const std::vector<std::string>& args) {
  if (args.size() < 2) return false;

  // Recombine parsed spell name.
  std::string search_name = args[2];
  for (size_t i = 3; i < args.size(); ++i) search_name += " " + args[i];

  if (search_name.size() < 3) {
    Zeal::Game::print_chat("Spell name lookup search requires at least the first three letters");
    return false;
  }

  const auto* spell_mgr = Zeal::Game::get_spell_mgr();
  if (!spell_mgr) return false;

  // Normalize the names and check for matches (handles extra whitespace, quotes).
  Zeal::Game::print_chat("[Id] lookup for \"%s\":", search_name.c_str());
  std::string norm_name = NormalizeSpellName(search_name);
  for (int spell_id = 0; spell_id < GAME_NUM_SPELLS; ++spell_id) {
    const auto spell = spell_mgr->Spells[spell_id];
    if (!spell || !spell->Name) continue;
    std::string cand = NormalizeSpellName(spell->Name);
    if (cand.starts_with(norm_name)) Zeal::Game::print_chat("[%d]: %s", spell_id, cand.c_str());
  }

  return true;
}

// Handles individual spell override commands.
bool SpellEffects::HandleSpellOverrideCommand(const std::vector<std::string>& args) {
  // Reset overrides: wipe the in-memory vector and erase the persistent file storage.
  if (args.size() == 3 && args[2] == "reset") {
    std::fill(individual_spell_effects.begin(), individual_spell_effects.end(), std::nullopt);
    Zeal::Game::print_chat("All individual spell effect overrides reset");
    return true;
  }

  // Expect commands to be /spelleffects override <target id> <selection>
  int target_id = -1;
  if (args.size() != 4 || !Zeal::String::tryParse(args[2], &target_id, true) || (target_id < 0) ||
      (target_id >= GAME_NUM_SPELLS))
    return false;

  // Sanity check target spell id.
  const auto* spell_mgr = Zeal::Game::get_spell_mgr();
  if (!spell_mgr) return false;
  const auto spell = spell_mgr->Spells[target_id];
  if (!spell || !spell->Name) {
    Zeal::Game::print_chat("Unrecognized spell id: %d", target_id);
    return false;
  }

  if (args[3] == "reset") {
    Zeal::Game::print_chat("Clearing override of spell [%d]: %s", target_id, spell->Name);
    individual_spell_effects[target_id] = std::nullopt;
    return true;
  }

  if (args[3] == "classic") {
    Zeal::Game::print_chat("Setting classic mode override for spell [%d]: %s", target_id, spell->Name);
    individual_spell_effects[target_id] = SpellEffectOverride{SpellEffectOverrideType::Classic, -1, false};
    return true;
  }

  if (args[3] == "default") {
    Zeal::Game::print_chat("Setting default mode override of spell [%d]: %s", target_id, spell->Name);
    individual_spell_effects[target_id] = SpellEffectOverride{SpellEffectOverrideType::ClientDefault, -1, false};
    return true;
  }

  // Last parameter must be the copy spell id.
  int copy_id = -1;
  if (!Zeal::String::tryParse(args[3], &copy_id, true) || (copy_id < 0) || (copy_id >= GAME_NUM_SPELLS)) return false;
  const auto copy_spell = spell_mgr->Spells[copy_id];
  if (!copy_spell || !copy_spell->Name) {
    Zeal::Game::print_chat("Unrecognized copy spell id: %d", copy_id);
    return false;
  }

  // Copy spell overrides use the current classic or default modes.
  bool use_classic = setting_spell_effects_classic.get();
  Zeal::Game::print_chat("Setting override of spell [%d]: %s to copy from [%d]: %s in %s mode", target_id, spell->Name,
                         copy_id, copy_spell->Name, use_classic ? "classic" : "default");
  individual_spell_effects[target_id] = SpellEffectOverride{SpellEffectOverrideType::Replacement, copy_id, use_classic};

  return true;
}

void SpellEffects::PrintUsage() const {
  Zeal::Game::print_chat("Usage:");

  Zeal::Game::print_chat("/spelleffects nosprites");
  Zeal::Game::print_chat("/spelleffects list (print current settings)");
  Zeal::Game::print_chat("/spelleffects bard <0,1,2,3>");
  Zeal::Game::print_chat("/spelleffects <classic | default> (global settings)");
  Zeal::Game::print_chat("/spelleffects buff <classic | default | global> (buff spells)");
  Zeal::Game::print_chat("/spelleffects heal <classic | default | global> (healing spells)");
  Zeal::Game::print_chat("/spelleffects lookup <spellname> (returns spell id for spellname)");
  Zeal::Game::print_chat("/spelleffects override <target spell id> <classic | default | copy spell id | reset>");
  Zeal::Game::print_chat("/spelleffects override reset (resets all individual spell overrides");

  Zeal::Game::print_chat(
      "nosprites: Disables the minor sprite enhancement of the 180 songs (out of 4000) that can cause a crash"
      " when `/showspelleffects on` is enabled");

  Zeal::Game::print_chat(
      "bard: Sets the effects mode (0 = default, 1, 2, 3 = alternatives) of 14 bard songs to optionally"
      " be more subtle (0 is invisible with /showspelleffects off)");

  Zeal::Game::print_chat(
      "global: most spell effects unless overridden (bard, buff, and heal have optional group overrides)");
  Zeal::Game::print_chat("classic: use the old effects, default: use the new default style");
}
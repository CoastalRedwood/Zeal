#include "item_display.h"

#include <algorithm>
#include <array>
#include <format>
#include <fstream>
#include <regex>

#include "callbacks.h"
#include "game_addresses.h"
#include "game_functions.h"
#include "hook_wrapper.h"
#include "string_util.h"
#include "ui_skin.h"
#include "zeal.h"
#undef max
#undef min

void ItemDisplay::InitUI() {
  if (!windows.empty()) Zeal::Game::print_chat("Warning: InitUI and CleanUI out of sync in ItemDisplay");

  windows.clear();
  for (int i = 0; i < max_item_displays; i++) {
    Zeal::GameUI::ItemDisplayWnd *new_wnd = Zeal::GameUI::ItemDisplayWnd::Create(0);
    if (!new_wnd) {
      Zeal::Game::print_chat("Error: Memory allocation failed in ItemDisplay");
      break;
    }
    windows.push_back(new_wnd);
    // new_wnd->SetupCustomVTable();  // Re-enable this and the deleter if custom vtables are required.

    // For an unclear reason the constructor above is not configuring the window relationships
    // correctly, which causes the ItemDescription window to pop above the IconBtn when clicked.
    new_wnd->HasChildren = true;  // Both flags seem to need to be set even if only one is true.
    new_wnd->HasSiblings = true;
    new_wnd->FirstChildWnd = new_wnd->ItemDescription;
    new_wnd->ItemDescription->NextSiblingWnd = new_wnd->IconBtn;
    new_wnd->ItemDescription->HasChildren = true;
    new_wnd->ItemDescription->HasSiblings = true;
    new_wnd->IconBtn->HasChildren = true;
    new_wnd->IconBtn->HasSiblings = true;

    // Set up independent ini settings for these new windows and reload using the new name.
    new_wnd->EnableINIStorage = 0x19;  // Magic value to use the INIStorageName.
    new_wnd->INIStorageName.Set(std::format("ZealItemDisplay{}", i));
    auto vtable = static_cast<Zeal::GameUI::ItemDisplayVTable *>(new_wnd->vtbl);
    auto load_ini =
        reinterpret_cast<void(__fastcall *)(Zeal::GameUI::ItemDisplayWnd *, int unused)>(vtable->LoadIniInfo);
    load_ini(new_wnd, 0);
  }
}

// Returns a window to display the item or spell (use nullptr) in.
Zeal::GameUI::ItemDisplayWnd *ItemDisplay::get_available_window(Zeal::GameStructures::GAMEITEMINFOBASE *item) {
  if (item) {
    /*check if the item is already being displayed*/
    for (auto &w : windows) {
      if (w->IsVisible && w->ItemValid && w->Item.ID == item->ID) return w;
    }
  }

  for (auto &w : windows) {
    if (!w->IsVisible) return w;
  }
  return windows.back();
}

bool ItemDisplay::close_latest_window() {
  // We don't keep track of the latest, so just close starting from the end of the list.
  for (auto rit = windows.rbegin(); rit != windows.rend(); ++rit) {
    Zeal::GameUI::ItemDisplayWnd *wnd = *rit;
    if (wnd->IsVisible) {
      wnd->Deactivate();
      return true;
    }
  }
  return false;
}

static bool IsFixedLevelSpell(WORD spell_id) { return (spell_id >= 1252 && spell_id <= 1266) || spell_id == 3999; }

static int GetEffectiveCastingLevelBonus() {
  if (!Zeal::Game::get_char_info()) return 0;
  // Note: Currently, Jam Fest gives bonus to all spells, mirring here
  int jam_fest_rank = Zeal::Game::get_char_info()->GetAbility(94);
  return jam_fest_rank > 0 ? (jam_fest_rank * 2 - 1) : 0;
}

static int GetCastingLevel(Zeal::GameStructures::GAMECHARINFO *char_info, Zeal::GameStructures::SPELL *spell,
                           Zeal::GameStructures::GAMEITEMINFO *item = nullptr) {
  if (item && item->Type == 0 && item->Common.CastingLevel > 0) {
    if (item->Common.Skill == Zeal::GameEnums::ItemTypePotion) {
      // Only potions with specific spell IDs use the item's casting level, the rest use caster's level
      if (IsFixedLevelSpell(spell->ID)) {
        return item->Common.CastingLevel;
      }
    }
    if (item->Common.IsStackableEx == Zeal::GameEnums::ItemEffectWorn) {
      return item->Common.CastingLevel;
    }
  }
  return char_info->Level + GetEffectiveCastingLevelBonus();
}

static short CalculateEffectAtLevel(std::string &value_str, Zeal::GameStructures::SPELL *spell, BYTE casting_level,
                                    BYTE effect_index, int bardmodifier = 10) {
  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();
  if (!char_info) return 0;
  BYTE effect = spell->Attrib[effect_index];
  short value = spell->CalculateAffectChange(char_info, casting_level, effect_index);
  if (bardmodifier > 10 && spell->IsInstrumentModdableSpellEffect(effect)) {
    value = value * bardmodifier / 10;
  }
  switch (effect) {
    case 1:  // Armor Class
      if (value >= 0 && char_info->is_cloth_caster())
        value /= 3;  // Buff on cloth caster, show cloth AC amount
      else
        value /= 4;  // Debuff, show regular AC amount
      value_str = std::format("{}", std::abs(value));
      break;
    case 3:  // Movement Speed
      value_str = std::format("{}%", std::abs(value));
      break;
    case 11:  // Attack Speed
      if (value > 100)
        value -= 100;  // Increase by X%
      else if (value > 0)
        value = 100 - value;  // Decrease by X%
      value_str = std::format("{}%", std::abs(value));
      break;
    case 21:  // Stun
      value_str = std::format("({} seconds)", value * 1.0f / 1000.0f);
      break;
    case 118:  // Amplification
      value *= 10;
      value_str = std::format("{}%", value);
      break;
    case 119:  // Haste V3
      value_str = std::format("{}%", value);
      break;
    default:
      value_str = std::format("{}", std::abs(value));
      break;
  }
  return value;
}

static int GetMaxBardModifier() {
  if (!Zeal::Game::get_char_info()) return 36;
  return 36 + Zeal::Game::get_char_info()->GetAbility(192);  // Ayonae's Tutelage
}

static std::string CopperToAll(unsigned long long copper) {
  unsigned long long platinum = copper / 1000;
  unsigned long long gold = (copper % 1000) / 100;
  unsigned long long silver = (copper % 100) / 10;
  unsigned long long remainingCopper = copper % 10;

  std::ostringstream result;
  result << platinum << "p " << gold << "g " << silver << "s " << remainingCopper << "c";

  return result.str();
}

static std::string GetSpellClassLevels(const Zeal::GameStructures::_GAMEITEMINFO &item, const std::string &original) {
  if (item.Type != 0 || item.Common.Skill != 0x14) return original;

  const auto *spell_mgr = Zeal::Game::get_spell_mgr();
  int spell_id = item.Common.SpellIdEx;
  if (spell_id < 1 || spell_id >= GAME_NUM_SPELLS || !spell_mgr) return original;

  const auto *spell = spell_mgr->Spells[spell_id];
  if (!spell) return original;

  // Cycle through the classes adding levels.
  std::string result("Class: ");
  for (int i = Zeal::GameEnums::ClassTypes::Warrior; i <= Zeal::GameEnums::ClassTypes::Beastlord; ++i) {
    unsigned int class_bit = 1 << (i - 1);
    if ((item.Common.Classes & class_bit) == 0) continue;
    int class_level = spell->ClassLevel[i];
    auto class_name = Zeal::Game::class_name_short(i);
    std::transform(class_name.begin(), class_name.end(), class_name.begin(), ::toupper);
    result += std::format(" {} ({})", class_name, class_level);
  }

  return result;
}

static void ApplySpellInfo(Zeal::GameStructures::_GAMEITEMINFO *item, std::string &s) {
  if (item->Type == 0 && item->Common.Skill != Zeal::GameEnums::ItemTypeSpell && item->Common.IsStackable > 1 &&
      item->Common.IsStackable < 5) {
    // Items with Clicky/Proc/Worn Spells
    if (item->Common.IsStackableEx >= Zeal::GameEnums::ItemEffectCombatProc &&
        item->Common.IsStackableEx <= Zeal::GameEnums::ItemEffectCanEquipClick && item->Common.SpellIdEx > 0 &&
        item->Common.SpellIdEx < 4000) {
      int effect_level_req = Zeal::Game::get_effect_required_level(item);

      if (item->Common.IsStackableEx == Zeal::GameEnums::ItemEffectCombatProc &&
          s.find(" (Combat)") != std::string::npos) {
        // Combat Effect: Spell Name (Level 10)
        s = std::string("Combat ") + s.substr(0, s.find(" (Combat)"));
        if (effect_level_req > 1) {
          s += std::format(" (Level {})", effect_level_req);
        }
      } else if (item->Common.IsStackableEx == Zeal::GameEnums::ItemEffectWorn &&
                 s.find("Effect: Haste (Worn)") != std::string::npos) {
        s = std::format("Effect: Haste: +{}%", item->Common.CastingLevel + 1);
      } else if (item->Common.IsStackableEx == Zeal::GameEnums::ItemEffectClick &&
                 s.find("Casting Time:") != std::string::npos) {
        if (effect_level_req > 1) {
          std::string effect_level_str = std::string("Level: ") + std::to_string(effect_level_req) + std::string(". ");
          s = s.insert(s.find("Casting Time:"), effect_level_str);
        }
      } else if (item->Common.IsStackableEx == Zeal::GameEnums::ItemEffectMustEquipClick &&
                 s.find("(Must Equip") != std::string::npos) {
        if (effect_level_req > 1) {
          std::string effect_level_str = std::string("Level: ") + std::to_string(effect_level_req) + std::string(". ");
          s = s.insert(s.find("(Must Equip") + 1, effect_level_str);
        }
      } else if (item->Common.IsStackableEx == Zeal::GameEnums::ItemEffectCanEquipClick &&
                 s.find("Casting Time:") != std::string::npos) {
        if (effect_level_req > 1) {
          std::string effect_level_str = std::string("Level: ") + std::to_string(effect_level_req) + std::string(". ");
          s = s.insert(s.find("Casting Time:"), effect_level_str);
        }
        s = s.insert(s.find("Casting Time:"), "Can Equip. ");
      }
    }
  }
  if (item->Type == 0 && item->Common.Skill == 0x14 && s.starts_with("Class: ")) {
    s = GetSpellClassLevels(*item, s);
  }
}

static void ApplyInstrumentModifiers(Zeal::GameStructures::_GAMEITEMINFO *item, std::string &s) {
  if (item->Type == 0 && item->Common.BardType != 0 && item->Common.BardValue > 10 && s.ends_with("Instruments") ||
      s.ends_with("Instrument Types")) {
    int modifier = (item->Common.BardValue - 10) * 10;  // 18 = +80%, 24 = +140%
    // s += (std::string(": ") + std::to_string(modifier) + std::string("%"));
    s += std::format(": {}%", modifier);
  }
}

static void ApplyWeaponRatio(Zeal::GameStructures::_GAMEITEMINFO *item, std::string &s) {
  if (item->Common.Damage <= 0 || item->Common.AttackDelay <= 0) return;

  // First handle bane lines to avoid collisions with the generic DMG search.
  if (s.starts_with("Bane DMG: ")) {
    float ratio = (item->Common.Damage + item->Common.BaneDmgAmount) / (float)(item->Common.AttackDelay);
    s += std::format(" ({:.3f})", ratio);
    return;
  }

  size_t pos = s.find("DMG: ");
  if (pos != std::string::npos) {
    pos = s.find(" ", pos + strlen("DMG: "));  // Find next space (if any) after dmg.
    pos = (pos == std::string::npos) ? s.length() : pos;
    float ratio = (float)item->Common.Damage / (float)item->Common.AttackDelay;
    s = s.insert(pos, std::format(" ({:.3f})", ratio));
  }
}

// Cache of all items displayed so we can access them later if needed.
void ItemDisplay::add_to_cache(const Zeal::GameStructures::GAMEITEMINFO *item) {
  // GAMEITEMINFO is a union of different structs with type 0 (common) the largest, so
  // we only cache the common type to ensure the copy below is valid.
  if (item && item->Type == 0) item_cache[item->ID] = *item;
}

const Zeal::GameStructures::GAMEITEMINFO *ItemDisplay::get_cached_item(int item_id) const {
  auto it = item_cache.find(item_id);
  if (it != item_cache.end()) return &(it->second);
  return nullptr;
}

static void append_effect_description(std::string &line, Zeal::GameStructures::SPELL *spell, BYTE caster_level,
                                      BYTE effect_index) {
  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();
  if (!char_info) return;

  BYTE min_level;
  BYTE max_level;
  const BYTE highest_casting_level = 65 + GetEffectiveCastingLevelBonus();

  if (IsFixedLevelSpell(spell->ID)) {
    min_level = caster_level;
    max_level = caster_level;
  } else {
    // Find the earliest castable level for the spell (if it has one)
    min_level = 0xFF;
    for (int i = Zeal::GameEnums::ClassTypes::Warrior; i <= Zeal::GameEnums::ClassTypes::Beastlord; i++) {
      if (spell->ClassLevel[i] > 0 && spell->ClassLevel[i] < min_level) min_level = spell->ClassLevel[i];
    }
    if (min_level == 0xFF) min_level = 1;

    // Determine when spell starts scaling
    short value = spell->CalculateAffectChange(char_info, min_level, effect_index);
    for (int lvl = min_level + 1; lvl <= highest_casting_level; lvl++) {
      if (spell->CalculateAffectChange(char_info, lvl, effect_index) == value)
        min_level = lvl;
      else
        break;
    }

    // Determine when spell stops scaling
    value = spell->CalculateAffectChange(char_info, min_level, effect_index);
    max_level = min_level;
    for (int lvl = min_level + 1; lvl <= highest_casting_level; lvl++) {
      short test_value = spell->CalculateAffectChange(char_info, lvl, effect_index);
      if (test_value != value) {
        max_level = lvl;
        value = test_value;
      }
    }
  }

  std::string min_effect_str;
  CalculateEffectAtLevel(min_effect_str, spell, min_level, effect_index);

  if (max_level == min_level) {
    line += min_effect_str;
  } else {
    std::string max_effect_str;
    CalculateEffectAtLevel(max_effect_str, spell, max_level, effect_index);
    line += std::format("{} (L{}) to {} (L{})", min_effect_str.c_str(), min_level, max_effect_str.c_str(), max_level);
  }
}

// Fixes missing effects
static void fix_effect_line(std::string &line, Zeal::GameStructures::SPELL *spell, BYTE caster_level,
                            BYTE effect_index) {
  BYTE effect = spell->Attrib[effect_index];
  BYTE display_index = effect_index + 1;
  switch (effect) {
    case 0:                    // CurrentHP
      if (spell->ID == 742) {  // Fix - Denon's Desperate Dirge missing accurate base description
        line = std::format("  {}: Decrease hitpoints by ", display_index);
        append_effect_description(line, spell, caster_level, effect_index);
      }
      break;
    case 1:  // Armor class (clean up inaccuracies)
      if (spell->Base[effect_index] < 0) {
        line = std::format("  {}: Decrease AC by ", display_index);
      } else {
        if (Zeal::Game::get_char_info() && Zeal::Game::get_char_info()->is_cloth_caster()) {
          line = std::format("  {}: Increase AC for Cloth Casters by ", display_index);
        } else {
          line = std::format("  {}: Increase AC by ", display_index);
        }
      }
      append_effect_description(line, spell, caster_level, effect_index);
      break;
    case 11:  // Attack Speed (wrong on some spells)
      if (spell->Base[effect_index] < 100 || (spell->Max[effect_index] > 0 && spell->Max[effect_index] < 100))
        line = std::format("  {}: Decrease Attack Speed by ", display_index);
      else
        line = std::format("  {}: Increase Attack Speed by ", display_index);
      append_effect_description(line, spell, caster_level, effect_index);
      break;
    case 21:  // Stun
      // SE_Stun: There is a client / server mismatch where the server uses the max field to set the max level
      // but the client treats the max field as an upper value limit in the calc, so don't try to calculate.
      break;
    case 78:  // Absorb Magic
      line = std::format("  {}: Increase Absorb Magic Damage by ", display_index);
      append_effect_description(line, spell, caster_level, effect_index);
      break;
    case 115:  // Song of Sustenance
      line = std::format("  {}: Food/Water", display_index);
      break;
    case 117:  // MagicWeapon
      line = std::format("  {}: Make Weapons Magical", display_index);
      break;
    case 118:  // Amplification
      line = std::format("  {}: Increase Singing Modifier by ", display_index);
      append_effect_description(line, spell, caster_level, effect_index);
      break;
    case 119:  // Haste V3
      line = std::format("  {}: Increase Haste v3 (Overhaste) by ", display_index);
      append_effect_description(line, spell, caster_level, effect_index);
      break;
  }
}

// Help the mathematically challenged by calculating the spell effect value and
// the durations at the casting level and appending it to the string.
static void add_value_at_level(std::string &line, Zeal::GameStructures::SPELL *spell, int level, int bardmodifier = 10,
                               bool is_buff = false) {
  if (!spell || level <= 0) return;
  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();
  if (!char_info) return;

  bool is_effect = line.starts_with("  ");
  bool is_duration = line.starts_with("Duration:");
  bool is_detrimental = spell->SpellType == 0;
  if (!is_effect && !is_duration) return;

  if (is_duration) {
    short ticks = spell->CalculateSpellDuration(char_info, level);
    if (ticks > 0 && !is_buff) line += std::format("<c \"#c0c000\"> [{} ticks @ L{}]</c>", ticks, level);  // In yellow.
    return;
  }

  std::smatch effect_index_match;
  static const char *effect_index_pattern_str = "(\\d+):.*";
  static const std::regex effect_index_pattern(effect_index_pattern_str);
  if (std::regex_search(line, effect_index_match, effect_index_pattern)) {
    int effect_index = std::stoi(effect_index_match.str(1)) - 1;
    if (effect_index >= 0 && effect_index <= 11) {
      fix_effect_line(line, spell, level, effect_index);
      bool has_level_range = line.find("(L") != std::string::npos;
      BYTE effect = spell->Attrib[effect_index];
      std::string value1;
      std::string value2;
      if (is_buff) {
        if (!is_detrimental && (has_level_range || spell->IsInstrumentModdableSpellEffect(effect))) {
          CalculateEffectAtLevel(value1, spell, level, effect_index, bardmodifier);
          line += std::format("<c \"#c0c000\"> [{}]</c>", value1.c_str());  // In yellow.
        }
      } else if (spell->IsInstrumentModdableSpellEffect(effect)) {
        CalculateEffectAtLevel(value1, spell, level, effect_index, 10);
        CalculateEffectAtLevel(value2, spell, level, effect_index, GetMaxBardModifier());
        if (has_level_range)
          line += std::format("<c \"#c0c000\"> [{}-{} @ L{}]</c>", value1.c_str(), value2.c_str(),
                              level);  // In yellow.
        else
          line += std::format("<c \"#c0c000\"> [{}-{}]</c>", value1.c_str(), value2.c_str());  // In yellow.
      } else if (has_level_range) {
        CalculateEffectAtLevel(value1, spell, level, effect_index);
        line += std::format("<c \"#c0c000\"> [{} @ L{}]</c>", value1.c_str(), level);  // In yellow.
      }
    }
  }
}

static std::string get_spell_info(int spell_id) {
  // Could add a memory cache, but the file access seems quick enough.
  std::filesystem::path full_filename =
      UISkin::get_zeal_resources_path() / std::filesystem::path("spell_info") / std::filesystem::path("spell_info.txt");
  std::ifstream spell_file;
  spell_file.open(full_filename.c_str());
  if (spell_file.fail()) return "";

  // Scan through file skipping all lines then get the relevant one.
  for (int i = 0; i < spell_id; ++i) {
    spell_file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    if (spell_file.eof()) return "";
  }
  std::string info;
  if (!std::getline(spell_file, info)) return "";  // Failed to read or no data for this spell id.

  return info;
}

// Appends item focus effects to the item display text if enabled, the item has an effect,
// and the info is available.
static void append_item_focus_info(Zeal::GameUI::ItemDisplayWnd *wnd, Zeal::GameStructures::_GAMEITEMINFO *item) {
  if (!ZealService::get_instance()->item_displays->setting_enhanced_spell_info.get() || !wnd ||
      !wnd->DisplayText.Data || !item || item->Type != 0 || !Zeal::Game::get_self())
    return;

  int focus_spell_id = item ? item->Common.FocusSpellId : 0;
  if (focus_spell_id < 1 || focus_spell_id >= GAME_NUM_SPELLS) return;  // Not a valid focus effect spell ID.

  std::string focus_name = (Zeal::Game::get_spell_mgr() && Zeal::Game::get_spell_mgr()->Spells[focus_spell_id] &&
                            Zeal::Game::get_spell_mgr()->Spells[focus_spell_id]->Name)
                               ? Zeal::Game::get_spell_mgr()->Spells[focus_spell_id]->Name
                               : "";

  if (focus_name.empty()) return;  // Valid focus effects will have names.

  std::string description = get_spell_info(focus_spell_id);
  if (description.empty()) return;  // Failed to read or empty info.

  const std::string stml_line_break = "<BR>";
  std::string label = std::format("<c \"#ff7eff\">{}:</c>", focus_name);  // In pink/purple.
  label = stml_line_break + label + stml_line_break;
  wnd->DisplayText.Append(label.c_str());

  auto lines = Zeal::String::split_text(description, "^");
  for (auto &line : lines) {
    if (!line.starts_with("  ")) continue;  // Hack way to skip everything but effects.
    line = "&nbsp;&nbsp;" + line + stml_line_break;
    wnd->DisplayText.Append(line.c_str());
  }
  return;
}

// Appends item effects (proc, worn, click, spell scroll) to the item display text
// if enabled and the info is available.
static void append_item_effect_info(Zeal::GameUI::ItemDisplayWnd *wnd, Zeal::GameStructures::_GAMEITEMINFO *item) {
  if (!ZealService::get_instance()->item_displays->setting_enhanced_spell_info.get() || !wnd ||
      !wnd->DisplayText.Data || !item || item->Type != 0 || !Zeal::Game::get_self())
    return;

  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();
  if (!char_info) return;
  int spell_id = item->Common.SpellIdEx;
  if (spell_id < 1 || spell_id >= GAME_NUM_SPELLS) return;
  Zeal::GameStructures::SPELL *spell =
      Zeal::Game::get_spell_mgr() ? Zeal::Game::get_spell_mgr()->Spells[spell_id] : nullptr;
  if (!spell) return;

  // Items have abbreviated info appended to the display window.
  bool is_spell_scroll = (item && item->Common.Skill == Zeal::GameEnums::ItemTypeSpell);
  bool is_proc_effect = (item && item->Common.Skill != Zeal::GameEnums::ItemTypeSpell &&
                         item->Common.IsStackableEx == Zeal::GameEnums::ItemEffectCombatProc);
  bool is_worn_effect = (item && item->Common.Skill != Zeal::GameEnums::ItemTypeSpell &&
                         item->Common.IsStackableEx == Zeal::GameEnums::ItemEffectWorn);
  bool is_click_effect = (item && item->Common.Skill != Zeal::GameEnums::ItemTypeSpell &&
                          (item->Common.IsStackableEx == Zeal::GameEnums::ItemEffectClick ||
                           item->Common.IsStackableEx == Zeal::GameEnums::ItemEffectMustEquipClick ||
                           item->Common.IsStackableEx == Zeal::GameEnums::ItemEffectExpendable ||
                           item->Common.IsStackableEx == Zeal::GameEnums::ItemEffectCanEquipClick));
  if (!(is_spell_scroll || is_proc_effect || is_worn_effect || is_click_effect))
    return;  // Not an item with a displayable spell effect.

  std::string description = get_spell_info(spell_id);
  if (description.empty()) return;  // Failed to read or no data for this spell id.

  bool detrimental = spell->SpellType == 0;

  const std::string stml_line_break = "<BR>";
  wnd->DisplayText.Append(stml_line_break.c_str());  // Add a line break.

  int level = GetCastingLevel(char_info, spell, item);
  if (!is_spell_scroll)
    wnd->DisplayText.Append(std::format("Effect casting level: {}{}", level, stml_line_break).c_str());

  auto lines = Zeal::String::split_text(description, "^");
  for (auto &line : lines) {
    if (line.starts_with("Skill:") || line.starts_with("Mana Cost:") || line.starts_with("Classes:") || line.empty())
      continue;  // Skip redundant lines already in the base display.
    if ((is_click_effect || is_proc_effect || is_worn_effect) && line.starts_with("Cast Time:"))
      continue;  // Skip, shown already for clicks and n/a for worn.
    if ((is_proc_effect || is_worn_effect) && line.starts_with("Range:"))
      continue;  // Skip, range is ignored for procs and worn effects.
    if (!is_spell_scroll && line.starts_with("Recast Time:")) continue;  // Recast doesn't matter for item effects.
    if (line.starts_with("Resist:") && !detrimental) continue;           // Skip resists if not a detrimental spell.
    if (is_worn_effect && (line.starts_with("Target:") || line.starts_with("Duration:")))
      continue;  // Skip, these are also ignored for worn effects.

    add_value_at_level(line, spell, level);
    if (line.starts_with("  "))
      line = "&nbsp;&nbsp;" + line + stml_line_break;  // Indent effects for items.
    else
      line += stml_line_break;
    wnd->DisplayText.Append(line.c_str());
  }

  return;
}

// If enabled and the info is available, replace all of the text in the spell
// display box with information from the spell_info.txt file.
static bool UpdateSetSpellTextEnhanced(Zeal::GameUI::ItemDisplayWnd *wnd, int spell_id, bool buff = false) {
  if (!ZealService::get_instance()->item_displays->setting_enhanced_spell_info.get() || !wnd ||
      !wnd->DisplayText.Data || !Zeal::Game::get_self())
    return false;

  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();
  if (!char_info) return false;
  if (spell_id < 1 || spell_id >= GAME_NUM_SPELLS) return false;
  if (!Zeal::Game::get_spell_mgr()) return false;
  Zeal::GameStructures::SPELL *spell = Zeal::Game::get_spell_mgr()->Spells[spell_id];
  if (!spell) return false;

  bool is_detrimental = spell->SpellType == 0;
  int level = GetCastingLevel(char_info, spell);
  int bardmodifier = 10;

  if (buff && !is_detrimental) {
    int max_buffs = char_info->GetMaxBuffs();
    for (int i = 0; i < max_buffs; i++) {
      Zeal::GameStructures::_GAMEBUFFINFO *buff = char_info->GetBuff(i);
      if (buff && buff->SpellId == spell_id) {
        level = buff->CasterLevel;
        bardmodifier = buff->Modifier;
        break;
      }
    }
  }

  std::string description = get_spell_info(spell_id);
  if (description.empty()) return false;  // Failed to read or no data for this spell id.

  const std::string stml_line_break = "<BR>";
  wnd->DisplayText.Set("");  // Replace the SetSpell text entirely with the blob.

  auto lines = Zeal::String::split_text(description, "^");
  for (auto &line : lines) {
    if (line.starts_with("Resist:") && !is_detrimental) continue;  // Skip resists if not a detrimental spell.

    add_value_at_level(line, spell, level, bardmodifier, buff);
    line += stml_line_break;
    wnd->DisplayText.Append(line.c_str());
  }

  return true;
}

// Appends the meal or drink duration if applicable.
static void ApplyMealTime(Zeal::GameStructures::_GAMEITEMINFO *item, std::string &s) {
  const BYTE kSkillMeal = 0xe;
  const BYTE kSkillDrink = 0xf;
  if (item->Type != 0 || (item->Common.Skill != kSkillMeal && item->Common.Skill != kSkillDrink)) return;

  static constexpr std::array<const char *, 7> kSuffixes = {" snack.",  " meal.",  " feast!", " meal!",
                                                            " wetter.", " drink.", " drink!"};
  for (const auto suffix : kSuffixes) {
    if (s.ends_with(suffix)) {
      s += std::format(" ({})", item->Common.CastTime);
      return;
    }
  }
}

// Generate our customized item description text.
static void UpdateSetItemText(Zeal::GameUI::ItemDisplayWnd *wnd, Zeal::GameStructures::_GAMEITEMINFO *item) {
  if (!item || wnd->DisplayText.Data == nullptr) return;

  // Split the existing text into separate lines, release it, and then update line by line.
  const std::string stml_line_break = "<BR>";
  auto strings = Zeal::String::split_text(std::string(wnd->DisplayText), stml_line_break);
  wnd->DisplayText.Set("");
  for (auto &s : strings) {
    // Perform partial iteminfo filtering in combination with substrings.
    ApplySpellInfo(item, s);
    ApplyInstrumentModifiers(item, s);
    ApplyWeaponRatio(item, s);
    ApplyMealTime(item, s);
    s += stml_line_break;
    wnd->DisplayText.Append(s.c_str());
  }
  if (item->NoDrop != 0) wnd->DisplayText.Append("Value: " + CopperToAll(item->Cost) + stml_line_break);

  append_item_effect_info(wnd, item);
  append_item_focus_info(wnd, item);
}

void __fastcall SetItem(Zeal::GameUI::ItemDisplayWnd *wnd, int unused, Zeal::GameStructures::_GAMEITEMINFO *item,
                        bool show) {
  ZealService::get_instance()->hooks->hook_map["SetItem"]->original(SetItem)(wnd, unused, item, show);

  if (ZealService::get_instance() && ZealService::get_instance()->item_displays)
    ZealService::get_instance()->item_displays->add_to_cache(item);

  // The callers of SetItem always call Activate() on it immediately after which includes
  // a call to update the window text, so we just need to update the DisplayText here.
  UpdateSetItemText(wnd, item);
}

static std::string get_target_type_string(int target_type) {
  const char *type = nullptr;
  switch (target_type) {
    case 3:
    case 41:  // Bard songs at least.
      type = "Group";
      break;
    case 4:
      type = "Area of effect (Point blank)";
      break;
    case 5:
      type = "Single";
      break;
    case 6:
      type = "Self";
      break;
    case 8:
      type = "Area of effect (Targeted)";
      break;
    case 9:
      type = "Animal";
      break;
    case 10:
      type = "Undead";
      break;
    case 11:
      type = "Summoned";
      break;
    case 14:
      type = "Pet";
      break;
    case 16:
      type = "Plant";
      break;
    default:
      break;
  }
  if (!type) return std::format("Target: Unknown ({})", target_type);
  return std::string("Target: ") + std::string(type);
}

static void UpdateSetSpellText(Zeal::GameUI::ItemDisplayWnd *wnd, int spell_id, bool buff) {
  if (UpdateSetSpellTextEnhanced(wnd, spell_id, buff)) return;

  auto *spell_mgr = Zeal::Game::get_spell_mgr();
  if (!spell_mgr || spell_id < 1 || spell_id >= GAME_NUM_SPELLS || wnd->DisplayText.Data == nullptr) return;

  const auto *sp_data = Zeal::Game::get_spell_mgr()->Spells[spell_id];
  if (!sp_data) return;

  const std::string stml_line_break = "<BR>";
  wnd->DisplayText.Append(get_target_type_string(sp_data->TargetType) + stml_line_break);

  if (sp_data->Resist >= 0 && sp_data->Resist < 6) {
    const char *resist_lut[6] = {"Unresistable", "Magic", "Fire", "Cold", "Poison", "Disease"};  // No chromatic yet
    std::string resist_type(resist_lut[sp_data->Resist]);
    // std::string resist_adj = std::format(" ({:+d})", sp_data->ResistAdj);  // Unexpected values.
    wnd->DisplayText.Append("Resist: " + resist_type + stml_line_break);
  }
  if (sp_data->SpellType == 0) wnd->DisplayText.Append(std::string("Detrimental") + stml_line_break);
  if (sp_data->Location > 0 && sp_data->Location < 3)
    wnd->DisplayText.Append(std::string("Location: ") +
                            std::string((sp_data->Location == 1) ? "Outdoors" : "Dungeons") + stml_line_break);
}

void __fastcall SetSpell(Zeal::GameUI::ItemDisplayWnd *wnd, int unused, int spell_id, bool show, int unknown) {
  ZealService *zeal = ZealService::get_instance();
  bool buff = !show;  // Buff bar sets show to false.
  if (!show)          // Allow enhanced spell info to enable show (else blank).
    show = ZealService::get_instance()->item_displays->setting_enhanced_spell_info.get();
  zeal->hooks->hook_map["SetSpell"]->original(SetSpell)(wnd, unused, spell_id, show, unknown);
  UpdateSetSpellText(wnd, spell_id, buff);
}

void ItemDisplay::CleanUI() {
  for (auto &w : windows) {
    if (w) {
      if (w->IsVisible)  // Should never happen.
        w->Deactivate();

      if (reinterpret_cast<uint32_t>(w->vtbl) != Zeal::GameUI::ItemDisplayWnd::kDefaultVTableAddr)
        w->DeleteCustomVTable();
      w->Destroy();
    }
  }
  windows.clear();
  item_cache.clear();
}

void ItemDisplay::DeactivateUI() {
  for (auto &w : windows) {
    if (w && w->IsVisible) w->Deactivate();  // Calls show(0) and clears IsActivated.
  }
}

// Response handler for OP_LinkRequest that calls SetItem and Activate().
void __cdecl msg_request_inspect_item(Zeal::GameStructures::_GAMEITEMINFO *item) {
  auto *default_item_display_wnd = Zeal::Game::Windows->ItemWnd;  // Cache the default.
  Zeal::Game::Windows->ItemWnd = ZealService::get_instance()->item_displays->get_available_window(item);
  if (Zeal::Game::Windows->ItemWnd->IsVisible) Zeal::Game::Windows->ItemWnd->Deactivate();  // Avoid double activation.

  ZealService::get_instance()->hooks->hook_map["msg_request_inspect_item"]->original(msg_request_inspect_item)(item);
  Zeal::Game::Windows->ItemWnd = default_item_display_wnd;  // Restore.
}

// Replaces the default vtable callback to allow temporarily replacing the global pointer.
static int __fastcall InvSlotWnd_HandleLButtonUp(Zeal::GameUI::InvSlotWnd *wnd, int unused_edx, int mouse_x,
                                                 int mouse_y, unsigned int flags) {
  // If there is an item, modify the ItemWnd global pointer to point to one of our windows.
  auto *default_item_display_wnd = Zeal::Game::Windows->ItemWnd;
  if (wnd->IsActive && wnd->invSlot && wnd->invSlot->Item)
    Zeal::Game::Windows->ItemWnd = ZealService::get_instance()->item_displays->get_available_window(wnd->invSlot->Item);

  if (Zeal::Game::Windows->ItemWnd->IsVisible) Zeal::Game::Windows->ItemWnd->Deactivate();  // Avoid double activation.

  int result = wnd->HandleLButtonUp(mouse_x, mouse_y, flags);

  Zeal::Game::Windows->ItemWnd = default_item_display_wnd;
  return result;
}

static int __fastcall CastSpellWnd_WndNotification(Zeal::GameUI::CastSpellWnd *wnd, int unused_edx,
                                                   Zeal::GameUI::BasicWnd *src_wnd, int param_2, void *param_3) {
  // Forward all messages except for left mouse click messages with the alt key depressed.
  if (param_2 != 1 || !Zeal::Game::get_wnd_manager() || !Zeal::Game::get_wnd_manager()->AltKeyState)
    return wnd->WndNotification(src_wnd, param_2, param_3);

  // Temporarily modify the ItemWnd global pointer to point to one of our windows.
  auto *default_item_display_wnd = Zeal::Game::Windows->ItemWnd;
  Zeal::Game::Windows->ItemWnd = ZealService::get_instance()->item_displays->get_available_window();

  // The HandleSpellInfoDisplay() will toggle off a visible window, so deactivate it if needed.
  if (Zeal::Game::Windows->ItemWnd->IsVisible) Zeal::Game::Windows->ItemWnd->Deactivate();

  // Invoke CCastSpellWnd::HandleSpellInfoDisplay() which calls SetSpell() and activates.
  wnd->HandleSpellInfoDisplay(src_wnd);

  Zeal::Game::Windows->ItemWnd = default_item_display_wnd;
  return 0;
}

static int __fastcall SpellBookWnd_WndNotification(Zeal::GameUI::SpellBookWnd *wnd, int unused_edx,
                                                   Zeal::GameUI::BasicWnd *src_wnd, int param_2, void *param_3) {
  // Forward all messages except for left mouse click messages with the alt key depressed.
  if (param_2 != 1 || !Zeal::Game::get_wnd_manager() || !Zeal::Game::get_wnd_manager()->AltKeyState)
    return wnd->WndNotification(src_wnd, param_2, param_3);

  // Temporarily modify the ItemWnd global pointer to point to one of our windows.
  auto *default_item_display_wnd = Zeal::Game::Windows->ItemWnd;
  Zeal::Game::Windows->ItemWnd = ZealService::get_instance()->item_displays->get_available_window();

  // The DisplaySpellInfo() will toggle off a visible window, so deactivate it if needed.
  if (Zeal::Game::Windows->ItemWnd->IsVisible) Zeal::Game::Windows->ItemWnd->Deactivate();

  // Invoke CSpellBookWnd::DisplaySpellInfo() which calls SetSpell() and activates.
  wnd->DisplaySpellInfo(src_wnd);

  Zeal::Game::Windows->ItemWnd = default_item_display_wnd;
  return 0;
}

ItemDisplay::ItemDisplay(ZealService *zeal) {
  if (!Zeal::Game::is_new_ui()) return;  // Old UI not supported.

  windows.clear();
  zeal->hooks->Add("SetItem", 0x423640, SetItem, hook_type_detour);    // CItemDisplayWnd::SetItem
  zeal->hooks->Add("SetSpell", 0x425957, SetSpell, hook_type_detour);  // CItemDisplayWnd::SetSpell
  zeal->hooks->Add("msg_request_inspect_item", 0x004e81c6, msg_request_inspect_item, hook_type_detour);
  zeal->callbacks->AddGeneric([this]() { InitUI(); }, callback_type::InitUI);
  zeal->callbacks->AddGeneric([this]() { CleanUI(); }, callback_type::CleanUI);
  zeal->callbacks->AddGeneric([this]() { DeactivateUI(); }, callback_type::DeactivateUI);

  // Modify the Alt + Left Mouse click SetItem() related callback of CInvSlotWnd.
  auto *inv_slot_wnd_vtable = Zeal::GameUI::InvSlotWnd::default_vtable;
  mem::unprotect_memory(inv_slot_wnd_vtable, sizeof(*inv_slot_wnd_vtable));
  inv_slot_wnd_vtable->HandleLButtonUp = InvSlotWnd_HandleLButtonUp;
  mem::reset_memory_protection(inv_slot_wnd_vtable);

  // Modify the Alt + Left Mouse click SetItem() related callback of CastSpellWnd.
  auto *spell_gems_wnd_vtable = Zeal::GameUI::CastSpellWnd::default_vtable;
  mem::unprotect_memory(spell_gems_wnd_vtable, sizeof(*spell_gems_wnd_vtable));
  spell_gems_wnd_vtable->WndNotification = CastSpellWnd_WndNotification;
  mem::reset_memory_protection(spell_gems_wnd_vtable);

  // Modify the Alt + Left Mouse click SetItem() related callback of SpellBookWnd.
  auto *spell_book_wnd_vtable = Zeal::GameUI::SpellBookWnd::default_vtable;
  mem::unprotect_memory(spell_book_wnd_vtable, sizeof(*spell_book_wnd_vtable));
  spell_book_wnd_vtable->WndNotification = SpellBookWnd_WndNotification;
  mem::reset_memory_protection(spell_book_wnd_vtable);

  // Not bothering to modify these windows (Retain default behavior using the default ItemDisplayWnd).
  // CBuffWindow: Alt + Left click toggles persistent one at a time, Right click is temporary.
  // CHotButtonWnd: Right click does a temporary pop-up.
}

ItemDisplay::~ItemDisplay() {}

// Notes:
//  - Besides CInvSlotWnd, the global ItemDisplayWnd is accessed in:
//   - CBuffWindow::HandleSpellInfoDisplay: InvSlotWnd sends 0x17 or 0x19 to WndNotification (probably)
//   - CCastSpellWnd::WndNotification responding to Alt+1 or 0x17 and 0x19. CSpellGemWnd::HandleLButtonUp
//   - CCastSpellWnd::HandleSpellInfoDisplay called by WndNotification
//   - CDisplay::NewUIProcessEscape
//   - CHotButtonWnd::WndNotification: Responding to 0x17 or 0x19.
//   - CSpellBookWnd::DisplaySpellInfo: Through WndNotification of Alt+1, 0x17, or 0x19.

// Deprecated patches:
//
// Setting spell was toggling the item display window unlike items. These disable that.
// mem::write<BYTE>(0x4090AB, 0xEB);
// mem::write<BYTE>(0x40a4c4, 0xEB);

// There are three separate calls to SetItem() in the client and each of those feed in the
// Zeal::Game::Windows->ItemWnd to both SetItem() and an Activate() immediately after.
// The patches below disable the Activate() call afterwards.
// mem::set(0x421EBF, 0x90, 14); // Disables in CInvSlotWnd::HandleLButtonUp.
// mem::set(0x4229BE, 0x90, 14); // Disables in CInvSlot::HandleRButtonHeld().
// mem::set(0x4e81e0, 0x90, 14); // Disables in OP_ItemLinkResponse message from the server.

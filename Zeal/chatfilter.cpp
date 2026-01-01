#include "chatfilter.h"

#include <array>
#include <string_view>

#include "callbacks.h"
#include "game_addresses.h"
#include "game_functions.h"
#include "game_ui.h"
#include "hook_wrapper.h"
#include "io_ini.h"
#include "memory.h"
#include "zeal.h"

// Standard ChannelMaps and filter offset
#define ChannelMap0 0
#define ChannelMap40 0x28
#define FILTER_OFFSET 0x64

bool chatfilter::isExtendedCM(int channelMap, int applyOffset) {
  for (auto &ec : Extended_ChannelMaps) {
    if ((channelMap + applyOffset) == ec.channelMap) {
      return true;
    }
  }
  return false;
}

bool chatfilter::isStandardCM(int channelMap, int applyOffset) {
  channelMap = channelMap + applyOffset;
  if (channelMap >= ChannelMap0 && channelMap <= ChannelMap40) {
    return true;
  }
  return false;
}

void __fastcall ClearChannelMaps(Zeal::GameUI::CChatManager *cman, int u, Zeal::GameUI::ChatWnd *window) {
  chatfilter *cf = ZealService::get_instance()->chatfilter_hook.get();

  if (window != cman->ChatWindows[0]) {
    for (auto &ec : cf->Extended_ChannelMaps) {
      if (ec.windowHandle == window) {
        ec.windowHandle = cman->ChatWindows[0];
      }
    }
    for (int i = 0; i <= ChannelMap40; i++) {
      if (cman->ChannelMapWnd[i] == window) {
        cman->ChannelMapWnd[i] = cman->ChatWindows[0];
      }
    }
  }
}

void __fastcall ClearChannelMap(Zeal::GameUI::CChatManager *cman, int u, int filter) {
  chatfilter *cf = ZealService::get_instance()->chatfilter_hook.get();
  if (cf->isExtendedCM(filter, FILTER_OFFSET)) {
    int index = filter - (0x10000 - FILTER_OFFSET);
    cf->Extended_ChannelMaps.at(index).windowHandle = cman->ChatWindows[0];
  } else if (cf->isStandardCM(filter)) {
    cman->ChannelMapWnd[filter] = cman->ChatWindows[0];
  }
}

// Return a Window handle for a filter. We'll need to store our own references here.
Zeal::GameUI::ChatWnd *__fastcall GetChannelMap(Zeal::GameUI::CChatManager *cman, int u, int filter) {
  chatfilter *cf = ZealService::get_instance()->chatfilter_hook.get();
  Zeal::GameUI::ChatWnd *windowHandle = 0;

  if (cf->isExtendedCM(filter, FILTER_OFFSET)) {
    int index = filter - (0x10000 - FILTER_OFFSET);
    windowHandle = cf->Extended_ChannelMaps.at(index).windowHandle;
  } else if (cf->isStandardCM(filter)) {
    windowHandle = cman->ChannelMapWnd[filter];
  }
  return windowHandle;
}

void __fastcall SetChannelMap(Zeal::GameUI::CChatManager *cman, int u, int filter, Zeal::GameUI::ChatWnd *window) {
  chatfilter *cf = ZealService::get_instance()->chatfilter_hook.get();

  if (cf->isExtendedCM(filter, FILTER_OFFSET)) {
    int index = filter - (0x10000 - FILTER_OFFSET);
    cf->Extended_ChannelMaps.at(index).windowHandle = window;
  } else if (cf->isStandardCM(filter)) {
    cman->ChannelMapWnd[filter] = window;
  }
}

__declspec(naked) void FilterConditional(void) {
  __asm
      {
      pushad
      }

  DWORD filterID;
  chatfilter *cf;

  cf = ZealService::get_instance()->chatfilter_hook.get();

  __asm
  {
      mov filterID, ebx
  }

  if (cf->isExtendedCM(filterID) || cf->isStandardCM(filterID, -FILTER_OFFSET)) {
    __asm
    {
            popad
            mov eax, 414123h
            jmp eax
    }
  }
  else {
    __asm
    {
            popad
            mov eax, 41426Fh
            jmp eax
    }
  }
}

static Zeal::GameUI::ContextMenu *InitializeMenu(void *notificationFunc = nullptr) {
  auto menu = Zeal::GameUI::ContextMenu::Create(0, 0, {100, 100, 100, 100});
  if (!menu) throw std::bad_alloc();

  menu->HasChildren = true;  // Note: Evaluate if these are still necessary.
  menu->HasSiblings = true;
  menu->Unknown0x015 = 0;
  menu->Unknown0x016 = 0;
  menu->Unknown0x017 = 0;
  menu->fnTable->WndNotification = notificationFunc;
  return menu;
}

int32_t __fastcall AddMenu(int this_, int u, Zeal::GameUI::ContextMenu *menu) {
  chatfilter *cf = ZealService::get_instance()->chatfilter_hook.get();
  cf->ZealMenu = InitializeMenu();

  for (auto &ec : cf->Extended_ChannelMaps) cf->ZealMenu->AddMenuItem(ec.name, ec.channelMap);
  cf->menuIndex = Zeal::Game::Windows->ContextMenuManager->AddMenu(cf->ZealMenu);

  menu->AddMenuItem("Zeal", cf->menuIndex, true, true);

  return ZealService::get_instance()->hooks->hook_map["AddMenu"]->original(AddMenu)(this_, u, menu);
}

void chatfilter::LoadSettings(Zeal::GameUI::CChatManager *cman) {
  std::string ini_name = Zeal::Game::get_ui_ini_filename();
  if (ini_name.length()) {
    IO_ini ui_ini(ini_name);
    std::string cmap = "ChannelMap";
    int num = 41;

    chatfilter *cf = ZealService::get_instance()->chatfilter_hook.get();

    for (int i = 0; auto &filter : cf->Extended_ChannelMaps) {
      std::string this_cmap = cmap + std::to_string(num + i);
      if (!ui_ini.exists("ChatManager", this_cmap)) {
        ui_ini.setValue<int>("ChatManager", this_cmap, 0);
      }
      filter.windowHandle = cman->ChatWindows[ui_ini.getValue<int>("ChatManager", this_cmap)];
      i++;
    }
  }
}

int __fastcall CChatManager(Zeal::GameUI::CChatManager *cman, int u) {
  int retVal = ZealService::get_instance()->hooks->hook_map["CChatManager"]->original(CChatManager)(cman, u);

  chatfilter *cf = ZealService::get_instance()->chatfilter_hook.get();
  cf->LoadSettings(cman);

  return retVal;
}

void __fastcall UpdateContextMenus(Zeal::GameUI::CChatManager *cman, int u, Zeal::GameUI::ChatWnd *window) {
  chatfilter *cf = ZealService::get_instance()->chatfilter_hook.get();

  if (cf->ZealMenu) {
    for (int i = 0; i < cf->Extended_ChannelMaps.size(); i++) {
      Zeal::GameUI::ChatWnd *mapped = cf->Extended_ChannelMaps.at(i).windowHandle;
      cf->ZealMenu->CheckMenuItem(i, mapped == window);
    }
  }
  ZealService::get_instance()->hooks->hook_map["UpdateContextMenus"]->original(UpdateContextMenus)(cman, u, window);
}

void __fastcall Deactivate(Zeal::GameUI::CChatManager *cman, int u) {
  std::string ini_name = Zeal::Game::get_ui_ini_filename();
  if (ini_name.length()) {
    IO_ini ui_ini(ini_name);
    chatfilter *cf = ZealService::get_instance()->chatfilter_hook.get();
    std::string cmap = "ChannelMap";
    int num = 41;
    for (int i = 0; auto &filter : cf->Extended_ChannelMaps) {
      for (int window_index = 0; window_index < cman->MaxChatWindows; window_index++) {
        if (filter.windowHandle == cman->ChatWindows[window_index]) {
          std::string this_cmap = cmap + std::to_string(num + i);
          ui_ini.setValue<int>("ChatManager", this_cmap, window_index);
          break;
        }
      }
      i++;
    }
  }
  ZealService::get_instance()->hooks->hook_map["Deactivate"]->original(Deactivate)(cman, u);
}

void chatfilter::callback_clean_ui() {
  if (ZealMenu && menuIndex != -1) {
    Zeal::Game::Windows->ContextMenuManager->RemoveMenu(menuIndex, true);
  }
  menuIndex = -1;
  ZealMenu = NULL;
  current_string_id = 0;
  isDamage = false;
}

void chatfilter::AddOutputText(Zeal::GameUI::ChatWnd *&wnd, std::string &msg, short &channel) {
  for (auto &filter : Extended_ChannelMaps) {
    if (filter.isHandled(channel, msg)) wnd = filter.windowHandle;
  }
}

void __fastcall whoGlobalPrintChat_wrapped(int t, int unused, const char *data) {
  Zeal::Game::print_chat(USERCOLOR_WHO, data);
}

void __fastcall whoGlobalPrintChat_full(int t, int unused, const char *data, short color, bool un) {
  Zeal::Game::print_chat(USERCOLOR_WHO, data);
}

void __fastcall PrintSplit(int t, int unused, const char *data, short color_index, bool u) {
  ZealService::get_instance()->hooks->hook_map["PrintSplit"]->original(PrintSplit)(t, unused, data,
                                                                                   USERCOLOR_MONEY_SPLIT, u);
}

void __fastcall PrintAutoSplit(int t, int unused, const char *data, short color_index, bool u) {
  ZealService::get_instance()->hooks->hook_map["PrintAutoSplit"]->original(PrintAutoSplit)(t, unused, data,
                                                                                           USERCOLOR_ECHO_AUTOSPLIT, u);
}

static void HandleMyHitsMode(char *buffer, const char *s1, const char *s2, const char *s3, int damage) {
  // Support re-routing special melee attacks to the special chat color index.
  if (Zeal::Game::is_new_ui() &&
      (strcmp(s1, "backstab") == 0 || strcmp(s1, "kick") == 0 || strcmp(s1, "strike") == 0)) {
    char *output = buffer;
    char new_buffer[512];
    int hitmode = Zeal::Game::Windows->ChatManager->MyHitsMode;
    if (hitmode == 1) {
      snprintf(new_buffer, sizeof(new_buffer), "%s %s for %d", s1, s2, damage);
      output = new_buffer;
    } else if (hitmode == 2) {
      snprintf(new_buffer, sizeof(new_buffer), "%d", damage);
      output = new_buffer;
    }

    if (hitmode >= 0 && hitmode <= 2)  // Print with logging disabled (for abbreviated cases).
      Zeal::Game::get_game()->dsp_chat(output, CHANNEL_MYMELEESPECIAL, false);

    if (*Zeal::Game::is_logging_enabled && damage > -0x29)  // Comparison copied from client code.
      Zeal::Game::log(buffer);

    return;
  }

  ZealService::get_instance()->hooks->hook_map["HandleMyHitsMode"]->original(HandleMyHitsMode)(buffer, s1, s2, s3,
                                                                                               damage);
}

void HandleOtherHitsOtherMode(char *buffer, const char *s1, const char *s2, const char *s3, int damage) {
  // Support re-routing special melee attacks to the special chat color index.
  if (Zeal::Game::is_new_ui() &&
      (strcmp(s1, "backstabs") == 0 || strcmp(s1, "kicks") == 0 || strcmp(s1, "strikes") == 0)) {
    char *output = buffer;
    char new_buffer[512];
    int hitmode = Zeal::Game::Windows->ChatManager->OthersHitsMode;
    if (hitmode == 1) {
      snprintf(new_buffer, sizeof(new_buffer), "%s %s %s for %d", s3, s1, s2, damage);
      output = new_buffer;
    } else if (hitmode == 2) {
      snprintf(new_buffer, sizeof(new_buffer), "%d", damage);
      output = new_buffer;
    }

    if (hitmode >= 0 && hitmode <= 2)  // Print with logging disabled (for abbreviated cases).
      Zeal::Game::get_game()->dsp_chat(output, CHANNEL_OTHERMELEESPECIAL, false);

    if (*Zeal::Game::is_logging_enabled && damage > -0x29)  // Comparison copied from client code.
      Zeal::Game::log(buffer);

    return;
  }

  ZealService::get_instance()->hooks->hook_map["HandleOtherHitsOtherMode"]->original(HandleOtherHitsOtherMode)(
      buffer, s1, s2, s3, damage);
}

// Returns true if the fizzle message is not from a group member.
static bool is_non_group_fizzle(const char *data) {
  if (!data) return false;
  std::string message(data);
  const std::string suffix("'s spell fizzles!");
  if (!message.ends_with(suffix)) return false;
  if (!Zeal::Game::GroupInfo->is_in_group()) return true;
  std::string name = message.substr(0, message.length() - suffix.length());
  for (int i = 0; i < GAME_NUM_GROUP_MEMBERS; i++)
    if (name == Zeal::Game::GroupInfo->Names[i]) return false;
  return true;
}

// The relevant messages start with the source name (aka  "%1 Scores a critical hit !(%2)"
static bool is_from_me(const char *data) {
  auto self = Zeal::Game::get_self();
  if (!self || !data) return false;

  // Check that the start of the message has 'Name' + a space.
  const char *my_name = Zeal::Game::strip_name(self->Name);
  auto length = strlen(my_name);
  return (!strncmp(my_name, data, length) && strlen(data) > length && data[length] == ' ');
}

// Returns true if the id is from an "item speech" string.
static bool is_item_speech(int current_string_id) {
  static constexpr std::array<int, 10> item_speech_strings = {{
      422,   // Your %1 begins to glow.
      1230,  // Your %1 flickers with a pale light.
      1231,  // Your %1 pulses with light as your vision sharpens.
      1232,  // Your %1 feeds you with power.
      1233,  // You feel your power drain into your %1.
      1234,  // Your %1 seems drained of power.
      1235,  // Your %1 feels alive with power.
      1236,  // Your %1 sparkles.
      1237,  // Your %1 grows dim.
      1238,  // Your %1 begins to shine.
  }};

  for (const int &i : item_speech_strings) {
    if (current_string_id == i) return true;
  }
  return false;
}

enum class PetSpeech { NotPet, MyPetSay, OtherPetSay };

static PetSpeech is_pet_speech(int string_id, short color_index, const char *data) {
  static const std::array<int, 5> my_pet_string_ids{
      438,  // Taunting attacker, Master.
      489,  // No longer taunting attackers, Master.
      490,  // Taunting attackers as normal, Master.
      555,  // %1 tells you, 'I am unable to wake %2, master.'
      5501  // %1 tells you, 'Attacking %2 Master.'
  };

  // These pet messages are exceptions and do not use the PetResponse color_index.
  static const std::array<const char *, 3> exception_pet_strings{
      "My leader is ",                                           // 1136
      "I follow no one.",                                        // 1137
      "I beg forgiveness, Master. That is not a legal target.",  // 1139
  };

  static const short kPetResponse = 337;  // Chat::PetResponse.
  static const int kGenericSay = 554;     // StringID:GENERIC_STRINGID_SAY

  if (color_index != kPetResponse) {
    // Scan the few messages that don't use kPetResponse but do all use generic say.
    if (string_id != kGenericSay) return PetSpeech::NotPet;
    std::string_view message = std::string_view(data);
    bool found = false;
    for (const auto &substr : exception_pet_strings)
      if (message.find(substr) != std::string::npos) found = true;
    if (!found) return PetSpeech::NotPet;
  }

  const auto *pet = Zeal::Game::get_pet();
  if (!pet) return PetSpeech::OtherPetSay;  // If we don't have a pet, not ours.

  if (string_id == 488)        // /pet health: I have %1 percent of my hit points left.
    return PetSpeech::NotPet;  // Do not route to keep with buffs on Chat::White.

  // Next check the explicit string IDs that only come from "my pet".
  for (const int &i : my_pet_string_ids) {
    if (string_id == i) return PetSpeech::MyPetSay;
  }

  // Then check for pet name matches to see if it is ours.
  //  Most pet sayings use StringID::GENERIC_STRINGID_SAY = 554: %1 says '%T2'
  if (string_id != kGenericSay) return PetSpeech::OtherPetSay;

  // We have a %T2 pet message. Now sort out if the %1 is equal to the client's pet name.
  const char *pet_name = Zeal::Game::strip_name(pet->Name);
  std::string message = std::string(data);
  size_t end_of_name_space = message.find(" says");
  if (end_of_name_space == std::string::npos) return PetSpeech::NotPet;
  auto name = message.substr(0, end_of_name_space);
  return strcmp(name.c_str(), pet_name) ? PetSpeech::OtherPetSay : PetSpeech::MyPetSay;
}

void __fastcall serverPrintChat(int t, int unused, const char *data, short color_index, bool u) {
  chatfilter *cf = ZealService::get_instance()->chatfilter_hook.get();
  if (cf->current_string_id == 1219 && cf->setting_suppress_missed_notes.get() &&
      !strncmp("A missed note brings ", data, 21)) {
    cf->current_string_id = 0;
    return;  // Just drop missed note messages from others.
  } else if (cf->current_string_id == 1218 && cf->setting_suppress_other_fizzles.get() && is_non_group_fizzle(data)) {
    cf->current_string_id = 0;
    return;  // Just drop fizzles from others.
  }

  PetSpeech pet_speech = is_pet_speech(cf->current_string_id, color_index, data);

  // Support filtering messages from other pets on the pet response channel (let leader ones through).
  if (pet_speech == PetSpeech::OtherPetSay && color_index == 337 && cf->setting_suppress_other_pets.get()) {
    cf->current_string_id = 0;
    return;  // Just drop other pet messages.
  }

  if (pet_speech == PetSpeech::MyPetSay)
    color_index = CHANNEL_MYPETSAY;
  else if (pet_speech == PetSpeech::OtherPetSay)
    color_index = CHANNEL_OTHERPETSAY;
  else if (color_index == USERCOLOR_MELEE_CRIT && cf->current_string_id != 143 && !is_from_me(data))
    color_index = CHANNEL_OTHER_MELEE_CRIT;
  else if (is_item_speech(cf->current_string_id))
    color_index = CHANNEL_ITEMSPEECH;

  ZealService::get_instance()->hooks->hook_map["serverPrintChat"]->original(serverPrintChat)(t, unused, data,
                                                                                             color_index, u);
  cf->current_string_id = 0;
}

char *__fastcall serverGetString(int stringtable, int unused, int string_id, bool *valid) {
  chatfilter *cf = ZealService::get_instance()->chatfilter_hook.get();
  cf->current_string_id = string_id;  // Cache string id for use in serverPrintChat.
  return ZealService::get_instance()->hooks->hook_map["serverGetString"]->original(serverGetString)(stringtable, unused,
                                                                                                    string_id, valid);
}

// Suppress the you beam a smile and lifetap messages.
void chatfilter::handle_suppress_lifetaps(bool value) {
  // The client already filters these messages out if the entity level is >= 35. Just patch that value.
  static const BYTE kOriginalFilterLevel = 35;
  const BYTE filter_level = value ? 1 : kOriginalFilterLevel;
  if (*reinterpret_cast<BYTE *>(0x0052a07e) == filter_level) return;

  mem::write<BYTE>(0x0052a07e, filter_level);
}

void chatfilter::callback_hit(Zeal::GameStructures::Entity *source, Zeal::GameStructures::Entity *target, WORD type,
                              short spell_id, short damage, char output_text) {
  bool other_non_melee =
      (spell_id > 0 && source && source != Zeal::Game::get_self() && target != Zeal::Game::get_self());
  if (output_text || (other_non_melee && damage > 0)) {
    isDamage = true;
    damageData = {source, target, type, spell_id, damage};
  }

  if (!other_non_melee || (damage == 0) || !setting_report_other_non_melee_dmg.get()) return;

  bool is_ds_damage_to_non_pet_npcs =
      (damage < 0 && type >= 244 && type <= 249 && target->Type == Zeal::GameEnums::EntityTypes::NPC &&
       target->PetOwnerSpawnId == 0);
  if (damage > 0 || is_ds_damage_to_non_pet_npcs) {
    if (damage < 0) damage = -damage;
    if ((source->Position.Dist2D(Zeal::Game::get_self()->Position) < 500 ||
         target->Position.Dist2D(Zeal::Game::get_self()->Position) < 500) &&
        std::abs(source->Position.z - Zeal::Game::get_self()->Position.z) < 20) {
      short channel = is_ds_damage_to_non_pet_npcs ? CHANNEL_OTHER_DAMAGE_SHIELD : USERCOLOR_NON_MELEE;
      Zeal::Game::print_chat(channel, "%s hit %s for %i points of non-melee damage.",
                             Zeal::Game::strip_name(source->Name), Zeal::Game::strip_name(target->Name), damage);
    }
  }
}

// The Zeal Spam producers (nameplate tagging as top priority) have options to
// suppress / re-route the channels / modify the text.
bool chatfilter::HandleZealSpamCallbacks(short &color_index, std::string &msg) {
  for (const auto &callback : zeal_spam_callbacks) {
    if (callback(color_index, msg)) return true;
  }
  return false;
}

chatfilter::chatfilter(ZealService *zeal) {
  if (!Zeal::Game::is_new_ui()) return;  // Old UI not supported.

  zeal->callbacks->AddReportSuccessfulHit(
      [this](Zeal::GameStructures::Entity *source, Zeal::GameStructures::Entity *target, WORD type, short spell_id,
             short damage, char out_text) { callback_hit(source, target, type, spell_id, damage, out_text); });

  zeal->callbacks->AddGeneric([this]() { isDamage = false; }, callback_type::ReportSuccessfulHitPost);

  Extended_ChannelMaps.push_back(CustomFilter(
      "Random", 0x10000, [this](short &color, const std::string &data) { return color == USERCOLOR_RANDOM; }));
  Extended_ChannelMaps.push_back(
      CustomFilter("Loot", 0x10001, [this](short &color, const std::string &data) { return color == USERCOLOR_LOOT; }));
  Extended_ChannelMaps.push_back(CustomFilter("Money", 0x10002, [this](short &color, const std::string &data) {
    return color == USERCOLOR_MONEY_SPLIT || color == USERCOLOR_ECHO_AUTOSPLIT;
  }));
  Extended_ChannelMaps.push_back(
      CustomFilter("My Pet Say", 0x10003,
                   [this, zeal](short &color, const std::string &data) { return color == CHANNEL_MYPETSAY; }));
  Extended_ChannelMaps.push_back(
      CustomFilter("My Pet Damage", 0x10004, [this, zeal](short &color, const std::string &data) {
        if (isDamage && damageData.source && damageData.source->PetOwnerSpawnId &&
            damageData.source->PetOwnerSpawnId == Zeal::Game::get_self()->SpawnId) {
          color = CHANNEL_MYPETDMG;
          return true;
        }
        if (isDamage && damageData.target && damageData.target->PetOwnerSpawnId &&
            damageData.target->PetOwnerSpawnId == Zeal::Game::get_self()->SpawnId) {
          color = CHANNEL_MYPETDMG;
          return true;
        }
        return false;
      }));
  Extended_ChannelMaps.push_back(
      CustomFilter("Other Pet Say", 0x10005,
                   [this, zeal](short &color, const std::string &data) { return color == CHANNEL_OTHERPETSAY; }));
  Extended_ChannelMaps.push_back(
      CustomFilter("Other Pet Damage", 0x10006, [this, zeal](short &color, const std::string &data) {
        if (isDamage && damageData.target == Zeal::Game::get_self()) return false;  // Don't re-route damage to self.
        if (isDamage && damageData.source && damageData.source->PetOwnerSpawnId &&
            damageData.source->PetOwnerSpawnId != Zeal::Game::get_self()->SpawnId) {
          color = CHANNEL_OTHERPETDMG;
          return true;
        }
        if (isDamage && damageData.target && damageData.target->PetOwnerSpawnId &&
            damageData.target->PetOwnerSpawnId != Zeal::Game::get_self()->SpawnId) {
          color = CHANNEL_OTHERPETDMG;
          return true;
        }
        return false;
      }));
  Extended_ChannelMaps.push_back(CustomFilter(
      "/who", 0x10007, [this, zeal](short &color, const std::string &data) { return color == USERCOLOR_WHO; }));
  Extended_ChannelMaps.push_back(
      CustomFilter("My Melee Special", 0x10008,
                   [this, zeal](short &color, const std::string &data) { return color == CHANNEL_MYMELEESPECIAL; }));
  Extended_ChannelMaps.push_back(
      CustomFilter("Other Melee Special", 0x10009,
                   [this, zeal](short &color, const std::string &data) { return color == CHANNEL_OTHERMELEESPECIAL; }));
  Extended_ChannelMaps.push_back(CustomFilter(
      "/mystats", 0x1000A, [this, zeal](short &color, const std::string &data) { return color == CHANNEL_MYSTATS; }));
  Extended_ChannelMaps.push_back(
      CustomFilter("Item Speech", 0x1000B,
                   [this, zeal](short &color, const std::string &data) { return color == CHANNEL_ITEMSPEECH; }));
  Extended_ChannelMaps.push_back(
      CustomFilter("Other Melee Critical", 0x1000C,
                   [this, zeal](short &color, const std::string &data) { return color == CHANNEL_OTHER_MELEE_CRIT; }));
  Extended_ChannelMaps.push_back(CustomFilter(
      "Other Damage Shield", 0x1000D,
      [this, zeal](short &color, const std::string &data) { return color == CHANNEL_OTHER_DAMAGE_SHIELD; }));
  Extended_ChannelMaps.push_back(CustomFilter(
      "Zeal Spam", 0x1000E, [this](short &color, std::string &data) { return HandleZealSpamCallbacks(color, data); }));

  // Callbacks
  zeal->callbacks->AddOutputText([this](Zeal::GameUI::ChatWnd *&wnd, std::string &msg, short &channel) {
    this->AddOutputText(wnd, msg, channel);
  });
  zeal->callbacks->AddGeneric([this]() { callback_clean_ui(); }, callback_type::CleanUI);

  // ChatManager
  zeal->hooks->Add("CChatManager", 0x4100e2, CChatManager, hook_type_detour);
  zeal->hooks->Add("Deactivate", 0x410871, Deactivate, hook_type_detour);
  zeal->hooks->Add("AddMenu", 0x4120DD, AddMenu, hook_type_replace_call);
  zeal->hooks->Add("GetChannelMap", 0x41161D, GetChannelMap, hook_type_detour);
  zeal->hooks->Add("SetChannelMap", 0x4113F1, SetChannelMap, hook_type_detour);
  zeal->hooks->Add("ClearChannelMap", 0x41140C, ClearChannelMap, hook_type_detour);
  zeal->hooks->Add("ClearChannelMaps", 0x411638, ClearChannelMaps, hook_type_detour);
  zeal->hooks->Add("UpdateContextMenus", 0x412f9b, UpdateContextMenus, hook_type_detour);
  zeal->hooks->Add("HandleMyHitsMode", 0x500f86, HandleMyHitsMode, hook_type_detour);
  zeal->hooks->Add("HandleOtherHitsOtherMode", 0x501168, HandleOtherHitsOtherMode, hook_type_detour);

  // Individiual Modifications
  zeal->hooks->Add("PrintSplit", 0x54755b, PrintSplit, hook_type_replace_call);          // fix up money split
  zeal->hooks->Add("PrintAutoSplit", 0x4FB477, PrintAutoSplit, hook_type_replace_call);  // fix up money split
  zeal->hooks->Add("serverGetString", 0x4EE6C9, serverGetString, hook_type_replace_call);
  zeal->hooks->Add("serverPrintChat", 0x4ee727, serverPrintChat, hook_type_replace_call);
  zeal->hooks->Add("whoGlobalPrintChat1", 0x4e4d6f, whoGlobalPrintChat_wrapped, hook_type_replace_call);
  zeal->hooks->Add("whoGlobalPrintChat2", 0x4e4d7e, whoGlobalPrintChat_wrapped, hook_type_replace_call);
  zeal->hooks->Add("whoGlobalPrintChat3", 0x4e523a, whoGlobalPrintChat_full, hook_type_replace_call);
  zeal->hooks->Add("whoGlobalPrintChat4", 0x4e53b1, whoGlobalPrintChat_wrapped, hook_type_replace_call);

  // ChatWindow::WndNotification Conditional Patch
  mem::write<BYTE[2]>(0x414117, {0x8d, 0x05});  // lea eax
  mem::write<int>(0x414119, (int)FilterConditional);
  mem::write<BYTE[2]>(0x41411D, {0xFF, 0xE0});  // jmp eax
  mem::write<BYTE[4]>(0x41411F, {0x90, 0x90, 0x90, 0x90});
}

chatfilter::~chatfilter() {}

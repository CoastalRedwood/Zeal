#include "chat.h"

#include <algorithm>
#include <format>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <unordered_set>

#include "binds.h"
#include "callbacks.h"
#include "chatfilter.h"
#include "commands.h"
#include "entity_manager.h"
#include "game_addresses.h"
#include "game_functions.h"
#include "game_packets.h"
#include "game_structures.h"
#include "game_ui.h"
#include "hook_wrapper.h"
#include "labels.h"
#include "memory.h"
#include "string_util.h"
#include "zeal.h"

std::map<std::string, std::string> channelPrefixes = {
    {"guild", "G"},             // Guild
    {"party", "P"},             // Group (Received)
    {"group", "P"},             // Group (Sent)
    {"shout", "Sh"},            // Shout
    {"auction", "A"},           // Auction
    {"out of character", "O"},  // OOC
    {"BROADCAST", "B"},         // Broadcast
    {"tell", "Fr"},             // Tell (Received)
    {"say", "S"},               // Say
    {"told", "To"},             // TellEcho (Sent)
    {"raid", "R"},              // Raid
};

std::string playerRolling;

std::string ReadFromClipboard() {
  std::string text;
  if (OpenClipboard(nullptr)) {
    HANDLE hData = GetClipboardData(CF_TEXT);
    if (hData != nullptr) {
      char *pszText = static_cast<char *>(GlobalLock(hData));
      if (pszText != nullptr) {
        text = pszText;
        GlobalUnlock(hData);
      }
    }
    CloseClipboard();
  }
  return text;
}

void SetClipboardText(const std::string &text) {
  if (OpenClipboard(nullptr)) {
    EmptyClipboard();
    HGLOBAL hClipboardData = GlobalAlloc(GMEM_MOVEABLE, text.length() + 1);
    if (hClipboardData != nullptr) {
      char *buffer = static_cast<char *>(GlobalLock(hClipboardData));
      if (buffer != nullptr) {
        memcpy(buffer, text.c_str(), text.length());
        buffer[text.length()] = '\0';  // Null-terminate the string
        GlobalUnlock(hClipboardData);
        SetClipboardData(CF_TEXT, hClipboardData);
      } else {
        std::cerr << "Failed to lock clipboard memory." << std::endl;
      }
    } else {
      std::cerr << "Failed to allocate memory for clipboard data." << std::endl;
    }
    CloseClipboard();
  } else {
    std::cerr << "Failed to open clipboard." << std::endl;
  }
}

std::string StripSpecialCharacters(const std::string &input) {
  std::string result = input;
  result.erase(std::remove_if(result.begin(), result.end(), [](char c) { return c == '\n' || c == '\r'; }),
               result.end());
  return result;
}

std::string abbreviateChat(const std::string &original_message) {
  // Pattern to look for chat messages
  static const std::regex chat_pattern(
      R"(^([\w ]+) (?:(?:say to your |says? |tells the |tell your |(told|tell)s? )(say)?(?:\w+:)?([\w\d: ]+)|(auction|say|shout|BROADCAST)[sS]?),?[^']+'(.*)'\s*$)");

  static const std::regex roll_player_pattern(R"(^\*\*A Magic Die is rolled by (\w+)\.$)");
  static const std::regex roll_result_pattern(
      R"(^\*\*It could have been any number from (\d+) to (\d+), but this time it turned up a (\d+)\.$)");

  std::smatch match;

  if (std::regex_search(original_message, match, chat_pattern)) {
    // match[1] is always the sender
    // match[6] is always the message
    // The channel can be one of the folllowing
    //   match[5] if it exists
    //   match[4] if it's a number (e.g. 1 from 'General:1')
    //   match[4] if it's a known (e.g. 'party', 'guild')
    //   otherwise, match[2] is the channel

    std::string sender = match[1].str();
    std::string message = match[6].str();
    std::string channel;
    std::string channel_prefix;
    if (match[5].matched) {
      channel = match[5].str();
    } else if (match[4].matched) {
      if (std::regex_match(match[4].str(), std::regex(R"(\d+)"))) {
        channel = match[4].str();
        channel_prefix = channel;  // Use the number for the prefix
        // Could be a channel or a player, so need to be specific
      } else if (match[4].str() == "party" || match[4].str() == "group" || match[4].str() == "guild" ||
                 match[4].str() == "raid" || match[4].str() == "out of character") {
        channel = match[4].str();
      } else {
        channel = match[2].str();
      }
    }

    // Match known channels with prefixes (if not already set)
    if (channel_prefix.empty() && channelPrefixes.count(channel)) {
      channel_prefix = channelPrefixes[channel];
    }

    // Abort if a prefix wasn't found
    if (channel_prefix.empty()) {
      return original_message;
    }

    if (channel == "told") {
      // For told, match[4] is the sender
      sender = match[4].str();
    }

    // Replace you with your actual name
    if (sender == "You") {
      Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
      if (self) {
        sender = self->Name;
      }
    }

    std::string newMessage = "[" + channel_prefix + "] [" + sender + "]: " + message;
    return newMessage;
  }

  if (std::regex_search(original_message, match, roll_player_pattern)) {
    playerRolling = match[1].str();  // Player will be used when the actual roll result is printed
    return std::string();            // Prevent this line from being printed
  }

  if (std::regex_search(original_message, match, roll_result_pattern)) {
    if (playerRolling.length() == 0)  // Just in case
      playerRolling = "?????";
    std::string rollMin = match[1].str();
    std::string rollMax = match[2].str();
    std::string rollResult = match[3].str();
    std::string newMessage = "[" + rollMin + "-" + rollMax + "]: " + rollResult + " rolled by " + playerRolling + ".";
    playerRolling = std::string();  // Clear it for the next person
    return newMessage;
  }

  // If there were no matches, return the original message
  return original_message;
}

DWORD get_class_color(std::string character_name, short channel) {
  static const std::unordered_set<int> valid_channels_you = {
      USERCOLOR_SPELLS,         USERCOLOR_YOU_HIT_OTHER,   USERCOLOR_OTHER_HIT_YOU,  USERCOLOR_YOU_MISS_OTHER,
      USERCOLOR_OTHER_MISS_YOU, USERCOLOR_DISCIPLINES,     USERCOLOR_YOUR_DEATH,     USERCOLOR_OTHER_DEATH,
      USERCOLOR_NON_MELEE,      USERCOLOR_MONEY_SPLIT,     USERCOLOR_LOOT,           USERCOLOR_OTHERS_SPELLS,
      USERCOLOR_SPELL_FAILURE,  USERCOLOR_MELEE_CRIT,      USERCOLOR_SPELL_CRIT,     USERCOLOR_NPC_RAMPAGE,
      USERCOLOR_NPC_FURRY,      USERCOLOR_NPC_ENRAGE,      CHANNEL_OTHERPETDMG,      CHANNEL_MYPETSAY,
      CHANNEL_MYMELEESPECIAL,   CHANNEL_OTHERMELEESPECIAL, CHANNEL_OTHER_MELEE_CRIT, CHANNEL_OTHER_DAMAGE_SHIELD,
  };

  // Set class color for self
  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();
  if ((character_name == "You" || character_name == "Your") && valid_channels_you.count(channel) &&
      char_info != nullptr)
    return Zeal::Game::get_raid_class_color(char_info->Class);

  // Check Zone Entities for a match
  auto entity = ZealService::get_instance()->entity_manager->Get(character_name);
  if (entity) {
    if (entity->Type == 0 && !entity->AnonymousState) return Zeal::Game::get_raid_class_color(entity->Class);
  }

  // Check raid members for a match
  Zeal::GameStructures::RaidInfo *raid_info = Zeal::Game::RaidInfo;
  if (raid_info->is_in_raid()) {
    for (int i = 0; i < Zeal::GameStructures::RaidInfo::kRaidMaxMembers; ++i) {
      const auto &member = raid_info->MemberList[i];
      if (character_name == member.Name) return Zeal::Game::get_raid_class_color(member.ClassValue);
    }
  }

  // No match, return 0 (0x00000000)
  return 0;
}

std::string add_class_colors(std::string message, short channel) {
  auto entity_manager = ZealService::get_instance()->entity_manager.get();
  if (!entity_manager) return message;  // Abort if entity manager unavailable

  // Pattern to find STML tags or possible names
  static const std::regex tags_or_names_pattern(R"((<[^>]*>)|(\b(?:[a-zA-Z]{4,}|Your?)\b))", std::regex::icase);

  // Find matches
  auto words_begin = std::sregex_iterator(message.cbegin(), message.cend(), tags_or_names_pattern);
  auto words_end = std::sregex_iterator();

  // Return original message if no matches were found
  if (words_begin == words_end) {
    return message;
  }

  // Modified string for output
  std::string result;
  result.reserve(message.length() * 2);

  // Keep track of the last match postion
  std::string::const_iterator last_match_pos = message.cbegin();

  for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
    std::smatch match = *i;

    // Append non-matched data up to the current match
    result.append(last_match_pos, match[0].first);
    last_match_pos = match[0].second;

    // If this matches a STML tag, no modifications are needed
    if (match[1].matched) {
      result.append(match.str());
    }
    // Possible name, if it matches an entiy, add color tags
    else if (match[2].matched) {
      std::string possible_name = match.str();
      std::string possible_name_lower = possible_name;
      std::transform(possible_name_lower.begin(), possible_name_lower.end(), possible_name_lower.begin(), ::tolower);
      std::string possible_name_capitalized = possible_name_lower;
      possible_name_capitalized[0] = std::toupper(possible_name_capitalized[0]);

      // Try to find class color for name
      DWORD class_color = get_class_color(possible_name_capitalized, channel);

      // Add color tags if a match was found
      if (class_color) {
        std::string replacement_name = std::format("<c \"#{:06x}\">{}</c>", class_color & 0x00ffffff, possible_name);
        result.append(replacement_name);
      } else {
        result.append(possible_name);
      }
    }
  }
  // Append remaining message
  result.append(last_match_pos, message.cend());

  return result;
}

std::string generateTimestampedString(const std::string &message, int timestamp_style) {
  time_t rawtime;
  struct tm timeinfo;
  time(&rawtime);
  localtime_s(&timeinfo, &rawtime);

  std::ostringstream oss;
  if (timestamp_style == 1) {
    oss << "[" << std::setw(2) << std::setfill('0') << ((timeinfo.tm_hour % 12 == 0) ? 12 : timeinfo.tm_hour % 12)
        << ":" << std::setw(2) << std::setfill('0') << timeinfo.tm_min << ":" << std::setw(2) << std::setfill('0')
        << timeinfo.tm_sec << " " << ((timeinfo.tm_hour >= 12) ? "PM" : "AM") << "] " << message;
  } else if (timestamp_style == 2) {
    oss << "[" << std::setw(2) << std::setfill('0') << timeinfo.tm_hour << ":" << std::setw(2) << std::setfill('0')
        << timeinfo.tm_min << "] " << message;
  } else {
    oss << "[" << std::setw(2) << std::setfill('0') << timeinfo.tm_hour << ":" << std::setw(2) << std::setfill('0')
        << timeinfo.tm_min << ":" << std::setw(2) << std::setfill('0') << timeinfo.tm_sec << "] " << message;
  }
  return oss.str();
}

// Function to replace underscores with spaces in a word
std::string replaceUnderscores(const std::smatch &match) {
  std::string word = match[1].str();                 // Get the word part
  std::replace(word.begin(), word.end(), '_', ' ');  // Replace underscores with spaces
  return word + match[2].str();                      // Append the three digits
}

UINT32 __fastcall GetRGBAFromIndex(int t, int u, USHORT index) {
  auto zeal = ZealService::get_instance();
  Chat *c = zeal->chat_hook.get();
  if (!c->get_color_callback) return zeal->hooks->hook_map["GetRGBAFromIndex"]->original(GetRGBAFromIndex)(t, u, index);

  switch (index) {
    case 4:  // Update the blue color in chat, track, etc.
    case 0x10:
      if (c->UseBlueCon.get()) {
        return c->get_color_callback(14);
      }
      break;
    case CHANNEL_MYPETDMG:
      return c->get_color_callback(19);
    case CHANNEL_OTHERPETDMG:
      return c->get_color_callback(20);
    case CHANNEL_MYPETSAY:
      return c->get_color_callback(21);
    case CHANNEL_OTHERPETSAY:
      return c->get_color_callback(22);
    case CHANNEL_MYMELEESPECIAL:
      return c->get_color_callback(23);
    case CHANNEL_OTHERMELEESPECIAL:
      return c->get_color_callback(24);
    case CHANNEL_MYSTATS:
      return 0xffffffff;  // Just hard-code to white.
    case CHANNEL_ITEMSPEECH:
      index = USERCOLOR_SPELLS;  // Just hard-code to spells.
      break;
    case CHANNEL_OTHER_MELEE_CRIT:
      return c->get_color_callback(25);
    case CHANNEL_OTHER_DAMAGE_SHIELD:
      return c->get_color_callback(26);
    case CHANNEL_ZEAL_SPAM:
      return c->get_color_callback(27);
    default:
      break;
  }
  return zeal->hooks->hook_map["GetRGBAFromIndex"]->original(GetRGBAFromIndex)(t, u, index);
}

void Chat::handle_print_chat(const char *data, int color_index) {
  for (const auto &callback : print_chat_callbacks) callback(data, color_index);
}

// Note that the client PrintChat does modify the data parameter (percent converts) so it is not
// a const char* parameter, but we are adding internal buffer copies in this calls so that this
// wrapper effectively treats it like it is and makes print_chat safer to use in general.
static void __fastcall PrintChat(int t, int unused, char *data, short color_index, bool add_log) {
  if (!data || strlen(data) == 0)  // Skip phantom prints like the client does.
    return;

  ZealService::get_instance()->chat_hook->handle_print_chat(data, color_index);

  const auto &abbreviated_chat = ZealService::get_instance()->chat_hook->UseAbbreviatedChat;

  std::string abbreviated_buffer = (abbreviated_chat.get() > 0) ? abbreviateChat(data) : "";
  const char *chat_buffer = (abbreviated_chat.get() > 0) ? abbreviated_buffer.c_str() : data;
  const char *log_buffer = (abbreviated_chat.get() == 2) ? abbreviated_buffer.c_str() : data;

  // Perform extra copies to protect unwary callers against the potential buffer size growth.
  char buffer[2048];  // Client maximum buffer size for print chat calls.
  const auto &timestamp_style = ZealService::get_instance()->chat_hook->TimeStampsStyle;
  bool log_is_different = timestamp_style.get() || (chat_buffer != log_buffer);
  if (timestamp_style.get()) {
    std::string timestamp_buffer = generateTimestampedString(chat_buffer, timestamp_style.get());
    strncpy_s(buffer, timestamp_buffer.c_str(), sizeof(buffer));
  } else {
    strncpy_s(buffer, chat_buffer, sizeof(buffer));
  }

  if (std::strlen(chat_buffer) > 0)
    ZealService::get_instance()->hooks->hook_map["PrintChat"]->original(PrintChat)(t, unused, buffer, color_index,
                                                                                   add_log && !log_is_different);

  if (add_log && log_is_different && std::strlen(log_buffer) > 0 && *Zeal::Game::is_logging_enabled) {
    strncpy_s(buffer, log_buffer, sizeof(buffer));
    Zeal::Game::GameInternal::DoPercentConvert(t, unused, buffer, 0);
    Zeal::Game::log(buffer);
  }
}

char *__fastcall StripName(int t, int unused, char *data) {
  if (ZealService::get_instance()->hooks && ZealService::get_instance()->hooks->hook_map.count("StripName")) {
    if (ZealService::get_instance()->chat_hook->UseUniqueNames.get())
      return data;
    else
      return ZealService::get_instance()->hooks->hook_map["StripName"]->original(StripName)(t, unused, data);
  }
  return data;
}

void __fastcall RezConfirmationDialogActivate(int t, int u, int unknown1, int unknown2, const char *message) {
  if (message) Zeal::Game::print_chat(message);
  return ZealService::get_instance()->hooks->hook_map["RezConfirmationDialogActivate"]->original(
      RezConfirmationDialogActivate)(t, u, unknown1, unknown2, message);
}

enum class caret_dir : int { none, left, right };

enum class game_modifier_keys : int { none = 0, shift = 1, ctrl = 2, alt = 4 };

void move_caret(Zeal::GameUI::EditWnd *active_edit, caret_dir dir) {
  static caret_dir highlight_direction = dir;
  int move_index = 0;
  DWORD *caret = &active_edit->Caret_End;

  if (Zeal::Game::KeyMods->Shift && active_edit->Caret_End == active_edit->Caret_Start)
    highlight_direction = dir;
  else if (!Zeal::Game::KeyMods->Shift)
    highlight_direction = caret_dir::none;

  if (dir == caret_dir::left) {
    caret = &active_edit->Caret_Start;
    if (highlight_direction == caret_dir::right && Zeal::Game::KeyMods->Shift) caret = &active_edit->Caret_End;

    move_index = *caret - 1;
    if (move_index < 0) move_index = 0;
  } else {
    caret = &active_edit->Caret_End;
    if (highlight_direction == caret_dir::left && Zeal::Game::KeyMods->Shift) caret = &active_edit->Caret_Start;
    move_index = *caret + 1;
    if (move_index > active_edit->GetInputLength()) move_index = active_edit->GetInputLength();
  }

  for (DWORD i = 0; i < active_edit->item_link_count; i++) {
    if ((dir == caret_dir::left && active_edit->LinkEndIndex[i] - 1 == move_index) ||
        (dir == caret_dir::right && active_edit->LinkStartIndex[i] + 1 == move_index)) {
      move_index = (dir == caret_dir::left) ? active_edit->LinkStartIndex[i] : active_edit->LinkEndIndex[i];
      break;
    }
  }

  if (dir == caret_dir::left) {
    *caret = move_index;
    if (!Zeal::Game::KeyMods->Shift) active_edit->Caret_End = move_index;
  } else {
    *caret = move_index;
    if (!Zeal::Game::KeyMods->Shift) active_edit->Caret_Start = move_index;
  }
}

static std::string get_tab_completion_target(const std::string &text, const std::string &prefix_with_space) {
  // Only process if the correct prefix and some characters after the prefix's trailing space.
  int length = prefix_with_space.length();
  if (text.starts_with(prefix_with_space) && text.length() > length) {
    // Valid tab completion target fragment if no second space.
    if (text.find(' ', length) == std::string::npos) return text.substr(length);  // Return just the target phrase.
  }
  return std::string();
}

// Search the client's tell list, which is a most recent first sorted list of other players that
// have sent tells to the client.
static std::vector<std::string> get_tell_list_matches(const std::string &start_of_name) {
  std::vector<std::string> result;
  const int tell_list_size = 31;  // Stores most recent 31 tell names.
  const char(*tell_list)[64] = reinterpret_cast<const char(*)[64]>(0x007CE45C);
  for (int i = 0; i < tell_list_size; ++i) {
    if (tell_list[i][0] == 0)  // Rest of list is empty.
      break;
    if (start_of_name.empty() || _strnicmp(tell_list[i], start_of_name.c_str(), start_of_name.length()) == 0)
      result.push_back(tell_list[i]);
  }
  return result;
}

// Search the client's raid list for matches.
static std::vector<std::string> get_raid_list_matches(const std::string &start_of_name) {
  std::vector<std::string> result;
  const Zeal::GameStructures::RaidInfo *raid_info = Zeal::Game::RaidInfo;
  if (!raid_info->is_in_raid()) return result;

  const int kRaidMaxMembers = Zeal::GameStructures::RaidInfo::kRaidMaxMembers;
  for (int i = 0; i < kRaidMaxMembers; ++i) {
    const auto &member = raid_info->MemberList[i];
    if (_strnicmp(member.Name, start_of_name.c_str(), start_of_name.length()) == 0) result.push_back(member.Name);
  }
  return result;
}

// Assembles a list of all tell list, raid, and local zone name matches
static std::vector<std::string> get_all_matches(const std::string &start_of_name) {
  auto matches = get_tell_list_matches(start_of_name);
  auto raid_matches = get_raid_list_matches(start_of_name);
  for (const auto &name : raid_matches)
    if (std::find(matches.begin(), matches.end(), name) == matches.end()) matches.push_back(name);

  auto entity_manager = ZealService::get_instance()->entity_manager.get();  // Short-term ptr.
  if (entity_manager) {
    auto ent_matches = entity_manager->GetPlayerPartialMatches(start_of_name);
    for (const auto &name : ent_matches)
      if (std::find(matches.begin(), matches.end(), name) == matches.end()) matches.push_back(name);
  }
  if (Zeal::Game::get_self()) std::erase(matches, std::string(Zeal::Game::get_self()->Name));
  return matches;
}

// Enhance the /tell tab completion to include the raid and zone player list and also enable
// it for /t and /consent.
static bool handle_tell_tab_completion(Zeal::GameUI::EditWnd *active_edit, int modifier,
                                       std::vector<std::string> &matches) {
  // First check if there is an in-progress /command match list.
  if (!matches.empty() && matches.front().starts_with("/")) return false;  // Not targets, so bail out.

  // First check if the start of the text is one that supports tab completion.
  // We cycle through this even if we already have matches just to re-use code.
  std::vector<const char *> prefix_list = {"/tell", "/t", "/consent"};
  std::string text = std::string(active_edit->InputText);
  for (const char *prefix : prefix_list) {
    std::string prefix_with_space = std::string(prefix) + " ";
    if (text != prefix && !text.starts_with(prefix_with_space)) continue;  // Not a valid tab-triggered command.

    if (matches.empty()) {
      auto target = get_tab_completion_target(text, prefix_with_space);
      if (!target.empty()) {
        matches = get_all_matches(target);
        if (matches.size() > 1) {
          Zeal::Game::print_chat("Possible matches:");
          for (const auto &match : matches) Zeal::Game::print_chat("  %s", match.c_str());
        } else if (matches.empty())
          Zeal::Game::print_chat("No matching targets found for %s", target.c_str());
      }

      // Add the tell list to the back.
      if (matches.empty())
        matches = get_tell_list_matches("");
      else {
        std::vector tell_list = get_tell_list_matches("");
        for (const auto &name : tell_list)
          if (std::find(matches.begin(), matches.end(), name) == matches.end())
            matches.push_back(name);  // Add only new names.
      }

      if (matches.size() > 1 && target == matches.front())
        std::rotate(matches.begin(), matches.begin() + 1, matches.end());
    } else if (matches.size() > 1) {
      if (modifier == 0)  // Shift not down, cycle forwards
        std::rotate(matches.begin(), matches.begin() + 1, matches.end());
      else
        std::rotate(matches.rbegin(), matches.rbegin() + 1, matches.rend());
    }
    if (matches.empty())
      Zeal::Game::print_chat("Tab cycle list empty");
    else  // Update the text with the first match in the list.
    {
      const char *end_space = strcmp(prefix, "/consent") ? " " : "";
      std::string updated_text = prefix_with_space + matches.front() + end_space;
      active_edit->InputText.Set(updated_text);
      active_edit->Caret_End = active_edit->GetInputLength();
      active_edit->Caret_Start = active_edit->Caret_End;
    }
    return true;  // Break and ignore further processing of the tab key.
  }
  return false;
}

// Utility method for finding the common prefix for command tab completion.
static std::string find_common_prefix(const std::vector<std::string> &strings) {
  if (strings.empty()) return "";

  std::string prefix = strings[0];
  for (size_t i = 1; i < strings.size(); ++i) {
    while (!strings[i].starts_with(prefix)) {
      prefix = prefix.substr(0, prefix.size() - 1);
      if (prefix.empty()) return "";
    }
  }
  return prefix;
}

// Tab command completion works more like bash shell completion.
static bool handle_command_tab_completion(Zeal::GameUI::EditWnd *active_edit, int modifier,
                                          std::vector<std::string> &matches) {
  std::string text = std::string(active_edit->InputText);
  if (text.size() < 2 || text[0] != '/' || (std::find(text.begin(), text.end(), ' ') != text.end())) return false;

  std::string prefix;
  if (matches.empty()) {
    matches = Zeal::Game::get_command_matches(text);
    const auto &command_map = ZealService::get_instance()->commands_hook->CommandFunctions;
    for (const auto &entry : command_map) {
      if (entry.first.starts_with(text) && std::find(matches.begin(), matches.end(), entry.first) == matches.end())
        matches.push_back(entry.first);
      const auto &aliases = entry.second.aliases;
      for (const auto &alias : aliases)
        if (alias.starts_with(text) && std::find(matches.begin(), matches.end(), alias) == matches.end())
          matches.push_back(alias);
    }
    if (matches.size() > 1) {
      Zeal::Game::print_chat("Possible matches:");
      for (const auto &match : matches) Zeal::Game::print_chat("  %s", match.c_str());
      prefix = find_common_prefix(matches);
    }
  } else if (matches.size() > 1 && text == matches.front()) {
    if (modifier == 0)  // Shift not down, cycle forwards
      std::rotate(matches.begin(), matches.begin() + 1, matches.end());
    else
      std::rotate(matches.rbegin(), matches.rbegin() + 1, matches.rend());
  }

  if (matches.empty())
    Zeal::Game::print_chat("No matches");
  else  // Update the text with the first match in the list.
  {
    std::string updated_text = prefix.empty() ? matches.front() : prefix;
    active_edit->InputText.Set(updated_text);
    active_edit->Caret_End = active_edit->GetInputLength();
    active_edit->Caret_Start = active_edit->Caret_End;
  }
  return !matches.empty();
}

// Handles tab completion logic for tell targets and commands.
static bool check_for_tab_completion(Zeal::GameUI::EditWnd *active_edit, UINT32 key, int modifier, char keydown) {
  static std::vector<std::string> matches;  // Cache state for cycling through the list.

  bool not_a_tab_down = (key != 0xf || !keydown || (modifier != 0 && modifier != 1));
  if (not_a_tab_down || !active_edit->InputText.Data || active_edit->item_link_count ||
      active_edit->InputText.Data->Length < 2) {
    // Clear the cache if keydown is not a shift key (DIK_LSHIFT or DIK_RSHIFT).
    if (keydown && (key != 0x2a && key != 0x36))
      matches.clear();  // Flush the cached list (if any) for any other keydown press.
    return false;
  }

  if (handle_tell_tab_completion(active_edit, modifier, matches)) return true;

  if (handle_command_tab_completion(active_edit, modifier, matches)) return true;

  matches.clear();
  return false;
}

bool Chat::handle_key_press(int key, bool down, int modifier) {
  if (key_press_callback) return key_press_callback(key, down, modifier);
  return false;
}

int __fastcall EditWndHandleKey(Zeal::GameUI::EditWnd *active_edit, int u, UINT32 key, int modifier, char keydown) {
  if (!ZealService::get_instance()->chat_hook->UseZealInput.get())
    return ZealService::get_instance()->hooks->hook_map["EditWndHandleKey"]->original(EditWndHandleKey)(
        active_edit, u, key, modifier, keydown);
  // Zeal::Game::print_chat("EditWnd: 0x%x key: %x modifier: %i state: %i", active_edit, key, modifier, keydown);
  if (ZealService::get_instance()->chat_hook->handle_key_press(key, keydown, modifier)) return 0;
  if (check_for_tab_completion(active_edit, key, modifier, keydown)) {
    return 0;
  }
  // you can use a bitwise & operator on the modifier with game_modifier_keys to check key states
  if (keydown) {
    if (!active_edit->InputText.Data) active_edit->InputText.Assure(32, 0);

    static caret_dir last_highlight_dir = caret_dir::none;

    switch (key) {
      case 0x1:  // escape
      {
        Zeal::Game::get_wnd_manager()->Focused = active_edit->ParentWnd;
        return 0;
      }
      case 0xC7:  // home
      {
        active_edit->Caret_Start = 0;
        if (!Zeal::Game::KeyMods->Shift) active_edit->Caret_End = 0;
        last_highlight_dir = caret_dir::left;
        return 0;
      }
      case 0xCF:  // end
      {
        active_edit->Caret_End = active_edit->GetInputLength();
        if (!Zeal::Game::KeyMods->Shift) active_edit->Caret_Start = active_edit->Caret_End;
        last_highlight_dir = caret_dir::right;
        return 0;
      }
      case 0xCB:  // left arrow
        move_caret(active_edit, caret_dir::left);
        return 0;
      case 0xCD:  // right arrow
        move_caret(active_edit, caret_dir::right);
        return 0;
    }

    if (!Zeal::Game::KeyMods->Shift) last_highlight_dir = caret_dir::none;

    if (Zeal::Game::KeyMods->Ctrl) {
      switch (key) {
        case 0xCB:  // left arrow
        {
          move_caret(active_edit, caret_dir::left);
          return 0;
        }
        case 0xCD:  // right arrow
        {
          move_caret(active_edit, caret_dir::right);
          return 0;
        }
        case 0x2F:  // v (paste)//025595 Doljonijiarnimorinar
        {
          std::string temp_text = StripSpecialCharacters(ReadFromClipboard());
          int links_in_paste = std::count(temp_text.begin(), temp_text.end(), '');
          if (links_in_paste > 0) links_in_paste /= 2;

          if (active_edit->item_link_count + links_in_paste < 10) {
            active_edit->InputText.Assure(temp_text.length() + active_edit->InputText.Data->Length + 1, 0);
            active_edit->ReplaceSelection(temp_text.c_str(), false);
          } else {
            Zeal::Game::print_chat_wnd(Zeal::Game::Windows->ChatManager->GetActiveChatWindow(), 0,
                                       "<c \"#FF0000\">Too many item links to paste</c>");
          }
          return 0;
        }
        case 0x1E:  // a (select all)
        {
          active_edit->Caret_Start = 0;
          active_edit->Caret_End = active_edit->GetInputLength();
          return 0;
        }
        case 0x2D:  // x (cut)
        case 0x2E:  // c (copy)
        {
          if (active_edit->Caret_End - active_edit->Caret_Start > 0) {
            int len_highlighted = active_edit->Caret_End - active_edit->Caret_Start;
            int highlighted_link_count = 0;
            int links_prior = 0;
            for (DWORD i = 0; i < active_edit->item_link_count; i++) {
              if (active_edit->LinkStartIndex[i] < active_edit->Caret_Start) links_prior++;
              if (active_edit->LinkEndIndex[i] <= active_edit->Caret_End &&
                  active_edit->LinkStartIndex[i] >= active_edit->Caret_Start)
                highlighted_link_count++;
            }

            int new_caret_start = active_edit->Caret_Start + (links_prior * 9);
            int new_caret_end = active_edit->Caret_End + (links_prior * 9);
            std::string highlighted_text(
                active_edit->InputText.Data->Text + new_caret_start,
                active_edit->InputText.Data->Text + new_caret_end + (highlighted_link_count * 9));
            SetClipboardText(highlighted_text);
          }
          if (key == 0x2D) active_edit->ReplaceSelection("", false);
          return 0;
        }
      }
    }
  }
  return ZealService::get_instance()->hooks->hook_map["EditWndHandleKey"]->original(EditWndHandleKey)(
      active_edit, u, key, modifier, keydown);
}

void __fastcall DoPercentConvert(int *t, int u, char *data, int u2) {
  if (!data || !ZealService::get_instance()) return;  // Early exit if data is null or ZealService instance is invalid

  std::string str_data = data;

  // Check if there's a '%' character; if not, exit early
  if (str_data.find('%') == std::string::npos) {
    return;
  }

  if (Zeal::Game::is_in_game()) {
    try {
      auto chat_hook = ZealService::get_instance()->chat_hook.get();  // Temporary raw pointer.
      if (chat_hook) {
        chat_hook->DoPercentReplacements(str_data);
        size_t new_len = str_data.length();
        std::memcpy(data, str_data.c_str(), new_len);
        data[new_len] = '\0';  // Ensure null-termination
      }
    } catch (const std::exception &e) {
      Zeal::Game::print_chat(USERCOLOR_SHOUT, "Error in DoPercentConvert: %s", e.what());
      return;
    }
  }

  // Call original function using the stored hook
  auto hook = ZealService::get_instance()->hooks->hook_map.find("DoPercentConvert");
  if (hook != ZealService::get_instance()->hooks->hook_map.end())
    hook->second->original(DoPercentConvert)(t, u, data, u2);
}

void Chat::DoPercentReplacements(std::string &str_data) {
  for (auto &fn : percent_replacements) fn(str_data);
}

std::string GetConsentMeTellName(const std::string &data) {
  static const char consent_ending[] = " tells you, 'Consent me'";
  if (!data.ends_with(consent_ending)) return "";

  // Extract the name. Normally it is the first word but we go backwards just in case.
  std::string truncated = data.substr(0, data.length() - strlen(consent_ending));
  size_t last_space = truncated.find_last_of(" ");
  std::string name = (last_space == std::string::npos) ? truncated : truncated.substr(last_space + 1);

  // And then check if the name is a raid or group member to authorize the auto consent.
  Zeal::GameStructures::RaidInfo *raid_info = Zeal::Game::RaidInfo;
  if (raid_info->is_in_raid()) {
    for (int i = 0; i < Zeal::GameStructures::RaidInfo::kRaidMaxMembers; ++i) {
      const auto &member = raid_info->MemberList[i];
      if (member.Name == name) return name;
    }
  }

  const auto *group_info = Zeal::Game::GroupInfo;
  for (int i = 0; i < GAME_NUM_GROUP_MEMBERS; ++i) {
    if (group_info->IsValidList[i] && group_info->Names[i] == name) return name;
  }
  return "";
}

void Chat::AddOutputText(Zeal::GameUI::ChatWnd *wnd, std::string &msg, short channel) {
  if (channel == USERCOLOR_TELL && EnableAutoConsent.get()) {
    std::string name = GetConsentMeTellName(msg);
    if (!name.empty()) Zeal::Game::do_consent(name.c_str());
  }

  if (UseClassChatColors.get() && !msg.empty()) {
    msg = add_class_colors(msg, channel);
  }
}

void Chat::InitPercentReplacements() {
  percent_replacements.push_back([](std::string &str_data) {
    std::string mana;
    ZealService::get_instance()->labels_hook->GetLabel(20, mana);
    Zeal::String::replace(str_data, "%mana", mana + "%");
    Zeal::String::replace(str_data, "%n", mana + "%");
  });
  percent_replacements.push_back([](std::string &str_data) {
    std::string hp;
    ZealService::get_instance()->labels_hook->GetLabel(19, hp);
    Zeal::String::replace(str_data, "%hp", hp + "%");
    Zeal::String::replace(str_data, "%h", hp + "%");
  });
  percent_replacements.push_back([](std::string &str_data) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << std::ceil(Zeal::Game::get_self()->Position.x * 100) / 100 << ", "
       << std::ceil(Zeal::Game::get_self()->Position.y * 100) / 100 << ", "
       << std::ceil(Zeal::Game::get_self()->Position.z * 100) / 100;
    Zeal::String::replace(str_data, "%loc", ss.str());
  });
  percent_replacements.push_back([](std::string &str_data) {
    std::string target_hp;
    ZealService::get_instance()->labels_hook->GetLabel(29, target_hp);
    Zeal::String::replace(str_data, "%targethp", target_hp + "%");
    Zeal::String::replace(str_data, "%th", target_hp + "%");
  });
}

void Chat::handle_incoming_gsay(const char *msg) {
  for (const auto &callback : gsay_callbacks) callback(msg);
}

void Chat::handle_incoming_rsay(const char *msg) {
  for (const auto &callback : rsay_callbacks) callback(msg);
}

bool Chat::handle_incoming_chat(const char *msg, int color_index) {
  for (const auto &callback : chat_callbacks) {
    if (callback(msg, color_index)) return true;
  }
  return false;
}

// Intercepts incoming chat channel messages from the server (replaces a specific print chat call).
static void __fastcall chatPrintChat(int t, int unused, const char *data, short color_index, bool u) {
  // Allow the handler to suppress the chat from printing if it returns a true.
  ZealService *zeal = ZealService::get_instance();
  if (data && zeal->chat_hook && zeal->chat_hook->handle_incoming_chat(data, color_index)) return;

  ZealService::get_instance()->hooks->hook_map["chatPrintChat"]->original(chatPrintChat)(t, unused, data, color_index,
                                                                                         u);
}

// Intercepts incoming channel messages from the server.
static void msg_new_text(char *msg_data) {
  if (!Zeal::Game::get_self()) return;  // Self is null, drop text to avoid potential crashes.

  static constexpr int kGroupTextChannel = 2;  // For chan_num in ChannelMessage_Struct.
  static constexpr int kRaidTextChannel = 15;  // For chan_num in ChannelMessage_Struct.

  ZealService *zeal = ZealService::get_instance();
  auto msg = reinterpret_cast<Zeal::Packets::ChannelMessage_Struct *>(msg_data);
  if (msg && msg->chan_num == kGroupTextChannel) zeal->chat_hook->handle_incoming_gsay(msg->message);
  if (msg && msg->chan_num == kRaidTextChannel) zeal->chat_hook->handle_incoming_rsay(msg->message);

  zeal->hooks->hook_map["MsgNewText"]->original(msg_new_text)(msg_data);
}

Chat::Chat(ZealService *zeal) {
  // zeal->callbacks->add_packet([this](UINT opcode, char* buffer, UINT size) {

  //
  //    if (opcode == 0x4058 || opcode == 0x404A)
  //    {
  //        Zeal::Game::print_chat("Opcode: 0x%x Size: %i Buffer: %s", opcode, size, Zeal::String::bytes_to_hex(buffer,
  //        size).c_str());
  //    }
  // if (opcode == 0x4236)
  //{
  //    Zeal::Game::print_chat("Opcode: 0x%x Size: %i Buffer: %s", opcode, size, Zeal::String::bytes_to_hex(buffer,
  //    size).c_str());
  //}
  //
  // return false;
  //}, callback_type::WorldMessage);
  zeal->commands_hook->Add(
      "/abbreviatedchat", {"/abc"}, "Abbreviates chat messages.", [this](std::vector<std::string> &args) {
        // 0 = Off
        // 1 = Chat Only
        // 2 = Chat and Log
        if (args.size() > 1) {
          int abbreviated_option;
          if (Zeal::String::tryParse(args[1], &abbreviated_option)) {
            abbreviated_option = std::clamp(abbreviated_option, 0, 2);
            UseAbbreviatedChat.set(abbreviated_option);
          }

        } else {
          UseAbbreviatedChat.set(UseAbbreviatedChat.get() > 0 ? 0 : 1);
        }
        Zeal::Game::print_chat(
            "Abbreviated chat set to %d (%s)", UseAbbreviatedChat.get(),
            UseAbbreviatedChat.get() == 0 ? "Off" : (UseAbbreviatedChat.get() == 1 ? "Chat only" : "Chat and Log"));
        return true;  // return true to stop the game from processing any further on this command,
                      // false if you want to just add features to an existing cmd
      });
  zeal->commands_hook->Add(
      "/timestamp", {"/tms"}, "Toggles/sets timestamps on chat windows: 0:Off, 1:Long, 2:Short, 3:Short+Secs.",
      [this](std::vector<std::string> &args) {
        if (args.size() == 2) {
          int style = 0;
          if (Zeal::String::tryParse(args[1], &style, true) && style >= 0 && style <= 3) {
            TimeStampsStyle.set(style);
            Zeal::Game::print_chat("Timestamps style set to: %d", TimeStampsStyle.get());
            return true;
          }
        }
        Zeal::Game::print_chat(
            "Usage: /timestamp <style> where <style> = 0 (Off), 1 (Long), 2 (Short), 3 (Short+Secs)");
        return true;  // No existing cmd.
      });
  zeal->commands_hook->Add("/zealinput", {"/zinput"}, "Toggles zeal input which gives you a more modern input feel.",
                           [this](std::vector<std::string> &args) {
                             UseZealInput.toggle();
                             return true;  // return true to stop the game from processing any further on this command,
                                           // false if you want to just add features to an existing cmd
                           });
  zeal->commands_hook->Add("/classchatcolors", {"/clc"},
                           "Toggles class-colorization of names in chat. Uses colors set in raid window options.",
                           [this](std::vector<std::string> &args) {
                             UseClassChatColors.toggle();
                             if (UseClassChatColors.get()) {
                               Zeal::Game::print_chat("Class Chat Colors enabled");
                             } else {
                               Zeal::Game::print_chat("Class Chat Colors disabled");
                             }
                             return true;  // return true to stop the game from processing any further on this command,
                                           // false if you want to just add features to an existing cmd
                           });
  zeal->commands_hook->Add("/classicclasses", {"/cc"},
                           "Toggles classic classes only (no longer showing vicar for a 50+ cleric for example).",
                           [this](std::vector<std::string> &args) {
                             UseClassicClassNames.toggle();
                             if (UseClassicClassNames.get()) {
                               Zeal::Game::print_chat("Classic Class Names enabled");
                             } else {
                               Zeal::Game::print_chat("Classic Class Names disabled");
                             }
                             return true;  // return true to stop the game from processing any further on this command,
                                           // false if you want to just add features to an existing cmd
                           });
  zeal->commands_hook->Add("/bluecon", {}, "Toggles the custom color for blue con that you can adjust in options.",
                           [this](std::vector<std::string> &args) {
                             UseBlueCon.toggle();
                             return true;  // return true to stop the game from processing any further on this command,
                                           // false if you want to just add features to an existing cmd
                           });
  zeal->commands_hook->Add("/loc", {}, "Adds noprint arguments to /loc to not log the location to your chat.",
                           [this](std::vector<std::string> &args) {
                             if (args.size() > 1 && args[1] == "noprint") {
                               std::stringstream ss;
                               ss << "Your Location is " << std::fixed << std::setprecision(2)
                                  << std::ceil(Zeal::Game::get_self()->Position.x * 100) / 100 << ", "
                                  << std::ceil(Zeal::Game::get_self()->Position.y * 100) / 100 << ", "
                                  << std::ceil(Zeal::Game::get_self()->Position.z * 100) / 100;
                               std::string result = ss.str();
                               Zeal::Game::log(result);
                               return true;
                             }
                             return false;  // return true to stop the game from processing any further on this command,
                                            // false if you want to just add features to an existing cmd
                           });
  zeal->commands_hook->Add("/log", {}, "Toggles log on/off or adds something directly to your log",
                           [this, zeal](std::vector<std::string> &args) {
                             std::string full_str;
                             if (args.size() == 2 && (Zeal::String::compare_insensitive(args[1], "on") ||
                                                      Zeal::String::compare_insensitive(args[1], "off"))) {
                               return false;
                             }
                             if (args.size() > 1) {
                               for (size_t i = 1; i < args.size(); ++i) {
                                 if (i < args.size() - 1)
                                   full_str += args[i] + " ";
                                 else
                                   full_str += args[i];
                               }
                               Zeal::Game::DoPercentConvert(full_str);
                               Zeal::Game::log(full_str);
                               return true;
                             }
                             return false;
                           });
  zeal->commands_hook->Add(
      "/autoconsent", {}, "Toggles autoconsent response on/off", [this](std::vector<std::string> &args) {
        EnableAutoConsent.toggle();
        Zeal::Game::print_chat("Auto-consent response is %s", EnableAutoConsent.get() ? "ON" : "OFF");
        return true;
      });

  zeal->commands_hook->Add("/optchat", {}, "Optionally broadcasts to raid or group if in one",
                           [this](std::vector<std::string> &args) {
                             handle_opt_chat(args);
                             return true;
                           });

  /*  zeal->commands_hook->Add("/uniquenaming", {}, "Toggles off the stripping of mob id and other identifiers from name
     of npc's (log only)", [this, ini](std::vector<std::string>& args) { uniquenames = !uniquenames;
            Zeal::Game::print_chat("Unique naming is %s", uniquenames ? "Enabled" : "Disabled");
            return true;
        });*/

  // zeal->hooks->Add("StripName", 0x529A8b, StripName, hook_type_replace_call);
  // zeal->hooks->Add("StripName1", 0x529B1D, StripName, hook_type_replace_call);
  // zeal->hooks->Add("StripName2", 0x529b6d, StripName, hook_type_replace_call);
  // zeal->hooks->Add("StripName3", 0x529b89, StripName, hook_type_replace_call);
  // zeal->hooks->Add("StripName4", 0x529A55, StripName, hook_type_replace_call);
  // zeal->hooks->Add("StripName5", 0x4EDBE5, StripName, hook_type_replace_call); //killed msg
  // zeal->hooks->Add("StripName6", 0x4EDBDA, StripName, hook_type_replace_call);//killed msg
  // zeal->hooks->Add("StripName7", 0x529371, StripName, hook_type_replace_call);//killed msg
  // zeal->hooks->Add("StripName8", 0x52933D, StripName, hook_type_replace_call);//killed msg
  // zeal->hooks->Add("StripName9", 0x5293EB, StripName, hook_type_replace_call);//killed msg
  // zeal->hooks->Add("StripName10", 0x529407, StripName, hook_type_replace_call);//killed msg
  // zeal->hooks->Add("StripName11", 0x529423, StripName, hook_type_replace_call);//killed msg
  // zeal->hooks->Add("StripName12", 0x5293CF, StripName, hook_type_replace_call);//killed msg
  // zeal->hooks->Add("StripName13", 0x5293B3, StripName, hook_type_replace_call);//killed msg
  // zeal->hooks->Add("StripName14", 0x5293A6, StripName, hook_type_replace_call);//killed msg
  zeal->hooks->Add("DoPercentConvert", 0x538110, DoPercentConvert,
                   hook_type_detour);                                    // add extra prints for new loot types
  zeal->hooks->Add("PrintChat", 0x537f99, PrintChat, hook_type_detour);  // add extra prints for new loot types
  zeal->hooks->Add("EditWndHandleKey", 0x5A3010, EditWndHandleKey,
                   hook_type_detour);  // this makes more sense than the hook I had previously
  zeal->hooks->Add("RezConfirmationDialogActivate", 0x004e206a, RezConfirmationDialogActivate,
                   hook_type_replace_call);  // add rez dialog to chat

  // My function for getting instruction length was failing on this function, couldn't be bothered to look into it too
  // deeply atm so just replaced all the calls to it
  zeal->hooks->Add("GetRGBAFromIndex", 0x406b02, GetRGBAFromIndex,
                   hook_type_replace_call);  // this is for modifying blue con color everywhere including chat
  zeal->hooks->Add("GetRGBAFromIndex1", 0x406b12, GetRGBAFromIndex, hook_type_replace_call);
  zeal->hooks->Add("GetRGBAFromIndex2", 0x406cdf, GetRGBAFromIndex, hook_type_replace_call);
  zeal->hooks->Add("GetRGBAFromIndex3", 0x407d90, GetRGBAFromIndex, hook_type_replace_call);
  zeal->hooks->Add("GetRGBAFromIndex4", 0x407da2, GetRGBAFromIndex, hook_type_replace_call);
  zeal->hooks->Add("GetRGBAFromIndex5", 0x4139eb, GetRGBAFromIndex, hook_type_replace_call);
  zeal->hooks->Add("GetRGBAFromIndex6", 0x438719, GetRGBAFromIndex, hook_type_replace_call);
  InitPercentReplacements();

  // Hook incoming text messages (raid, gsay, chat) to intercept messages.
  zeal->hooks->Add("MsgNewText", 0x004e25a1, msg_new_text, hook_type_detour);
  zeal->hooks->Add("chatPrintChat", 0x00524ca2, chatPrintChat, hook_type_replace_call);

  // Disable the cycle reply forwards and backwards if ZealInput enabled
  zeal->binds_hook->replace_cmd(
      0x3c, [this](int state) { return ZealService::get_instance()->chat_hook->UseZealInput.get(); });
  zeal->binds_hook->replace_cmd(
      0x3d, [this](int state) { return ZealService::get_instance()->chat_hook->UseZealInput.get(); });

  // Callbacks
  zeal->callbacks->AddOutputText([this](Zeal::GameUI::ChatWnd *&wnd, std::string &msg, short &channel) {
    this->AddOutputText(wnd, msg, channel);
  });
}

void Chat::set_classes() {
  if (UseClassicClassNames.get()) {
    mem::write<BYTE>(0x4bc090, 66);  // this just changes the if statement to check if the player is > 65 rather than >
                                     // 50
  } else {
    mem::write<BYTE>(0x4bc090, 51);
  }
}

Chat::~Chat() {}

void Chat::handle_opt_chat(std::vector<std::string> &args) {
  if (args.size() > 2) {
    bool rsay = (args[1] == "rs" || args[1] == "rsgs");
    bool gsay = (args[1] == "rsgs" || args[1] == "gs");
    bool do_rsay = rsay && Zeal::Game::RaidInfo->is_in_raid();
    bool do_gsay = !do_rsay && gsay && Zeal::Game::GroupInfo->is_in_group();
    if (do_rsay || do_gsay) {
      std::string message = args[2];
      for (int i = 3; i < args.size(); ++i) message += " " + args[i];
      if (do_rsay)
        Zeal::Game::send_raid_chat(message);
      else if (do_gsay)
        Zeal::Game::do_gsay(message);
    }
    if (rsay || gsay) return;
  }

  Zeal::Game::print_chat("Usage: /optchat <rs | gs | rsgs> <message>");
}

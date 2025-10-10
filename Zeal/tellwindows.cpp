#include "tellwindows.h"

#include <regex>

#include "binds.h"
#include "callbacks.h"
#include "chat.h"
#include "commands.h"
#include "game_addresses.h"
#include "game_functions.h"
#include "hook_wrapper.h"
#include "io_ini.h"
#include "memory.h"
#include "string_util.h"
#include "zeal.h"

// people will see this commit and be like OMG, then they will see this comment and message the discord channel.
// such hopes and dreams -- its on the list!
std::string TellWindowIdentifier = " ";

// std::pair<std::string, std::size_t> getName(const std::string& message)
//{
//     std::regex tells_pattern(R"((?:^|\]\s*)(\b\w+\b)\s+tells\s+you)");
//     std::regex told_pattern(R"((?:^|\]\s*)You\s+told\s+(\b\w+\b))");
//     std::regex say_pattern(R"((?:^|\]\s*)(\b\w+\b)\s+says,)");
//     std::regex ooc_pattern(R"((?:^|\]\s*)(\b\w+\b)\s+says\s+out\s+of\s+character,)");
//     std::regex auction_pattern(R"((?:^|\]\s*)(\b\w+\b)\s+auctions,)");
//
//     std::smatch match;
//     if (std::regex_search(message, match, tells_pattern)) {
//         return { match[1].str(), match.position(1) };  // Return the matched single word and starting index
//     }
//     else if (std::regex_search(message, match, told_pattern)) {
//         return { match[1].str(), match.position(1) };  // Return the matched single word and starting index
//     }
//     else if (std::regex_search(message, match, say_pattern)) {
//         return { match[1].str(), match.position(1) };  // Return the matched single word and starting index
//     }
//     else if (std::regex_search(message, match, ooc_pattern)) {
//         return { match[1].str(), match.position(1) };  // Return the matched single word and starting index
//     }
//     else if (std::regex_search(message, match, auction_pattern)) {
//         return { match[1].str(), match.position(1) };  // Return the matched single word and starting index
//     }
//     return { "", std::string::npos };  // Return empty string and invalid index if no match found
// }
//
// void replaceNameLinks(std::string& message)
//{
//     std::pair<std::string, std::size_t> name = getName(message);
//     if (name.second != std::string::npos)
//     {
//         Zeal::String::replace(message, name.first, "99998  " + name.first + "");
//     }
// }

void RestoreWindowState(Zeal::GameUI::ChatWnd *wnd) {
  while (wnd->ParentWnd) {
    wnd = (Zeal::GameUI::ChatWnd *)wnd->ParentWnd;
  }
  if (wnd->IsMinimized) wnd->MinimizeToggle();
}

Zeal::GameUI::ChatWnd *__fastcall GetActiveChatWindow(Zeal::GameUI::CChatManager *cm, int unused) {
  // fix up a little bit so the active window for things like linking items don't go to your always chat here if you are
  // using tell windows
  if (ZealService::get_instance()->tells && ZealService::get_instance()->tells->setting_enabled.get()) {
    Zeal::GameUI::ChatWnd *wnd = cm->ChatWindows[Zeal::Game::Windows->ChatManager->ActiveChatWnd];
    if (ZealService::get_instance()->tells->IsTellWindow(wnd)) return wnd;
  }
  return ZealService::get_instance()->hooks->hook_map["GetActiveChatWindow"]->original(GetActiveChatWindow)(cm, unused);
}

Zeal::GameUI::ChatWnd *TellWindows::FindPreviousTellWnd() {
  Zeal::GameUI::ChatWnd *wnd = nullptr;
  if (Zeal::Game::Windows->ChatManager->ActiveChatWnd > 0) {
    for (int i = Zeal::Game::Windows->ChatManager->ActiveChatWnd - 1;
         i != Zeal::Game::Windows->ChatManager->ActiveChatWnd; i--) {
      if (i <= -1) i = Zeal::Game::Windows->ChatManager->MaxChatWindows;

      wnd = Zeal::Game::Windows->ChatManager->ChatWindows[i];
      if (IsTellWindow(wnd)) return wnd;
    }
  }
  return nullptr;
}

Zeal::GameUI::ChatWnd *TellWindows::FindNextTellWnd() {
  Zeal::GameUI::ChatWnd *wnd = nullptr;

  for (int i = Zeal::Game::Windows->ChatManager->ActiveChatWnd + 1;
       i != Zeal::Game::Windows->ChatManager->ActiveChatWnd; i++) {
    if (i == Zeal::Game::Windows->ChatManager->MaxChatWindows) i = 0;

    wnd = Zeal::Game::Windows->ChatManager->ChatWindows[i];
    if (IsTellWindow(wnd)) return wnd;
  }
  return nullptr;
}

bool TellWindows::HandleKeyPress(int key, bool down, int modifier) {
  if (!setting_enabled.get() || !Zeal::Game::Windows || !Zeal::Game::Windows->ChatManager) return false;
  Zeal::GameUI::ChatWnd *wnd =
      Zeal::Game::Windows->ChatManager->ChatWindows[Zeal::Game::Windows->ChatManager->ActiveChatWnd];
  if (IsTellWindow(wnd)) {
    if (key == 0xf && down)  // tab and shift tab to cycle through tell windows
    {
      Zeal::GameUI::ChatWnd *focus_wnd = nullptr;
      if (modifier == 1)
        focus_wnd = FindPreviousTellWnd();
      else
        focus_wnd = FindNextTellWnd();
      if (focus_wnd && focus_wnd->edit) {
        RestoreWindowState(focus_wnd);
        focus_wnd->edit->SetFocus();
        return true;
      }
    }
  }
  return false;
}

std::string TellWindows::GetTellWindowName() const {
  if (!setting_enabled.get()) return "";
  if (Zeal::Game::Windows && Zeal::Game::Windows->ChatManager) {
    Zeal::GameUI::ChatWnd *wnd =
        Zeal::Game::Windows->ChatManager->ChatWindows[Zeal::Game::Windows->ChatManager->ActiveChatWnd];
    if (IsTellWindow(wnd)) {
      std::string window_title = std::string(wnd->Text);
      return window_title.substr(1, window_title.length() - 1);
    }
  }
  return "";
}

Zeal::GameUI::ChatWnd *TellWindows::FindTellWnd(std::string &name) {
  for (int i = 0; i < Zeal::Game::Windows->ChatManager->MaxChatWindows; i++) {
    Zeal::GameUI::ChatWnd *cwnd = Zeal::Game::Windows->ChatManager->ChatWindows[i];
    if (cwnd && cwnd->Text.Data) {
      std::string title = std::string(cwnd->Text);
      if (IsTellWindow(cwnd) && Zeal::String::compare_insensitive(title.substr(1, title.length() - 1), name))
        return cwnd;
    }
  }
  return nullptr;
}

std::string abbreviateTell(const std::string &original_message) {
  std::regex normal_tell_pattern(R"(^(\b\w+\b) (tells|told) (\b\w+\b),? '(.*)'$)");
  std::regex abbreviated_tell_pattern(R"(^(\[[\d\w: ]+\]\s+)?((?:<<|&LT;&LT;|>>|&RT;&RT;))\s+\[(\b\w+\b)\]:\s+(.*)$)");
  std::smatch tell_match;
  std::string sender;
  std::string message;
  std::string abbreviated_message;
  if (std::regex_search(original_message, tell_match, normal_tell_pattern)) {
    message = tell_match[4].str();
    if (tell_match[2].str() == "told") {
      sender = "You";
    } else {
      sender = tell_match[1].str();
    }
    abbreviated_message = "[" + sender + "]: " + message;
  } else if (std::regex_search(original_message, tell_match, abbreviated_tell_pattern)) {
    std::string direction;
    if (tell_match.size() == 5) {
      direction = tell_match[2].str();
      sender    = tell_match[3].str();
      message   = tell_match[4].str();
    } else {
      direction = tell_match[1].str();
      sender    = tell_match[2].str();
      message   = tell_match[3].str();
    }
    if (direction == ">>" || direction == "&RT;&RT;") {
      sender = "You";
    }
    if (tell_match.size() == 5) {
      // Include timestamp if present
      abbreviated_message = tell_match[1].str() + "[" + sender + "]: " + message;
    } else {
      abbreviated_message = "[" + sender + "]: " + message;
    }
  } else {
    return original_message;  // Return the original message if it doesn't match the tell patterns
  }
  // replace "You" with actual character name in abbreviated_message
  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  if (self && sender == "You") {
    sender = self->Name;
    abbreviated_message = std::regex_replace(abbreviated_message, std::regex(R"(\[You\])"), "[" + sender + "]");
  }

  return (abbreviated_message);
}

std::string stripTags(const std::string &message) {
  // Regex pattern to match <a ...>...</a> tags and keep only the inner text
  std::regex tag_pattern(R"(<a\s+[^>]*>([^<]+)<\/a>)");

  // Replace the matched tags with the inner text
  return std::regex_replace(message, tag_pattern, "$1");
}

std::string GetName(std::string &data) {
  std::string lower_msg = stripTags(data);
  // std::transform(lower_msg.begin(), lower_msg.end(), lower_msg.begin(), ::tolower);

  // Regex pattern for matching exactly one word before "tells you"
  std::regex tells_pattern(R"(^(?:\[[\d\w: ]+\]\s+)?(?:<<\s+|&LT;&LT;\s+)?\[?(\b\w+\b)\]?(?:\s+tells\s+you|:\s+))");
  // Regex pattern for matching exactly one word after "you told"
  std::regex told_pattern(R"(^(?:\[[\d\w: ]+\]\s+)?(?:(?:>>|&RT;&RT;)\s+|You told\s+)\[?(\b\w+\b)\]?)");

  std::smatch match;

  // Check for "tells you" pattern with only one word before it
  if (std::regex_search(lower_msg, match, tells_pattern)) {
    return match[1].str();  // Return the matched single word before "tells you"
  }
  // Check for "you told" pattern with only one word after it
  else if (std::regex_search(lower_msg, match, told_pattern)) {
    if (match[2].matched) {
      return match[2].str();  // Return the matched single word after "you told"
    } else if (match[1].matched) {
      return match[1].str();  // Return the matched character in the abbreviated chat format
    }
  }

  return "";  // Return an empty string if no match found
}

void replaceNameLinks(std::string &message) {
  std::string name = GetName(message);
  
  // Don't replace your own name with a link
  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  if (self)
    if (Zeal::String::compare_insensitive(self->Name, name)) return;

  if (name.length()) {
    // Use regex to match only whole word occurrences of name
    std::regex word_regex("\\b" + name + "\\b");
    message = std::regex_replace(
        message, word_regex,
        "<a WndNotify=\"153," + name + "\">" + name + "</a>");
  }
}

void TellWindows::AddOutputText(Zeal::GameUI::ChatWnd *&wnd, std::string &msg, short channel) {
  if (!ZealService::get_instance()->tells->setting_enabled.get())  // just early out if tell windows are not enabled
    return;

  if (channel == USERCOLOR_TELL || channel == USERCOLOR_ECHO_TELL)  // tell channel
  {
    std::string name = GetName(msg);
    if (name.length()) {
      name[0] = std::toupper(name[0]);
      Zeal::GameUI::ChatWnd *tell_window = ZealService::get_instance()->tells->FindTellWnd(name);
      // Modify msg if abbreviated chat is enabled
      if (ZealService::get_instance()->chat_hook->UseAbbreviatedChat.get())
        msg = abbreviateTell(std::string(msg));
      replaceNameLinks(msg);
      if (!tell_window) {
        std::string WinName = TellWindowIdentifier + name;
        std::string ini_name = Zeal::Game::get_ui_ini_filename();
        int font_size = 3;
        if (ini_name.length()) {
          IO_ini ui_ini(ini_name);
          font_size = ui_ini.getValue<int>("ChatManager", "ChatWindow0_FontStyle");
          font_size = std::clamp(font_size, 0, 6);
        }
        Zeal::Game::Windows->ChatManager->CreateChatWindow(WinName.c_str(), 0, 3, -1, "", font_size);
        tell_window = ZealService::get_instance()->tells->FindTellWnd(name);
        wnd = tell_window;

        if (wnd && setting_hist_enabled.get() && tell_cache.count(name)) {
          for (auto &[color, hist_msg] : tell_cache[name])
            Zeal::Game::print_chat_wnd(wnd, color, "<c \"#666666\">%s</c>", hist_msg.c_str());
        }
      } else {
        wnd = tell_window;
        RestoreWindowState(wnd);
      }
      if (setting_hist_enabled.get()) tell_cache[name].push_back(std::make_pair(channel, msg));
    }
  }
}

bool TellWindows::IsTellWindow(Zeal::GameUI::ChatWnd *wnd) const {
  if (wnd && wnd->Text.Data) {
    std::string title = std::string(wnd->Text);
    if (title.substr(0, 1) == TellWindowIdentifier) {
      return true;
    }
  }
  return false;
}

void TellWindows::CloseAllWindows() {
  if (!Zeal::Game::Windows || !Zeal::Game::Windows->ChatManager || !setting_enabled.get()) return;
  for (int i = 0; i < Zeal::Game::Windows->ChatManager->MaxChatWindows; i++) {
    Zeal::GameUI::ChatWnd *cwnd = Zeal::Game::Windows->ChatManager->ChatWindows[i];
    if (cwnd && cwnd->IsVisible && IsTellWindow(cwnd))
      reinterpret_cast<void(__thiscall *)(const Zeal::GameUI::ChatWnd *)>(cwnd->vtbl->Deactivate)(cwnd);
  }
}

void TellWindows::CleanUI() {
  if (!Zeal::Game::Windows || !Zeal::Game::Windows->ChatManager || !setting_enabled.get()) return;
  for (int i = 0; i < Zeal::Game::Windows->ChatManager->MaxChatWindows; i++) {
    Zeal::GameUI::ChatWnd *cwnd = Zeal::Game::Windows->ChatManager->ChatWindows[i];
    if (IsTellWindow(cwnd)) cwnd->show(false, false);
  }
}

void __fastcall DeactivateChatManager(Zeal::GameUI::CChatManager *t, int u) {
  // toggle the tell windows to not load on next game load
  ZealService::get_instance()->hooks->hook_map["DeactivateChatManager"]->original(DeactivateChatManager)(t, u);
  std::string ini_name = Zeal::Game::get_ui_ini_filename();
  if (ini_name.length()) {
    IO_ini ini(ini_name);
    for (int i = 0; i < t->MaxChatWindows; i++) {
      if (ZealService::get_instance()->tells->IsTellWindow(t->ChatWindows[i])) {
        ini.setValue<std::string>("ChatManager", "ChatWindow" + std::to_string(i) + "_Language", "@42");
      }
    }
  }
}

int __fastcall ChatWndNotification(Zeal::GameUI::BasicWnd *wnd, int unused, Zeal::GameUI::BasicWnd *sender, int message,
                                   int data) {
  if (message == 0x99)  // Item link clicked
  {
    char *msg = (char *)data;
    Zeal::Game::do_target(msg);
    return 0;
  }
  return ZealService::get_instance()->hooks->hook_map["ChatWndNotification"]->original(ChatWndNotification)(
      wnd, unused, sender, message, data);
}

TellWindows::TellWindows(ZealService *zeal) {
  if (!Zeal::Game::is_new_ui()) return;  // Old UI not supported.

  zeal->hooks->Add(
      "GetActiveChatWindow", 0x425D27, GetActiveChatWindow,
      hook_type_replace_call);  // hook to fix item linking to tell windows if always chat here is selected anywhere
  zeal->hooks->Add("DeactivateChatManager", 0x410871, DeactivateChatManager, hook_type_detour);
  zeal->hooks->Add("ChatWndNotification", 0x413BE9, ChatWndNotification, hook_type_detour);
  // zeal->hooks->Add("DeactivateMainUI", 0x4a7705, DeactivateMainUI, hook_type_detour); //clean up tell windows just
  // before they save zeal->callbacks->AddGeneric([this]() { Deactivate_Window(); }, callback_type::DeactivateUI);
  zeal->callbacks->AddGeneric([this]() { CleanUI(); }, callback_type::CleanUI);
  zeal->callbacks->AddOutputText(
      [this](Zeal::GameUI::ChatWnd *&wnd, std::string &msg, short channel) { this->AddOutputText(wnd, msg, channel); });
  zeal->chat_hook->add_key_press_callback(
      [this](int key, bool down, int modifier) { return HandleKeyPress(key, down, modifier); });
  zeal->commands_hook->Add(
      "/rt", {}, "Target the last tell or active tell window player", [this](std::vector<std::string> &args) {
        if (Zeal::Game::Windows && Zeal::Game::Windows->Raid) {
          Zeal::GameUI::ListWnd *players =
              (Zeal::GameUI::ListWnd *)Zeal::Game::Windows->Raid->GetChildItem("RAID_PlayerList");
          if (players) {
            std::string reply_who = (char *)(0x7CE45C);
            if (setting_enabled.get()) {
              if (Zeal::Game::Windows && Zeal::Game::Windows->ChatManager) {
                Zeal::GameUI::ChatWnd *wnd =
                    Zeal::Game::Windows->ChatManager->ChatWindows[Zeal::Game::Windows->ChatManager->ActiveChatWnd];
                if (IsTellWindow(wnd)) {
                  std::string window_title = std::string(wnd->Text);
                  reply_who = window_title.substr(1, window_title.length() - 1);  // strip the tell window identifier
                }
              }
            }

            for (int i = 1; i < players->ItemCount; i++) {
              std::string name = players->GetItemText(i, 1);
              if (name == reply_who) {
                players->SelectedIndex = i;  // select player in raid window
                break;
              }
            }
          }
        }

        if (setting_enabled.get()) {
          if (Zeal::Game::Windows && Zeal::Game::Windows->ChatManager) {
            Zeal::GameUI::ChatWnd *wnd =
                Zeal::Game::Windows->ChatManager->ChatWindows[Zeal::Game::Windows->ChatManager->ActiveChatWnd];
            if (IsTellWindow(wnd)) {
              std::string window_title = std::string(wnd->Text);
              Zeal::Game::do_target(window_title.substr(1, window_title.length() - 1).c_str());
              return true;
            }
          }
        }
        return false;
      });
  zeal->commands_hook->Add("/tellwindows", {}, "Toggle tell windows", [this](std::vector<std::string> &args) {
    setting_enabled.toggle();
    if (setting_enabled.get())
      Zeal::Game::print_chat("Tell windows enabled.");
    else
      Zeal::Game::print_chat("Tell windows disabled.");

    if (update_options_ui_callback) update_options_ui_callback();
    return true;
  });

  zeal->binds_hook->replace_cmd(0x3B, [this](int state) {
    if (!setting_enabled.get()) return false;
    if (state && !Zeal::Game::GameInternal::UI_ChatInputCheck()) {
      int last_tell_index = *(int *)0x7cf0dc;
      char *reply_to = (char *)(0x7CE45C + (last_tell_index * 0x40));
      std::string reply_to_str = reply_to;
      Zeal::GameUI::ChatWnd *wnd = FindTellWnd(reply_to_str);
      if (wnd && wnd->edit) {
        RestoreWindowState(wnd);
        wnd->edit->SetFocus();
        return true;
      } else
        return false;
    }
    return false;
  });  // reply hotkey

  zeal->binds_hook->replace_cmd(0xCE, [this](int state) {
    if (!setting_enabled.get()) return false;

    if (state && !Zeal::Game::GameInternal::UI_ChatInputCheck()) {
      if (Zeal::Game::Windows->ChatManager->AlwaysChatHereIndex >= 0)
        Zeal::Game::Windows->ChatManager->ActiveChatWnd = Zeal::Game::Windows->ChatManager->AlwaysChatHereIndex;
    }
    return false;
  });  // chat hotkey

  zeal->commands_hook->add_get_tell_name_callback([this]() { return GetTellWindowName(); });
}

TellWindows::~TellWindows() {}
#include "page10_binds.h"

#include <cctype>
#include <cstring>

#include "callbacks.h"
#include "game_addresses.h"
#include "game_functions.h"
#include "game_structures.h"
#include "game_ui.h"
#include "io_ini.h"
#include "zeal.h"

static constexpr int kPage10Index = 9;           // Zero-based index of hotbar / social page 10.
static constexpr int kSocialLinesPerButton = 5;  // Command lines per social in the client.
static constexpr int kMaxPauseTicks = 600;       // Client caps social /pause values at 600 (60 seconds).
static constexpr UINT kCmdHotButton1 = 31;       // Client ExecuteCmd id of the HOT1 keybind.

Page10Binds::Page10Binds(ZealService *zeal) {
  zeal->callbacks->AddGeneric([this]() { process_queue(); });  // MainLoop.
  zeal->callbacks->AddGeneric([this]() { command_queue.clear(); }, callback_type::CharacterSelect);
  zeal->callbacks->AddGeneric([this]() { command_queue.clear(); }, callback_type::DeactivateUI);
}

// Presses the given page 10 hotbutton by temporarily swapping the active page around the
// client's ExecuteCmd call, leaving the visible hotbar page unchanged. Note: if the button
// holds a multi-line social, the client's native macro engine may continue it on later frames
// after the page has been restored, so lines after the first may not resolve to page 10.
void Page10Binds::press_hotbutton(int slot) {
  auto hotbutton_wnd = Zeal::Game::Windows->HotButton;
  if (!hotbutton_wnd) return;

  const int saved_page = hotbutton_wnd->GetPage();
  hotbutton_wnd->SetPage(kPage10Index);
  Zeal::Game::execute_cmd(kCmdHotButton1 + slot, 1, 0);  // Key down.
  Zeal::Game::execute_cmd(kCmdHotButton1 + slot, 0, 0);  // Key up.
  hotbutton_wnd->SetPage(saved_page);
}

// Extracts the command to execute from a social line, handling the client's native
// "/pause <ticks>[, <command>]" prefix syntax. Returns the command ("" if none) and stores the
// delay to apply after this line in pause_ticks (tenths of a second).
static std::string parse_social_line(std::string line, int &pause_ticks) {
  pause_ticks = 0;
  constexpr size_t kPauseLength = 6;  // strlen("/pause").
  if (line.size() >= kPauseLength && _strnicmp(line.c_str(), "/pause", kPauseLength) == 0 &&
      (line.size() == kPauseLength || !isalpha(static_cast<unsigned char>(line[kPauseLength])))) {
    pause_ticks = min(max(0, atoi(line.c_str() + kPauseLength)), kMaxPauseTicks);
    const size_t comma = line.find(',');
    line = (comma == std::string::npos) ? std::string() : line.substr(comma + 1);
  }
  line.erase(0, line.find_first_not_of(" \t"));  // Also erases all of a whitespace-only line.
  return line;
}

// Loads the social lines for the page 10 button (1-based) from the character ini and queues
// them for execution with their accumulated /pause delays. Replaces any pending page 10 social.
// Note: a macro started natively (hotbar / socials window) is not stopped; the client has no
// exposed call for that, and pressing a hotbutton while a macro runs is ignored by the client.
void Page10Binds::execute_social(int button) {
  const auto *char_info = Zeal::Game::get_char_info();
  if (!char_info) return;

  command_queue.clear();

  // The client stores per-character settings (including socials) next to the exe.
  const IO_ini ini(std::string(".\\") + char_info->Name + Zeal::Game::get_host_tag() + ".ini");
  ULONGLONG execute_time = GetTickCount64();
  for (int line = 1; line <= kSocialLinesPerButton; ++line) {
    char key[32];
    snprintf(key, sizeof(key), "Page10Button%dLine%d", button, line);
    int pause_ticks = 0;
    std::string command = parse_social_line(ini.getValue<std::string>("Socials", key), pause_ticks);
    if (!command.empty()) command_queue.push_back({std::move(command), execute_time});
    execute_time += static_cast<ULONGLONG>(pause_ticks) * 100;  // /pause is in tenths of a second.
  }
}

// Executes queued social lines once their /pause delay has elapsed. Routes through the client's
// InterpretCommand so lines behave exactly like typed commands (including Zeal commands).
void Page10Binds::process_queue() {
  if (command_queue.empty()) return;
  if (!Zeal::Game::is_in_game() || !Zeal::Game::get_self()) {
    command_queue.clear();
    return;
  }

  const ULONGLONG now = GetTickCount64();
  while (!command_queue.empty() && command_queue.front().execute_time <= now) {
    auto *self = Zeal::Game::get_self();  // Re-checked per command in case one tears down state.
    if (!Zeal::Game::is_in_game() || !self) {
      command_queue.clear();
      return;
    }
    const std::string command = std::move(command_queue.front().command);
    command_queue.pop_front();
    reinterpret_cast<void(__fastcall *)(Zeal::GameStructures::GameClass *, int, Zeal::GameStructures::Entity *,
                                        const char *)>(Zeal::Game::GameInternal::fn_interpretcmd)(
        Zeal::Game::get_game(), 0, self, command.c_str());
  }
}

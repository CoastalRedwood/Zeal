#include "triggers.h"

#include <algorithm>
#include <fstream>
#include <regex>

#include "callbacks.h"
#include "chat.h"
#include "commands.h"
#include "game_functions.h"
#include "string_util.h"
#include "zeal.h"

// TODO:
// - Look into tick synchronization option / specify in ticks.

Triggers::Triggers(ZealService *zeal) {
  zeal->chat_hook->add_print_chat_callback(
      [this](const char *data, int color_index) { HandlePrintChat(data, color_index); });
  zeal->callbacks->AddGeneric([this]() { CallbackRender(); }, callback_type::RenderUI);
  zeal->callbacks->AddGeneric([this]() { LoadTriggers("", false); }, callback_type::InitUI);
  zeal->callbacks->AddGeneric([this]() { Clean(); }, callback_type::EnterZone);
  zeal->callbacks->AddGeneric([this]() { Clean(); }, callback_type::CleanUI);  // Note: new_ui only call.
  zeal->callbacks->AddGeneric([this]() { Clean(); }, callback_type::DXReset);  // Just release all resources.
  zeal->callbacks->AddGeneric([this]() { Clean(); }, callback_type::DXCleanDevice);

  zeal->commands_hook->Add("/triggers", {}, "Controls log trigger parsing", [this](std::vector<std::string> &args) {
    ParseArgs(args);
    return true;
  });
}

Triggers::~Triggers() { Clean(); }

void Triggers::Clean() {
  bitmap_font.reset();  // Releases all DX and other resources.
  trigger_events.clear();
}

void Triggers::SynchronizeEnable(bool verbose) {
  triggers.clear();
  trigger_events.clear();
  if (!enabled.get()) return;

  LoadTriggers(triggers_filename.get(), verbose);
}

bool Triggers::LoadTriggers(const std::string &load_filename, bool verbose) {
  triggers.clear();
  std::string filename = load_filename;
  if (filename.empty()) {
    // Parse the default per user triggers file.
    auto char_info = Zeal::Game::get_char_info();
    if (!char_info) return false;
    filename = std::string(char_info->Name) + "-Triggers.txt";
  }

  std::filesystem::path file_path = Zeal::Game::get_game_path() / std::filesystem::path(filename);
  if (!std::filesystem::exists(file_path)) {
    if (verbose) Zeal::Game::print_chat("Failed to load trigger file: %s", file_path.string().c_str());
    return false;
  }

  std::ifstream input_file(file_path);
  if (!input_file.is_open()) {
    if (verbose) Zeal::Game::print_chat("Error opening protect file: %s", file_path.string().c_str());
    return false;
  }

  std::string line;
  int error_count = 0;
  while (std::getline(input_file, line)) {
    if (line.empty()) continue;  // Ignore blank lines.
    if (!AddTrigger(line)) {
      if (verbose) Zeal::Game::print_chat("Zeal Triggers error parsing line: %s", line.c_str());
      return false;  // Bail out.
    }
  }
  if (verbose) Zeal::Game::print_chat("Zeal loaded %d triggers", triggers.size());
  return true;
}

bool Triggers::AddTrigger(const std::string &line) {
  auto fields = Zeal::String::split_text(line, "^");
  if (fields.size() != 5) return false;

  Action action = Action::Clear;
  if (fields[0] == "Add")
    action = Action::Add;
  else if (fields[0] != "Clear")
    return false;

  int duration_sec = 0;
  if (!Zeal::String::tryParse(fields[3], &duration_sec)) return false;
  if (duration_sec < 0 || duration_sec > 10 * 3600) return false;  // Failed duration sanity check of up to 10 hours.

  DWORD color = 0;
  try {
    color = std::stoul(fields[4], nullptr, 0);  // Hex conversion
  } catch (const std::exception &e) {
    return false;
  }

  Trigger trigger = {.action = action,
                     .label = fields[1],
                     .pattern_str = fields[2],
                     .pattern = std::regex(fields[2]),
                     .duration_sec = static_cast<DWORD>(duration_sec),
                     .color = color};
  triggers.push_back(trigger);
  return true;
}

std::string Triggers::GetTriggerDescription(const Trigger &trigger) const {
  switch (trigger.action) {
    case Action::Clear:
      return std::format("Clear {}", trigger.label);
    case Action::Add:
      return std::format("Add {}: \"{}\" {} secs, Color: {:#08x}", trigger.label, trigger.pattern_str,
                         trigger.duration_sec, trigger.color);
  }
  return "Unrecognized trigger";
}

void Triggers::ParseArgs(const std::vector<std::string> &args) {
  if (args.size() == 2 && (args[1] == "on" || args[1] == "off")) {
    enabled.set(args[1] == "on");
    Zeal::Game::print_chat("Triggers are %s", enabled.get() ? "on" : "off");
    return;
  }

  if ((args.size() == 2 || args.size() == 3) && args[1] == "load") {
    if (!enabled.get()) {
      Zeal::Game::print_chat("Triggers are disabled");
      return;
    }
    std::string filename = (args.size() == 3) ? args[2] : "";
    if (LoadTriggers(filename, true) && filename != triggers_filename.get()) {
      triggers_filename.set(filename);
      if (filename == "") filename = "(default)";
      Zeal::Game::print_chat("Loaded %d triggers from file: %s", triggers.size(), filename.c_str());
    }
    return;
  }

  if (args.size() == 2 && args[1] == "clear") {
    trigger_events.clear();
    Zeal::Game::print_chat("Cleared active trigger events");
    return;
  }

  if (args.size() == 2 && args[1] == "list") {
    if (!enabled.get()) {
      Zeal::Game::print_chat("Triggers are disabled");
      return;
    }
    if (triggers.empty()) {
      Zeal::Game::print_chat("No triggers are loaded");
      return;
    }
    Zeal::Game::print_chat("Triggers:");
    for (int i = 0; i < triggers.size(); ++i) {
      Zeal::Game::print_chat("[%d]: %s", i, GetTriggerDescription(triggers[i]).c_str());
    }

    if (trigger_events.empty()) return;

    Zeal::Game::print_chat("Active events:");
    for (const auto &event : trigger_events) {
      Zeal::Game::print_chat("%s: %d", event.label.c_str(), event.end_timestamp_ms);
    }
    return;
  }

  if (args.size() == 3 && args[1] == "font") {
    bitmap_font_filename.set(args[2]);
    Zeal::Game::print_chat("Font filename set to %s", bitmap_font_filename.get().c_str());
    return;
  }

  if (args.size() == 4 && args[1] == "position") {
    int x, y;
    if (Zeal::String::tryParse(args[2], &x) && Zeal::String::tryParse(args[3], &y)) {
      position_x.set(x);
      position_y.set(y);
      Zeal::Game::print_chat("Trigger list position set to (%d, %d)", x, y);
      return;
    }

    Zeal::Game::print_chat("Usage: /triggers position <x> <y> where (x,y) is the upper left of list");
    return;
  }

  Zeal::Game::print_chat("Usage: /triggers <on | off>, list, clear");
  Zeal::Game::print_chat("Usage: /triggers load [filename] (blank filename = load per user default)");
  Zeal::Game::print_chat("Usage: /triggers font font_filename");
  Zeal::Game::print_chat("Usage: /triggers position <x> <y> where (x,y) is the upper left of list");
}

void Triggers::HandlePrintChat(const char *data, int color_index) {
  if (!enabled.get()) return;

  for (const auto &trigger : triggers) {
    if (std::regex_match(data, trigger.pattern)) {
      ActivateTrigger(trigger);
      return;  // Only activates the first trigger.
    }
  }
}

void Triggers::ActivateTrigger(const Trigger &trigger) {
  if (trigger.action == Action::Clear) {
    std::erase_if(trigger_events, [trigger](const TriggerEvent &t) { return t.label == trigger.label; });
    return;
  }

  if (trigger.action != Action::Add) return;  // Unknown, ignore.

  auto display = Zeal::Game::get_display();
  if (!display) return;  // Unlikely but protect against.

  DWORD end_timestamp_ms = display->GameTimeMs + trigger.duration_sec * 1000;
  TriggerEvent event = {.label = trigger.label, .color = trigger.color, .end_timestamp_ms = end_timestamp_ms};
  trigger_events.push_back(event);
}

// Loads the bitmap font for real-time text rendering to screen.
void Triggers::LoadBitmapFont() {
  if (bitmap_font || bitmap_font_filename.get().empty()) return;

  IDirect3DDevice8 *device = ZealService::get_instance()->dx->GetDevice();
  std::string font_filename = bitmap_font_filename.get();
  bool is_default_font = (font_filename.empty() || font_filename == kUseDefaultFont);
  if (is_default_font) font_filename = kDefaultTriggerFont;
  if (device != nullptr) bitmap_font = BitmapFont::create_bitmap_font(*device, font_filename);
  if (!bitmap_font) {
    Zeal::Game::print_chat("Failed to load font: %s", font_filename.c_str());
    if (is_default_font) {
      Zeal::Game::print_chat("Disabling triggers due to font issue");
      enabled.set(false);
    } else {
      bitmap_font_filename.set(kUseDefaultFont);  // Try again with default next round.
    }
    return;
  }

  bitmap_font->set_drop_shadow(true);  // TODO
}

void Triggers::CallbackRender() {
  if (!enabled.get() || trigger_events.empty() || !Zeal::Game::is_in_game() || !Zeal::Game::is_gui_visible()) return;

  auto display = Zeal::Game::get_display();
  if (!display) return;

  LoadBitmapFont();
  if (!bitmap_font) return;

  // First remove any expired trigger events.
  DWORD current_time_ms = display->GameTimeMs;
  std::erase_if(trigger_events,
                [current_time_ms](const TriggerEvent &trigger) { return trigger.end_timestamp_ms <= current_time_ms; });
  if (trigger_events.empty()) return;

  Vec2 screen_size = ZealService::get_instance()->dx->GetScreenRect();
  float x = static_cast<float>(position_x.get());
  float y = static_cast<float>(position_y.get());
  for (const auto &event : trigger_events) {
    int seconds = (event.end_timestamp_ms - current_time_ms) / 1000;
    int hours = seconds / 3600;
    seconds = seconds - hours * 3600;
    int minutes = seconds / 60;
    seconds = seconds - minutes * 60;
    std::string label = std::format("{:02d}:{:02d}:{:02d} {}", hours, minutes, seconds, event.label);
    bitmap_font->queue_string(label.c_str(), Vec3(x, y, 0), false, event.color, true);
    y += bitmap_font->get_line_spacing();
  }

  bitmap_font->flush_queue_to_screen();
}
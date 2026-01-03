#pragma once
#include <Windows.h>

#include <regex>
#include <string>
#include <vector>

#include "bitmap_font.h"
#include "zeal_settings.h"

class Triggers {
 public:
  static constexpr char kUseDefaultFont[] = "Default";
  static constexpr char kDefaultTriggerFont[] = "arial_12";

  explicit Triggers(class ZealService *zeal);
  ~Triggers();

  // Disable copy.
  Triggers(Triggers const &) = delete;
  Triggers &operator=(Triggers const &) = delete;

  ZealSetting<bool> enabled = {false, "Triggers", "Enabled", true, [this](bool val) { SynchronizeEnable(); }};
  ZealSetting<int> position_x = {100, "Triggers", "PositionX", true};
  ZealSetting<int> position_y = {100, "Triggers", "PositionY", true};
  ZealSetting<std::string> triggers_filename = {std::string(), "Triggers", "Filename", true};
  ZealSetting<std::string> bitmap_font_filename = {std::string(kUseDefaultFont), "Triggers", "Font", true,
                                                   [this](std::string val) { bitmap_font.reset(); }};

 private:
  enum class Action { Clear = 0, Add = 1 };

  struct Trigger {
    Action action;            // Action to perform when there is a match.
    std::string label;        // Screen label for a visible trigger.
    std::string pattern_str;  // Original string used to generate regex pattern.
    std::regex pattern;       // Pattern to match to activate trigger.
    DWORD duration_sec;       // Countdown duration in seconds.
    D3DCOLOR color;           // Color of text.
  };

  struct TriggerEvent {
    std::string label;       // Copied from Trigger.
    D3DCOLOR color;          // Copied from Trigger.
    DWORD end_timestamp_ms;  // Set by activation time + duration.
  };

  void Clean();                                  // Resets state and releases all resources.
  void SynchronizeEnable(bool verbose = false);  // Loads triggers from file if enabled.
  bool LoadTriggers(const std::string &triggers_filename, bool verbose);
  void ParseArgs(const std::vector<std::string> &args);
  bool AddTrigger(const std::string &line);
  void LoadBitmapFont();  // Loads the bitmap font for rendering.
  std::string GetTriggerDescription(const Trigger &trigger) const;

  void HandlePrintChat(const char *data, int color_index);  // Scans chat text for matches.
  void ActivateTrigger(const Trigger &trigger);             // Executed when there is a match.
  void CallbackRender();                                    // Displays visible trigger list.

  std::unique_ptr<BitmapFont> bitmap_font = nullptr;
  std::vector<Trigger> triggers;
  std::vector<TriggerEvent> trigger_events;
};

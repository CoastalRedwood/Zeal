#pragma once
#include <Windows.h>

#include <array>
#include <string>
#include <vector>

#include "bitmap_font.h"
#include "game_structures.h"
#include "zeal_settings.h"

class RaidBars {
 public:
  static constexpr char kUseDefaultFont[] = "Default";
  static constexpr char kDefaultFont[] = "arial_08";

  explicit RaidBars(class ZealService *zeal);
  ~RaidBars();

  // Disable copy.
  RaidBars(RaidBars const &) = delete;
  RaidBars &operator=(RaidBars const &) = delete;

  ZealSetting<bool> setting_enabled = {false, "RaidBars", "Enabled", false, [this](bool) { Clean(); }};
  ZealSetting<bool> setting_clickable = {false, "RaidBars", "Clickable", false};
  ZealSetting<int> setting_position_left = {5, "RaidBars", "Left", false};
  ZealSetting<int> setting_position_top = {5, "RaidBars", "Top", false};
  ZealSetting<int> setting_position_right = {0, "RaidBars", "Right", false};
  ZealSetting<int> setting_position_bottom = {0, "RaidBars", "Bottom", false};
  ZealSetting<int> setting_bar_width = {0, "RaidBars", "BarWidth", false, [this](int) { Clean(); }};
  ZealSetting<int> setting_bar_height = {0, "RaidBars", "BarHeight", false, [this](int) { Clean(); }};
  ZealSetting<bool> setting_show_all = {false, "RaidBars", "ShowAll", false};
  ZealSetting<std::string> setting_class_priority = {std::string(), "RaidBars", "ClassPriority", false,
                                                     [this](const std::string &) { SyncClassPriority(); }};
  ZealSetting<std::string> setting_class_always = {std::string(), "RaidBars", "ClassAlways", false,
                                                   [this](const std::string &) { SyncClassAlways(); }};
  ZealSetting<std::string> setting_bitmap_font_filename = {std::string(kUseDefaultFont), "RaidBars", "Font", false,
                                                           [this](std::string val) { bitmap_font.reset(); }};

  // Internal callback use only.
  bool HandleLMouseUp(short x, short y);

 private:
  static constexpr int kNumClasses = Zeal::GameEnums::ClassTypes::Beastlord - Zeal::GameEnums::ClassTypes::Warrior + 1;
  static constexpr int kClassIndexOffset = Zeal::GameEnums::ClassTypes::Warrior;

  struct RaidMember {
    std::string name;                      // Copy to compare against when out of zone.
    Zeal::GameStructures::Entity *entity;  // Set to nullptr when out of zone.
  };

  void Clean();  // Resets state and releases all resources.
  void ParseArgs(const std::vector<std::string> &args);
  void LoadBitmapFont();  // Loads the bitmap font for rendering.
  void CallbackRender();  // Displays raid bars.
  void SyncClassPriority();
  void SyncClassAlways();
  void DumpClassSettings() const;
  void UpdateRaidMembers();

  DWORD next_update_game_time_ms = 0;
  std::unique_ptr<BitmapFont> bitmap_font = nullptr;
  float grid_height = 0;
  float grid_width = 0;
  int grid_height_count_max = 0;  // Maximum number of bars that will fit in a column.

  std::array<std::vector<RaidMember>, kNumClasses> raid_classes;  // Per class vectors of raid members.
  std::array<int, kNumClasses> class_priority;                    // Prioritization order for class types.
  std::array<bool, kNumClasses> class_always;                     // Boolean flag to show always for class types.
  std::vector<Zeal::GameStructures::Entity *> visible_list;       // List of visible names (for clicking).
};

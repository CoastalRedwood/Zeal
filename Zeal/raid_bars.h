#pragma once
#include <Windows.h>

#include <array>
#include <string>
#include <vector>

#include "bitmap_font.h"
#include "game_structures.h"
#include "raid_bars_manage.h"
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
  ZealSetting<bool> setting_group_sort = {false, "RaidBars", "GroupSort", false};
  ZealSetting<int> setting_show_threshold = {100, "RaidBars", "ShowThreshold", false};
  ZealSetting<int> setting_background_alpha = {0, "RaidBars", "BackgroundAlpha", false};
  ZealSetting<std::string> setting_class_priority = {std::string(), "RaidBars", "ClassPriority", false,
                                                     [this](const std::string &) { SyncClassPriority(); }};
  ZealSetting<std::string> setting_class_always = {std::string(), "RaidBars", "ClassAlways", false,
                                                   [this](const std::string &) { SyncClassAlways(); }};
  ZealSetting<std::string> setting_class_never = {std::string(), "RaidBars", "ClassNever", false,
                                                  [this](const std::string &) { SyncClassNever(); }};
  ZealSetting<std::string> setting_class_filter = {std::string(), "RaidBars", "ClassFilter", false,
                                                   [this](const std::string &) { SyncClassFilter(); }};
  ZealSetting<std::string> setting_bitmap_font_filename = {std::string(kUseDefaultFont), "RaidBars", "Font", false,
                                                           [this](std::string val) { bitmap_font.reset(); }};

  // Internal callback use only.
  bool HandleLMouseUp(short x, short y);

 private:
  friend class RaidBarsManage;  // Allow manage class to access internals.

  static constexpr int kNumClasses = Zeal::GameEnums::ClassTypes::Beastlord - Zeal::GameEnums::ClassTypes::Warrior + 1;
  static constexpr int kClassIndexOffset = Zeal::GameEnums::ClassTypes::Warrior;
  static constexpr int kNumGroupLabelSlots = 13;  // Groups 1-12 plus ungrouped.

  struct RaidMember {
    std::string name;                      // Copy to compare against when out of zone.
    Zeal::GameStructures::Entity *entity;  // Set to nullptr when out of zone.
    D3DCOLOR color;                        // Class color.
    unsigned long group_number;            // Group number within raid.
    bool is_group_leader;                  // Lead of the group.
  };

  void Clean();  // Resets state and releases all resources.
  void ParseArgs(const std::vector<std::string> &args);
  bool HandleSetGrid(int num_rows, int num_cols);
  void LoadBitmapFont();  // Loads the bitmap font for rendering.
  void CallbackRender();  // Displays raid bars.
  void SyncClassPriority();
  void SyncClassAlways();
  void SyncClassNever();
  void SyncClassFilter();
  void DumpClassSettings() const;
  void UpdateRaidMembers();
  void QueueByClass(const float x_min, const float y_min, const float x_max, const float y_max);
  void QueueByGroup(const float x_min, const float y_min, const float x_max, const float y_max);

  // Returns the visible_list index for screen coordinates, or -1 if out of bounds.
  int CalcClickIndex(short x, short y) const;

  DWORD next_update_game_time_ms = 0;
  bool raid_update_dirty = false;  // Set true by OP_RaidUpdate packet to skip the 1s delay.
  std::unique_ptr<BitmapFont> bitmap_font = nullptr;
  float grid_height = 0;
  float grid_width = 0;
  int grid_height_count_max = 0;  // Maximum number of bars that will fit in a column.

  std::array<std::vector<RaidMember>, kNumClasses> raid_classes;  // Per class vectors of raid members.
  std::array<int, kNumClasses> class_priority;                    // Prioritization order for class types.
  std::array<bool, kNumClasses> class_always;                     // Boolean flag to show always for class types.
  std::array<bool, kNumClasses> class_never;                      // Boolean flag to show never for class types.
  std::array<bool, kNumClasses> class_filter;                     // Boolean flag to filter class types by threshold.
  std::vector<Zeal::GameStructures::Entity *> visible_list;       // List of visible names (for clicking).

  // Maps group slot (12 groups plus ungrouped) to its visible list label index.
  std::array<int, kNumGroupLabelSlots> visible_group_index;

  RaidBarsManage manage{*this};  // Manage mode handler.
};

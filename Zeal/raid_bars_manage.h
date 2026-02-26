#pragma once
#include <Windows.h>

#include <string>
#include <vector>

#include "game_structures.h"

// Forward declare to avoid circular include.
class RaidBars;

// Manages the raid bars manage mode click modifier actions.
// Separated from RaidBars for readability.
class RaidBarsManage {
 public:
  explicit RaidBarsManage(RaidBars &raid_bars);
  ~RaidBarsManage() = default;

  // Disable copy.
  RaidBarsManage(RaidBarsManage const &) = delete;
  RaidBarsManage &operator=(RaidBarsManage const &) = delete;

  // Parses the /raidbars manage <on|off> subcommand. Returns true if handled.
  bool ParseManageArgs(const std::vector<std::string> &args);

  // Called from HandleLMouseUp before normal click handling. Returns true if consumed.
  bool HandleClick(short x, short y);

  // Resets manage mode state (e.g. pending move selection).
  void Clean();

  bool is_enabled() const { return enabled; }

 private:
  int FindFirstEmptyGroup() const;
  std::string GetRaidMemberNameAtIndex(int index) const;

  RaidBars &bars;  // Reference to parent for accessing visible_list, raid_classes, etc.
  bool enabled = false;
  std::string move_pending_name;  // Name of player selected for Ctrl move (empty = none pending).
};
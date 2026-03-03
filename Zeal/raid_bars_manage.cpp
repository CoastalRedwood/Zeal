#include "raid_bars_manage.h"

#include "game_addresses.h"
#include "game_functions.h"
#include "game_structures.h"
#include "game_ui.h"
#include "raid_bars.h"

RaidBarsManage::RaidBarsManage(RaidBars &raid_bars) : bars(raid_bars) {}

void RaidBarsManage::Clean() {
  move_pending_name.clear();
}

bool RaidBarsManage::ParseManageArgs(const std::vector<std::string> &args) {
  if (args.size() < 2 || args[1] != "manage") return false;

  if (args.size() == 3 && (args[2] == "on" || args[2] == "off")) {
    enabled = (args[2] == "on");
    if (enabled) {
      bars.setting_group_sort.set(true);
      bars.setting_clickable.set(true);
      bars.setting_show_all.set(true);
      bars.setting_enabled.set(true);
      move_pending_name.clear();
      Zeal::Game::print_chat("Raidbars manage mode ON");    
      Zeal::Game::print_chat("Shift+Click = Promote to group leader");
      Zeal::Game::print_chat("Alt+Click   = Kick to ungrouped");
      Zeal::Game::print_chat("Ctrl+Click  = Select player then Ctrl+Click destination group");
    } else {
      move_pending_name.clear();
      Zeal::Game::print_chat("Raidbars manage mode OFF");
    }
  } else {
    Zeal::Game::print_chat("Usage: /raidbars manage <on | off>");
    Zeal::Game::print_chat("Raidbars manage is %s", enabled ? "on" : "off");
  }
  return true;
}

bool RaidBarsManage::HandleClick(short x, short y) {
  if (!enabled || !bars.setting_enabled.get() || bars.visible_list.empty()) return false;

  Zeal::GameUI::CXWndManager *wnd_mgr = Zeal::Game::get_wnd_manager();
  if (!wnd_mgr) return false;

  BYTE shift = wnd_mgr->ShiftKeyState;
  BYTE ctrl = wnd_mgr->ControlKeyState;
  BYTE alt = wnd_mgr->AltKeyState;

  // No modifier keys held, let normal click handling proceed.
  if (!shift && !ctrl && !alt) return false;

  int index = bars.CalcClickIndex(x, y);
  if (index < 0) return false;

  // Alt+Click: Kick to ungrouped (#raidmove <name> 0).
  if (alt && !shift && !ctrl) return HandleAltClick(index);

  // Shift+Click: Promote to group leader.
  if (shift && !ctrl && !alt) return HandleShiftClick(index);

  // Ctrl+Click: Select a player, then Ctrl+Click destination group.
  if (ctrl && !shift && !alt) return HandleCtrlClick(index);

  return false;
}

bool RaidBarsManage::HandleAltClick(int index) {
  move_pending_name.clear();  // Cancel any pending move.
  std::string name = GetRaidMemberNameAtIndex(index);
  if (name.empty()) return true;  // Clicked on a label or empty slot.
  DWORD group = Zeal::Game::get_raid_group_number(name.c_str());
  if (group == Zeal::GameStructures::RaidMember::kRaidUngrouped) {
    Zeal::Game::print_chat("Player %s is already ungrouped.", name.c_str());
    return true;
  }
  Zeal::Game::print_chat("Kicking %s to ungrouped.", name.c_str());
  Zeal::Game::do_say(true, "#raidmove %s 0", name.c_str());
  return true;
}

bool RaidBarsManage::HandleShiftClick(int index) {
  move_pending_name.clear();  // Cancel any pending move.
  std::string name = GetRaidMemberNameAtIndex(index);
  if (name.empty()) return true;  // Clicked on a label or empty slot.
  DWORD group = Zeal::Game::get_raid_group_number(name.c_str());
  if (group == Zeal::GameStructures::RaidMember::kRaidUngrouped) {
    // Ungrouped: move to first empty group to create a new group with them as leader.
    int empty_group = FindFirstEmptyGroup();
    if (empty_group < 0) {
      Zeal::Game::print_chat("No empty groups available to move %s into.", name.c_str());
      return true;
    }
    Zeal::Game::print_chat("Moving %s to group %d.", name.c_str(), empty_group + 1);
    Zeal::Game::do_say(true, "#raidmove %s %d", name.c_str(), empty_group + 1);
  } else {
    Zeal::Game::print_chat("Promoting %s to group leader.", name.c_str());
    Zeal::Game::do_say(true, "#raidpromote %s", name.c_str());
  }
  return true;
}

bool RaidBarsManage::HandleCtrlClick(int index) {
  if (move_pending_name.empty()) {
    // First Ctrl+Click: select a player to move.
    std::string name = GetRaidMemberNameAtIndex(index);
    if (name.empty()) return true;  // Clicked on a label or empty slot.
    move_pending_name = name;
    Zeal::Game::print_chat("Selected %s for move. Ctrl+Click a destination group.", name.c_str());
    return true;
  }

  // Second Ctrl+Click: resolve destination group from the clicked index.
  DWORD dest_group = Zeal::GameStructures::RaidMember::kRaidUngrouped;
  
  // Use the group label index map to identify which group was clicked
  for (int group_slot = 0; group_slot < static_cast<int>(bars.visible_group_index.size()); ++group_slot) {

    // Ignore non visible groups
    if (bars.visible_group_index[group_slot] == -1) continue;

    if (bars.visible_group_index[group_slot] <= index) {
      dest_group = (group_slot == 12) ? Zeal::GameStructures::RaidMember::kRaidUngrouped : static_cast<DWORD>(group_slot);
    }
  }

  if (dest_group == static_cast<DWORD>(Zeal::GameStructures::RaidMember::kRaidUngrouped)) {
    Zeal::Game::print_chat("Moving %s to ungrouped.", move_pending_name.c_str());
    Zeal::Game::do_say(true, "#raidmove %s 0", move_pending_name.c_str());
  } else {
    int group_count = Zeal::Game::get_raid_group_count(dest_group);
    if (group_count >= 6) {
      Zeal::Game::print_chat("Group %d is full. Cannot move %s.", dest_group + 1, move_pending_name.c_str());
      move_pending_name.clear();
      return true;
    }
    Zeal::Game::print_chat("Moving %s to group %d.", move_pending_name.c_str(), dest_group + 1);
    Zeal::Game::do_say(true, "#raidmove %s %d", move_pending_name.c_str(), dest_group + 1);
  }
  move_pending_name.clear();
  return true;
}

int RaidBarsManage::FindFirstEmptyGroup() const {
  Zeal::GameStructures::RaidInfo *raid_info = Zeal::Game::RaidInfo;
  if (!raid_info->is_in_raid()) return -1;

  for (int group = 0; group < 12; ++group) {
    if (Zeal::Game::get_raid_group_count(group) == 0) return group;
  }
  return -1;
}

std::string RaidBarsManage::GetRaidMemberNameAtIndex(int index) const {
  if (index < 0 || index >= static_cast<int>(bars.visible_list.size())) return {};

  auto entity = bars.visible_list[index];
  if (entity)
    return entity->Name;  // If it's a valid entity, return the name directly from it
  
  return {};
}


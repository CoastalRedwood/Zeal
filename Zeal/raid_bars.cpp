#include "raid_bars.h"

#include <algorithm>

#include "callbacks.h"
#include "commands.h"
#include "entity_manager.h"
#include "game_addresses.h"
#include "game_functions.h"
#include "game_structures.h"
#include "hook_wrapper.h"
#include "string_util.h"
#include "zeal.h"

// Checks if the user clicked on one of the raid bars.
static void __fastcall LMouseUp(void *game, int unused_edx, short x, short y) {
  auto zeal = ZealService::get_instance();
  if (zeal->raid_bars->HandleLMouseUp(x, y)) return;

  zeal->hooks->hook_map["LMouseUp"]->original(LMouseUp)(game, unused_edx, x, y);
}

RaidBars::RaidBars(ZealService *zeal) {
  zeal->callbacks->AddGeneric([this]() { CallbackRender(); }, callback_type::RenderUI);
  zeal->callbacks->AddGeneric([this]() { Clean(); }, callback_type::EnterZone);
  zeal->callbacks->AddGeneric([this]() { Clean(); }, callback_type::CleanUI);  // Note: new_ui only call.
  zeal->callbacks->AddGeneric([this]() { Clean(); }, callback_type::DXReset);  // Just release all resources.
  zeal->callbacks->AddGeneric([this]() { Clean(); }, callback_type::DXCleanDevice);

  // Listen for OP_RaidUpdate packets to immediately trigger a visible list refresh.
  zeal->callbacks->AddPacket(
      [this](UINT opcode, char *buffer, UINT len) {
        if (opcode == Zeal::Packets::RaidUpdate) raid_update_dirty = true;
        return false;
      },
      callback_type::WorldMessage);

  zeal->commands_hook->Add("/raidbars", {}, "Controls raid status bars display",
                           [this](std::vector<std::string> &args) {
                             ParseArgs(args);
                             return true;
                           });

  zeal->hooks->Add("LMouseUp", 0x00531614, LMouseUp, hook_type_detour);

  // Ensure our cached entity pointer is flushed when an entity despawns.
  zeal->callbacks->AddEntity(
      [this](struct Zeal::GameStructures::Entity *entity) {
        if (!setting_enabled.get() || !entity || entity->Type != Zeal::GameEnums::Player) return;

        // For now use an unoptimized full sweep to ensure the entity is definitely flushed.
        for (auto &class_group : raid_classes)
          for (auto &member : class_group)
            if (member.entity == entity) member.entity = nullptr;

        // Also clean the visible list.  Sweep through all of it to be safe.
        for (auto &list_entity : visible_list)
          if (list_entity == entity) list_entity = nullptr;
      },
      callback_type::EntityDespawn);
}

RaidBars::~RaidBars() { Clean(); }

void RaidBars::Clean() {
  next_update_game_time_ms = 0;
  bitmap_font.reset();  // Releases all DX and other resources.
  visible_list.clear();
  for (auto &class_group : raid_classes) class_group.clear();  // Drop all entity references.
  manage.Clean();
}

void RaidBars::ParseArgs(const std::vector<std::string> &args) {
  if (args.size() == 2 && (args[1] == "on" || args[1] == "off")) {
    setting_enabled.set(args[1] == "on");
    Zeal::Game::print_chat("Raidbars are %s", setting_enabled.get() ? "on" : "off");
    return;
  }

  if (args.size() == 2 && args[1] == "toggle") {
    setting_enabled.toggle();
    Zeal::Game::print_chat("Raidbars are %s", setting_enabled.get() ? "on" : "off");
    return;
  }

  if (manage.ParseManageArgs(args)) return;

  if (args.size() >= 2 && args[1] == "groups") {
    if (args.size() == 3 && (args[2] == "on" || args[2] == "off"))
      setting_group_sort.set(args[2] == "on");
    else if (args.size() == 3 && args[2] == "toggle")
      setting_group_sort.toggle();
    else if (args.size() != 2)
      Zeal::Game::print_chat("Usage: /raidbars groups <on | off | toggle>");

    Zeal::Game::print_chat("Raidbars sort by groups is set to %s", setting_group_sort.get() ? "on" : "off");
    return;
  }

  if (args.size() > 1 && args[1] == "font") {
    if (args.size() == 3) {
      setting_bitmap_font_filename.set(args[2]);
    } else {
      Zeal::Game::print_chat("Usage: /raidbars font <fontname> where fontname is one of:");
      const auto fonts = BitmapFontBase::get_available_fonts();
      for (const auto &font : fonts) Zeal::Game::print_chat(font.c_str());
    }
    Zeal::Game::print_chat("Font filename set to %s", setting_bitmap_font_filename.get().c_str());
    return;
  }

  if (args.size() > 1 && args[1] == "barwidth") {
    int width = 0;
    if (args.size() == 3 && Zeal::String::tryParse(args[2], &width))
      setting_bar_width.set(width);
    else
      Zeal::Game::print_chat("Usage: '/raidbars barwidth value' where 0 = use auto-scale");
    Zeal::Game::print_chat("Bar width set to %d", setting_bar_width.get());
    return;
  }

  if (args.size() > 1 && args[1] == "barheight") {
    int height = 0;
    if (args.size() == 3 && Zeal::String::tryParse(args[2], &height))
      setting_bar_height.set(height);
    else
      Zeal::Game::print_chat("Usage: '/raidbars barheight value' where 0 = use auto-scale");
    Zeal::Game::print_chat("Bar height set to %d", setting_bar_height.get());
    return;
  }

  if (args.size() >= 2 && args[1] == "position") {
    int left, top;
    int right = 0;
    int bottom = 0;
    bool valid = false;
    if (args.size() == 4)
      valid = Zeal::String::tryParse(args[2], &left, true) && Zeal::String::tryParse(args[3], &top, true);
    else if (args.size() == 6)
      valid = Zeal::String::tryParse(args[2], &left, true) && Zeal::String::tryParse(args[3], &top, true) &&
              Zeal::String::tryParse(args[4], &right, true) && Zeal::String::tryParse(args[5], &bottom, true);

    if (valid && (left < 0 || (right && right < left) || top < 0 || (bottom && bottom < top))) {
      Zeal::Game::print_chat("Invalid position coordinates");
      valid = false;
    }

    if (valid) {
      setting_position_left.set(left);
      setting_position_top.set(top);
      setting_position_right.set(right);
      setting_position_bottom.set(bottom);
    }

    if (valid || args.size() == 2) {
      Zeal::Game::print_chat("Raidbars position set to (%d, %d, %d, %d)", setting_position_left.get(),
                             setting_position_top.get(), setting_position_right.get(), setting_position_bottom.get());
      return;
    }
  }

  if (args.size() >= 2 && args[1] == "grid") {
    int rows, columns;
    bool valid = false;
    if (args.size() == 4 && Zeal::String::tryParse(args[2], &rows, true) &&
        Zeal::String::tryParse(args[3], &columns, true) && HandleSetGrid(rows, columns)) {
      Zeal::Game::print_chat("Raidbars position set to (%d, %d, %d, %d)", setting_position_left.get(),
                             setting_position_top.get(), setting_position_right.get(), setting_position_bottom.get());
    } else {
      Zeal::Game::print_chat("Usage: /raidbars grid <num_rows> <num_cols>");
    }
    return;
  }

  if (args.size() >= 2 && args[1] == "showall") {
    if (args.size() == 3 && (args[2] == "on" || args[2] == "off"))
      setting_show_all.set(args[2] == "on");
    else if (args.size() == 3 && args[2] == "toggle")
      setting_show_all.toggle();
    else if (args.size() != 2)
      Zeal::Game::print_chat("Usage: /raidbars showall <on | off | toggle>");

    Zeal::Game::print_chat("Raidbars showall is set to %s", setting_show_all.get() ? "on" : "off");
    return;
  }

  if (args.size() >= 2 && args[1] == "clickable") {
    if (args.size() == 3 && (args[2] == "on" || args[2] == "off")) setting_clickable.set(args[2] == "on");
    Zeal::Game::print_chat("Raidbars clickable is set to %s", setting_clickable.get() ? "on" : "off");
    return;
  }

  if (args.size() >= 2 && args[1] == "background") {
    int alpha = 0;
    if (args.size() == 3 && Zeal::String::tryParse(args[2], &alpha, true) && alpha >= 0 && alpha <= 100) {
      setting_background_alpha.set(alpha);
    } else if (args.size() != 2) {
      Zeal::Game::print_chat("Usage: /raidbars background <alpha> (0 to 100 = invisible to solid black)");
    }
    Zeal::Game::print_chat("Raidbars background alpha is set to %d%%", setting_background_alpha.get());
    return;
  }

  if (args.size() >= 2 && args[1] == "threshold") {
    int threshold = 0;
    if (args.size() == 3 && Zeal::String::tryParse(args[2], &threshold, true) && threshold >= 0 && threshold <= 100) {
      setting_show_threshold.set(threshold);
    } else if (args.size() != 2) {
      Zeal::Game::print_chat("Usage: /raidbars threshold <value> (0 to 100, applies to /raidbars filter <classes>)");
    }
    Zeal::Game::print_chat("Raidbars filter threshold is set to show values <= to %d%%", setting_show_threshold.get());
    return;
  }

  if (args.size() >= 2 && (args[1] == "priority" || args[1] == "always" || args[1] == "never" || args[1] == "filter")) {
    if (args.size() > 2) {
      std::string text = args[2];
      for (int i = 3; i < args.size(); ++i) text += " " + args[i];
      std::transform(text.begin(), text.end(), text.begin(), ::toupper);
      if (args[1] == "priority")
        setting_class_priority.set(text);
      else if (args[1] == "always")
        setting_class_always.set(text);
      else if (args[1] == "never")
        setting_class_never.set(text);
      else if (args[1] == "filter")
        setting_class_filter.set(text);
    }
    DumpClassSettings();
    return;
  }

  Zeal::Game::print_chat("Usage: /raidbars <on | off | toggle>");
  Zeal::Game::print_chat("Usage: /raidbars manage <on | off>");
  Zeal::Game::print_chat("Usage: /raidbars position <left> <top> [<right> <bottom>]");
  Zeal::Game::print_chat("Note: right and bottom are screen coordinates relative to upper left");
  Zeal::Game::print_chat("Usage: /raidbars grid <num_rows> <num_cols> (auto-calcs right and bottom)");
  Zeal::Game::print_chat("Usage: /raidbars background <alpha> (0 to 100 = invisible to solid black)");
  Zeal::Game::print_chat("Usage: /raidbars [barheight | barwidth] <value> (0 = autoscale to font)");
  Zeal::Game::print_chat("Usage: /raidbars font font_filename");
  Zeal::Game::print_chat("Usage: /raidbars clickable <on | off>");
  Zeal::Game::print_chat("Usage: /raidbars groups <on | off | toggle>");
  Zeal::Game::print_chat("Usage: /raidbars showall <on | off | toggle>");
  Zeal::Game::print_chat("Usage: /raidbars always <class list> where list is like 'WAR PAL SHD'");
  Zeal::Game::print_chat("Usage: /raidbars never <class list> where list is like 'WAR PAL SHD'");
  Zeal::Game::print_chat("Usage: /raidbars priority <class list> where list is like 'WAR PAL SHD ENC'");
  Zeal::Game::print_chat("Usage: /raidbars filter <class list> where list is like 'WAR PAL SHD'");
  Zeal::Game::print_chat("Usage: /raidbars threshold <value> (filtered class shown with hp % <= value)");
}

// Utility for auto-calculating the positions box right and bottom using a target number of rows and columns.
bool RaidBars::HandleSetGrid(int num_rows, int num_cols) {
  if (num_rows <= 0 || num_rows > 100 || num_cols <= 0 || num_cols > 100) {
    Zeal::Game::print_chat("Error: num_rows and num_cols must be between 1 and 100");
    return false;
  }

  LoadBitmapFont();  // Need font loaded to set grid_height and grid_width properly.
  if (!bitmap_font || grid_height <= 0 || grid_width <= 0) return false;

  int max_rows = static_cast<int>((Zeal::Game::get_screen_resolution_y() - setting_position_top.get()) / grid_height);
  int max_cols = static_cast<int>((Zeal::Game::get_screen_resolution_x() - setting_position_left.get()) / grid_width);

  if (max_rows <= 0 || max_cols <= 0) {
    Zeal::Game::print_chat("Error: Can not fit any on screen. Reduce /raidbar positions left or top.");
    return false;
  }

  num_rows = min(num_rows, max_rows);
  num_cols = min(num_cols, max_cols);
  Zeal::Game::print_chat("Setting grid to %d rows by %d cols", num_rows, num_cols);
  float bottom = setting_position_top.get() + num_rows * grid_height;
  float right = setting_position_left.get() + num_cols * grid_width;
  setting_position_bottom.set(static_cast<int>(std::ceil(bottom)));
  setting_position_right.set(static_cast<int>(std::ceil(right)));
  return true;
}

// Loads the bitmap font for real-time text rendering to screen.
void RaidBars::LoadBitmapFont() {
  if (bitmap_font || setting_bitmap_font_filename.get().empty()) return;

  IDirect3DDevice8 *device = ZealService::get_instance()->dx->GetDevice();
  std::string font_filename = setting_bitmap_font_filename.get();
  bool is_default_font = (font_filename.empty() || font_filename == kUseDefaultFont);
  if (is_default_font) font_filename = kDefaultFont;
  if (device != nullptr) bitmap_font = BitmapFont::create_bitmap_font(*device, font_filename);
  if (!bitmap_font) {
    Zeal::Game::print_chat("Failed to load font: %s", font_filename.c_str());
    if (is_default_font) {
      Zeal::Game::print_chat("Disabling raidbars due to font issue");
      setting_enabled.set(false);
    } else {
      setting_bitmap_font_filename.set(kUseDefaultFont);  // Try again with default next round.
    }
    return;
  }

  bitmap_font->set_drop_shadow(true);
  bitmap_font->set_full_screen_viewport(true);  // Allow rendering list outside reduced viewport.

  std::string text("Fakenametotest");  // 14 character as maximum name length with average chars.
  auto text_only_size = bitmap_font->measure_string(text.c_str());  // Doesn't currently support multi-lines.

  float bar_width = static_cast<float>(setting_bar_width.get());
  if (bar_width == 0) bar_width = std::roundf(text_only_size.x * 0.9);
  bar_width = max(10.f, min(150.f, bar_width));
  bitmap_font->set_stats_bar_width(bar_width);

  float bar_height = static_cast<float>(setting_bar_height.get());
  if (bar_height == 0) bar_height = std::roundf(text_only_size.y * 0.7);
  bar_height = max(4.f, min(50.f, bar_height));
  bitmap_font->set_stats_bar_height(bar_height);

  const char healthbar[4] = {'\n', BitmapFontBase::kStatsBarBackground, BitmapFontBase::kHealthBarValue, 0};
  std::string full_text = text + healthbar;
  grid_width = max(text_only_size.x + 0.25f, bar_width + 5.f);
  grid_height = max(bitmap_font->get_text_height(full_text) + 0.25f, bar_height + 2.f);
}

// Load the class priority from settings (based on defaults).
void RaidBars::SyncClassPriority() {
  std::string priority_list = setting_class_priority.get();

  // Somewhat arbitrary ordering based on likelihood to need healing / monitoring.
  using Zeal::GameEnums::ClassTypes;
  class_priority = {ClassTypes::Warrior,   ClassTypes::Paladin,  ClassTypes::Shadowknight, ClassTypes::Enchanter,
                    ClassTypes::Wizard,    ClassTypes::Monk,     ClassTypes::Ranger,       ClassTypes::Rogue,
                    ClassTypes::Beastlord, ClassTypes::Bard,     ClassTypes::Cleric,       ClassTypes::Shaman,
                    ClassTypes::Druid,     ClassTypes::Magician, ClassTypes::Necromancer};
  for (auto &index : class_priority) index -= kClassIndexOffset;  // Convert to zero index.

  if (priority_list.empty()) return;  // Just go with default.

  // Capitalize to simplify comparisons.
  std::transform(priority_list.begin(), priority_list.end(), priority_list.begin(), ::toupper);

  auto split = Zeal::String::split_text(priority_list, " ");
  std::vector<int> entries;
  for (const auto &entry : split) {
    for (int i = 0; i < class_priority.size(); ++i) {
      if (entry == Zeal::Game::class_name_short(i + kClassIndexOffset)) {
        auto it = std::find(entries.begin(), entries.end(), i);
        if (it == entries.end())  // Do not add duplicates (could have used a set).
          entries.push_back(i);   // Pushing back zero index value.
        break;
      }
    }
  }
  // Copy the defaults into std::vector to extract from.
  std::vector<int> source(class_priority.begin(), class_priority.end());
  size_t index = 0;
  for (auto &value : entries) {
    class_priority[index++] = value;
    std::erase(source, value);
  }
  // Append any non-specified classes.
  for (auto &value : source) {
    if (index >= class_priority.size()) break;  // Paranoid overflow clamp (shouldn't happen).
    class_priority[index++] = value;
  }
}

// Load the show class always flags from settings.
void RaidBars::SyncClassAlways() {
  std::string always_list = setting_class_always.get();

  for (auto &entry : class_always) entry = false;  // Default to none.

  if (always_list.empty()) return;  // Just go with default.

  // Capitalize to simplify comparisons.
  std::transform(always_list.begin(), always_list.end(), always_list.begin(), ::toupper);

  auto split = Zeal::String::split_text(always_list, " ");
  for (const auto &entry : split) {
    for (int i = 0; i < kNumClasses; ++i) {
      if (entry == Zeal::Game::class_name_short(i + kClassIndexOffset)) {
        class_always[i] = true;
        break;
      }
    }
  }
}

// Load the show class never flags from settings.
void RaidBars::SyncClassNever() {
  std::string never_list = setting_class_never.get();

  for (auto &entry : class_never) entry = false;  // Default to none.

  if (never_list.empty()) return;  // Just go with default.

  // Capitalize to simplify comparisons.
  std::transform(never_list.begin(), never_list.end(), never_list.begin(), ::toupper);

  auto split = Zeal::String::split_text(never_list, " ");
  for (const auto &entry : split) {
    for (int i = 0; i < kNumClasses; ++i) {
      if (entry == Zeal::Game::class_name_short(i + kClassIndexOffset)) {
        class_never[i] = true;
        break;
      }
    }
  }
}

// Load the show class filter flags from settings.
void RaidBars::SyncClassFilter() {
  std::string filter_list = setting_class_filter.get();

  for (auto &entry : class_filter) entry = false;  // Default to none.

  if (filter_list.empty()) return;  // Just go with default.

  // Capitalize to simplify comparisons.
  std::transform(filter_list.begin(), filter_list.end(), filter_list.begin(), ::toupper);

  auto split = Zeal::String::split_text(filter_list, " ");
  for (const auto &entry : split) {
    for (int i = 0; i < kNumClasses; ++i) {
      if (entry == Zeal::Game::class_name_short(i + kClassIndexOffset)) {
        class_filter[i] = true;
        break;
      }
    }
  }
}

void RaidBars::DumpClassSettings() const {
  std::string prio_list;
  for (const auto &value : class_priority) prio_list += " " + Zeal::Game::class_name_short(value + kClassIndexOffset);
  std::string message = "RaidBars class priority:" + prio_list;
  Zeal::Game::print_chat(message.c_str());

  std::string list;
  for (int i = 0; i < class_always.size(); ++i)
    if (class_always[i]) list += " " + Zeal::Game::class_name_short(i + kClassIndexOffset);
  message = "RaidBars class always:" + list;
  Zeal::Game::print_chat(message.c_str());

  std::string never_list;
  for (int i = 0; i < class_never.size(); ++i)
    if (class_never[i]) never_list += " " + Zeal::Game::class_name_short(i + kClassIndexOffset);
  message = "RaidBars class never:" + never_list;
  Zeal::Game::print_chat(message.c_str());

  std::string filter_list;
  for (int i = 0; i < class_filter.size(); ++i)
    if (class_filter[i]) filter_list += " " + Zeal::Game::class_name_short(i + kClassIndexOffset);
  message = "RaidBars class filter:" + filter_list;
  Zeal::Game::print_chat(message.c_str());
}

// Populates raid_classes with all raid members.
void RaidBars::UpdateRaidMembers() {
  // For now do a full clear and reload every time.
  for (auto &class_group : raid_classes) class_group.clear();  // Drop all entity references.

  Zeal::GameStructures::RaidInfo *raid_info = Zeal::Game::RaidInfo;
  if (!raid_info->is_in_raid()) {
    return;
  }

  // Sweep through the entire raid list bucketizing into the difference classes.
  auto entity_manager = ZealService::get_instance()->entity_manager.get();  // Short-term ptr.
  for (int i = 0; i < Zeal::GameStructures::RaidInfo::kRaidMaxMembers; ++i) {
    const auto &member = raid_info->MemberList[i];
    if (!member.Name || !member.Name[0]) continue;  // Skip empty slots.
    size_t class_index = member.ClassValue - kClassIndexOffset;
    if (class_index >= raid_classes.size()) continue;  // Paranoia, shouldn't happen.
    auto &class_group = raid_classes[class_index];
    auto entity = entity_manager->Get(member.Name);  // Could be null if out of zone or a corpse.
    if (entity && entity->Type != Zeal::GameEnums::EntityTypes::Player) entity = nullptr;
    DWORD class_color = Zeal::Game::get_raid_class_color(member.ClassValue);
    class_group.emplace_back(RaidMember{.name = member.Name,
                                        .entity = entity,
                                        .color = class_color,
                                        .group_number = member.GroupNumber,
                                        .is_group_leader = (member.IsGroupLeader != 0)});
  }

  // And then alphabetically sort all class groups.
  for (auto &class_group : raid_classes) {
    std::sort(class_group.begin(), class_group.end(),
              [](const RaidMember &a, const RaidMember &b) { return a.name < b.name; });
  }
}

// Returns the visible_list index for screen coordinates, or -1 if out of bounds.
int RaidBars::CalcClickIndex(short x, short y) const {
  const float x_min = static_cast<float>(setting_position_left.get());
  const float y_min = static_cast<float>(setting_position_top.get());
  if (x < x_min || y < y_min) return -1;  // Off left or top side.
  int click_row_index = static_cast<int>((y - y_min) / grid_height);
  if (click_row_index >= grid_height_count_max) return -1;  // Off bottom.
  int click_column_index = static_cast<int>((x - x_min) / grid_width);
  int click_column_start = click_column_index * grid_height_count_max;
  int column_row_index_max = static_cast<int>(visible_list.size()) - click_column_start;
  if (column_row_index_max < 0 || click_row_index >= column_row_index_max) return -1;
  int index = click_column_start + click_row_index;
  if (index < 0 || index >= visible_list.size()) return -1;
  return index;
}

bool RaidBars::HandleLMouseUp(short x, short y) {
  if (!setting_enabled.get() || visible_list.empty()) return false;

  if (!setting_clickable.get()) return false;

  // Copy some client call behavior to bail out upon certain conditions.
  if (*reinterpret_cast<int *>(0x007d0254) != 0) return false;   // Waiting for server ack to unfreeze UI.
  if (*reinterpret_cast<BYTE *>(0x007985ea) != 0) return false;  // RMB held down.

  // Check manage mode modifier clicks first.
  if (manage.HandleClick(x, y)) return true;

  int index = CalcClickIndex(x, y);
  if (index < 0) return false;

  auto entity = visible_list[index];
  if (entity == nullptr) return false;

  // Quarm now allows targeting of any raid member across the zone. So just go ahead
  // and directly set the target instead of using do_target(entity->Name) with checks.
  Zeal::Game::set_target(entity);
  return true;
}

void RaidBars::CallbackRender() {
  if (!setting_enabled.get() || !Zeal::Game::is_in_game()) return;

  // Bail out if not in raid and also perform cleanup here when exiting a raid.
  if (!Zeal::Game::RaidInfo->is_in_raid()) {
    if (bitmap_font) Clean();  // Initialized font used as a flag for the need to flush.
    return;
  }

  auto display = Zeal::Game::get_display();
  if (!display || !Zeal::Game::is_gui_visible()) return;

  LoadBitmapFont();
  if (!bitmap_font) return;

  DWORD current_time_ms = display->GameTimeMs;
  if (raid_update_dirty || next_update_game_time_ms <= current_time_ms) {
    next_update_game_time_ms = current_time_ms + 1000;  // Roughly one second update intervals.
    raid_update_dirty = false;
    UpdateRaidMembers();
  }

  // The position coordinates are full screen (not viewport reduced).
  int left = setting_position_left.get();
  int top = setting_position_top.get();
  int right =
      setting_position_right.get() > left ? setting_position_right.get() : Zeal::Game::get_screen_resolution_x();
  int bottom =
      setting_position_bottom.get() > top ? setting_position_bottom.get() : Zeal::Game::get_screen_resolution_y();
  const float x_min = static_cast<float>(left);
  const float y_min = static_cast<float>(top);
  const float x_max = static_cast<float>(right);
  const float y_max = static_cast<float>(bottom);
  grid_height_count_max = static_cast<int>((y_max - y_min) / grid_height);

  if (setting_background_alpha.get()) {
    RECT rect = {.left = left, .top = top, .right = right, .bottom = bottom};
    BYTE alpha = setting_background_alpha.get() * 255 / 100;
    D3DCOLOR color = D3DCOLOR_ARGB(alpha, 0, 0, 0);
    bitmap_font->queue_background_rect(rect, color);
  }

  visible_list.clear();
  visible_group_index.fill(-1);  // Reset all group indexes to -1 (not visible) before repopulating.
  if (setting_group_sort.get())
    QueueByGroup(x_min, y_min, x_max, y_max);
  else
    QueueByClass(x_min, y_min, x_max, y_max);

  bitmap_font->flush_queue_to_screen();
}

void RaidBars::QueueByClass(const float x_min, const float y_min, const float x_max, const float y_max) {
  float x = x_min;
  float y = y_min;

  // Go through the classes in prioritized order.
  bool show_all = setting_show_all.get();
  const auto self = Zeal::Game::get_self();
  for (const int class_index : class_priority) {
    const auto &group = raid_classes[class_index];
    if (group.empty() || class_never[class_index]) continue;
    bool show_class = show_all || class_always[class_index];
    int threshold = class_filter[class_index] ? setting_show_threshold.get() : 99;

    const DWORD out_of_zone_color = D3DCOLOR_XRGB(0x80, 0x80, 0x80);  // Grey color.
    for (const auto &member : group) {
      if (y + grid_height > y_max) {
        y = y_min;
        x += grid_width;
      }
      if (x + grid_width > x_max) break;  // Bail out if list grows off-screen.

      const auto entity = member.entity;
      if (entity == self) continue;  // Skip self.
      int hp_percent =
          (entity && entity->HpCurrent > 0 && entity->HpMax > 0) ? (entity->HpCurrent * 100) / entity->HpMax : 0;
      if (hp_percent >= threshold && !show_class) continue;  // Skip.
      const char healthbar[4] = {'\n', BitmapFontBase::kStatsBarBackground, BitmapFontBase::kHealthBarValue, 0};
      std::string full_text = member.name + healthbar;

      visible_list.push_back(entity);
      bitmap_font->set_hp_percent(hp_percent);
      DWORD color = entity ? member.color : out_of_zone_color;
      bitmap_font->queue_string(full_text.c_str(), Vec3(x, y, 0), false, color);
      y += grid_height;
    }
  }
}

void RaidBars::QueueByGroup(const float x_min, const float y_min, const float x_max, const float y_max) {
  float x = x_min;
  float y = y_min;

  // Go through the groups.
  bool show_all = setting_show_all.get();
  const DWORD out_of_zone_color = D3DCOLOR_XRGB(0x80, 0x80, 0x80);  // Grey color.
  const DWORD empty_color = D3DCOLOR_XRGB(0x60, 0x60, 0x60);        // Darker grey.

  // Scan through all possible raid groups (0 to 11) plus the ungrouped.
  for (int i = 0; i < 13; ++i) {
    const bool ungrouped = (i == 12);
    const int group_index = ungrouped ? Zeal::GameStructures::RaidMember::kRaidUngrouped : i;

    // Do an unoptimized sweep through all raid members looking for members of group_index.
    // This does keep the same class priority sorting within groups except leader first.
    int group_count = 0;  // Track number of group members found.
    std::vector<const RaidMember *> group_members;
    const int group_max = ungrouped ? 72 : 6;
    for (const int class_index : class_priority) {
      for (const auto &member : raid_classes[class_index]) {
        if (member.group_number != group_index || group_members.size() >= group_max) continue;
        if (member.is_group_leader)
          group_members.insert(group_members.begin(), &member);  // Put leader at front.
        else
          group_members.push_back(&member);
      }
    }
    if (group_members.empty()) continue;  // Skip this Group.

    // Add a Group #: or Ungrouped: label.
    if (y + grid_height > y_max) {
      y = y_min;
      x += grid_width;
    }
    if (x + grid_width > x_max) return;  // Bail out if list grows off-screen.

    const std::string group_label =
        ungrouped ? "Ungrouped:" : std::string("Group ") + std::to_string(group_index + 1) + ": ";
    float y_offset = grid_height - bitmap_font->get_line_spacing() - 2;  // Add some padding for the label.
    visible_group_index[i] = static_cast<int>(visible_list.size());      // Record label index before pushing.
    visible_list.push_back(nullptr);
    bitmap_font->queue_string(group_label.c_str(), Vec3(x, y + y_offset, 0), false,
                              D3DCOLOR_XRGB(0xff, 0xff, 0xff));  // White label
    y += grid_height;

    for (const auto &member : group_members) {
      if (y + grid_height > y_max) {
        y = y_min;
        x += grid_width;
      }
      if (x + grid_width > x_max) return;  // Bail out if list grows off-screen.

      const auto entity = member->entity;
      int hp_percent =
          (entity && entity->HpCurrent > 0 && entity->HpMax > 0) ? (entity->HpCurrent * 100) / entity->HpMax : 0;
      const char healthbar[4] = {'\n', BitmapFontBase::kStatsBarBackground, BitmapFontBase::kHealthBarValue, 0};
      std::string full_text = member->name + healthbar;
      visible_list.push_back(entity);
      bitmap_font->set_hp_percent(hp_percent);
      DWORD color = entity ? member->color : out_of_zone_color;
      bitmap_font->queue_string(full_text.c_str(), Vec3(x, y, 0), false, color);
      y += grid_height;
    }

    // Add empty slots if show_all and not ungrouped else continue the loop.
    if (!show_all || ungrouped) continue;
    for (auto i = group_members.size(); i < 6; ++i) {
      if (y + grid_height > y_max) {
        y = y_min;
        x += grid_width;
      }
      if (x + grid_width > x_max) return;  // Bail out if list grows off-screen.
      visible_list.push_back(nullptr);
      bitmap_font->queue_string("Empty", Vec3(x, y, 0), false, empty_color);
      y += grid_height;
    }
  }
}
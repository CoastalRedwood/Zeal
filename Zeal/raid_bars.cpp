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
}

void RaidBars::ParseArgs(const std::vector<std::string> &args) {
  if (args.size() == 2 && (args[1] == "on" || args[1] == "off")) {
    setting_enabled.set(args[1] == "on");
    Zeal::Game::print_chat("Raidbars are %s", setting_enabled.get() ? "on" : "off");
    return;
  }

  if (args.size() == 3 && args[1] == "font") {
    setting_bitmap_font_filename.set(args[2]);
    Zeal::Game::print_chat("Font filename set to %s", setting_bitmap_font_filename.get().c_str());
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

  if (args.size() >= 2 && args[1] == "showall") {
    if (args.size() == 3 && (args[2] == "on" || args[2] == "off")) setting_show_all.set(args[2] == "on");
    Zeal::Game::print_chat("Raidbars showall is set to %s", setting_show_all.get() ? "on" : "off");
    return;
  }

  if (args.size() >= 2 && args[1] == "clickable") {
    if (args.size() == 3 && (args[2] == "on" || args[2] == "off")) setting_clickable.set(args[2] == "on");
    Zeal::Game::print_chat("Raidbars clickable is set to %s", setting_clickable.get() ? "on" : "off");
    return;
  }

  if (args.size() >= 2 && (args[1] == "priority" || args[1] == "always")) {
    if (args.size() > 2) {
      std::string text = args[2];
      for (int i = 3; i < args.size(); ++i) text += " " + args[i];
      std::transform(text.begin(), text.end(), text.begin(), ::toupper);
      if (args[1] == "priority")
        setting_class_priority.set(text);
      else
        setting_class_always.set(text);
    }
    DumpClassSettings();
    return;
  }

  Zeal::Game::print_chat("Usage: /raidbars <on | off>");
  Zeal::Game::print_chat("Usage: /raidbars font font_filename");
  Zeal::Game::print_chat("Usage: /raidbars position <left> <top> [<right=0> <bottom=0>]");
  Zeal::Game::print_chat("Usage: /raidbars <showall | clickable> <on | off>");
  Zeal::Game::print_chat("Usage: /raidbars always <class list> where list is like 'WAR PAL SHD'");
  Zeal::Game::print_chat("Usage: /raidbars priority <class list> where list is like 'WAR PAL SHD ENC'");
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
  const char healthbar[4] = {'\n', BitmapFontBase::kStatsBarBackground, BitmapFontBase::kHealthBarValue, 0};
  std::string full_text = text + healthbar;
  auto size = bitmap_font->measure_string(text.c_str());  // Doesn't currently support multi-lines.
  grid_width = size.x + 0.25f;
  grid_height = bitmap_font->get_text_height(full_text) + 0.25f;
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
    size_t class_index = member.ClassValue - 1;
    if (class_index >= raid_classes.size()) continue;  // Paranoia, shouldn't happen.
    auto &class_group = raid_classes[class_index];
    auto entity = entity_manager->Get(member.Name);  // Could be null if out of zone or a corpse.
    if (entity && entity->Type != Zeal::GameEnums::EntityTypes::Player) entity = nullptr;
    class_group.emplace_back(RaidMember{.name = member.Name, .entity = entity});
  }

  // And then alphabetically sort all class groups.
  for (auto &class_group : raid_classes) {
    std::sort(class_group.begin(), class_group.end(),
              [](const RaidMember &a, const RaidMember &b) { return a.name < b.name; });
  }
}

bool RaidBars::HandleLMouseUp(short x, short y) {
  if (!setting_clickable.get() || !setting_enabled.get() || visible_list.empty()) return false;

  // Copy some client call behavior to bail out upon certain conditions.
  if (*reinterpret_cast<int *>(0x007d0254) != 0) return false;   // Waiting for server ack to unfreeze UI.
  if (*reinterpret_cast<BYTE *>(0x007985ea) != 0) return false;  // RMB held down.

  // Calculate the index into the visible list.
  const float x_min = static_cast<float>(setting_position_left.get());
  const float y_min = static_cast<float>(setting_position_top.get());
  if (x < x_min || y < y_min) return false;  // Off left or top side.
  int click_row_index = static_cast<int>((y - y_min) / grid_height);
  if (click_row_index >= grid_height_count_max) return false;  // Off bottom.
  int click_column_index = static_cast<int>((x - x_min) / grid_width);
  int click_column_start = click_column_index * grid_height_count_max;
  int column_row_index_max = static_cast<int>(visible_list.size()) - click_column_start;
  if (column_row_index_max < 0 || click_row_index >= column_row_index_max) return false;
  int index = click_column_start + click_row_index;

  if (index < 0 || index >= visible_list.size()) return false;  // Paranoid final check.
  auto entity = visible_list[index];
  if (entity == nullptr) return false;

  Zeal::Game::do_target(entity->Name);
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
  if (next_update_game_time_ms <= current_time_ms) {
    next_update_game_time_ms = current_time_ms + 1000;  // Roughly one second update intervals.
    UpdateRaidMembers();
  }

  // The position coordinates are full screen (not viewport reduced).
  const float x_min = static_cast<float>(setting_position_left.get());
  const float y_min = static_cast<float>(setting_position_top.get());
  const float x_max = static_cast<float>(setting_position_right.get() > x_min ? setting_position_right.get()
                                                                              : Zeal::Game::get_screen_resolution_x());
  const float y_max = static_cast<float>(setting_position_bottom.get() > y_min ? setting_position_bottom.get()
                                                                               : Zeal::Game::get_screen_resolution_y());
  float x = x_min;
  float y = y_min;
  grid_height_count_max = static_cast<int>((y_max - y_min) / grid_height);

  // Then go through the classes in prioritized order.
  visible_list.clear();
  bool show_all = setting_show_all.get();
  const auto self = Zeal::Game::get_self();
  for (const int class_index : class_priority) {
    const auto &group = raid_classes[class_index];
    if (group.empty()) continue;
    bool show_class = show_all || class_always[class_index];

    // Get class color.
    DWORD class_color = Zeal::Game::get_raid_class_color(class_index + kClassIndexOffset);
    const DWORD out_of_zone_color = D3DCOLOR_XRGB(0x80, 0x80, 0x80);  // Grey color.
    for (const auto &member : group) {
      if (y + grid_height > y_max) {
        y = y_min;
        x += grid_width;
      }
      if (x + grid_width > x_max) break;  // Bail out if list grows off-screen.

      const auto entity = member.entity;
      if (entity == self) continue;          // Skip self.
      if (!entity && !show_class) continue;  // Skip out of zone if not forced on.
      int hp_percent =
          (entity && entity->HpCurrent > 0 && entity->HpMax > 0) ? (entity->HpCurrent * 100) / entity->HpMax : 0;
      if (hp_percent >= 99 && !(show_all || class_always[class_index])) continue;  // Skip.
      const char healthbar[4] = {'\n', BitmapFontBase::kStatsBarBackground, BitmapFontBase::kHealthBarValue, 0};
      std::string full_text = member.name + healthbar;

      visible_list.push_back(entity);
      bitmap_font->set_hp_percent(hp_percent);
      DWORD color = entity ? class_color : out_of_zone_color;
      bitmap_font->queue_string(full_text.c_str(), Vec3(x, y, 0), false, color);
      y += grid_height;
    }
  }

  bitmap_font->flush_queue_to_screen();
}
#define NOMINMAX
#include "assist_target.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "callbacks.h"
#include "commands.h"
#include "game_addresses.h"
#include "game_functions.h"
#include "game_packets.h"
#include "hook_wrapper.h"
#include "string_util.h"
#include "zeal.h"

// Chains onto the existing LMouseUp detour (raid_bars owns "LMouseUp" at this address). Our hook wraps it, so
// calling original() falls through to the raid bars handler and then into the game code.
static void __fastcall LMouseUp(void *game, int unused_edx, short x, short y) {
  auto zeal = ZealService::get_instance();
  if (zeal->assist_target->HandleLMouseUp(x, y)) return;
  zeal->hooks->hook_map["AssistTargetLMouseUp"]->original(LMouseUp)(game, unused_edx, x, y);
}

AssistTarget::AssistTarget(ZealService *zeal) {
  // Track all visible combat: build attacker -> victim and victim -> attacker maps from OP_Damage.
  zeal->callbacks->AddPacket(
      [this](UINT opcode, char *buffer, UINT len) {
        if (opcode == Zeal::Packets::Damage && len >= sizeof(Zeal::Packets::Damage_Struct))
          HandleDamagePacket(reinterpret_cast<Zeal::Packets::Damage_Struct *>(buffer));
        return false;  // Never swallow damage packets.
      },
      callback_type::WorldMessage);

  // Intercept OP_Assist responses (from our synthetic /assist or the user typing it): record the
  // authoritative candidate for the bar. While a silent poll is pending, swallow the response so
  // the client does not retarget us.
  zeal->callbacks->AddPacket(
      [this](UINT opcode, char *buffer, UINT len) {
        if (opcode == Zeal::Packets::Assist && len >= sizeof(Zeal::Packets::EntityId_Struct)) {
          auto pkt = reinterpret_cast<Zeal::Packets::EntityId_Struct *>(buffer);
          const DWORD now = GetTickCount();
          if (pkt->entity_id > 0) {
            assist_response_id = pkt->entity_id;
            assist_response_time = now;
          }
          if (now < suppress_until_ms && pending_suppress_count > 0) {
            --pending_suppress_count;
            return true;  // Swallow: skip native retarget handling (bar already recorded above).
          }
        }
        return false;
      },
      callback_type::WorldMessage);

  zeal->callbacks->AddGeneric([this]() { CallbackRender(); }, callback_type::RenderUI);
  zeal->callbacks->AddGeneric([this]() { Clean(); }, callback_type::EnterZone);
  zeal->callbacks->AddGeneric([this]() { Clean(); }, callback_type::CleanUI);  // Note: new_ui only call.
  zeal->callbacks->AddGeneric([this]() { Clean(); }, callback_type::DXReset);  // Just release all resources.
  zeal->callbacks->AddGeneric([this]() { Clean(); }, callback_type::DXCleanDevice);

  // Chain onto the existing LMouseUp detour (raid_bars owns "LMouseUp" at this address).
  zeal->hooks->Add("AssistTargetLMouseUp", 0x00531614, LMouseUp, hook_type_detour);

  zeal->commands_hook->Add("/assistbar", {}, "Always-up assist target bar (target of my target)",
                           [this](std::vector<std::string> &args) {
                             ParseArgs(args);
                             return true;
                           });
}

AssistTarget::~AssistTarget() {}

void AssistTarget::Clean() {
  candidate_entity = nullptr;
  drag_active = false;
  lmb_was_down = false;
  assist_response_id = 0;
  assist_response_time = 0;
  last_poll_time = 0;
  pending_suppress_count = 0;
  suppress_until_ms = 0;
  tracked_target_id = 0;
  last_change_poll_time = 0;
  victim_of.clear();
  hit_by.clear();
  if (bitmap_font) {
    bitmap_font->release();
    bitmap_font.reset();
  }
}

void AssistTarget::HandleDamagePacket(const Zeal::Packets::Damage_Struct *dmg) {
  if (!dmg || dmg->target == 0 || dmg->source == 0) return;
  const DWORD now = GetTickCount();
  victim_of[dmg->source] = {dmg->target, now};  // attacker -> most recent victim it damaged.
  hit_by[dmg->target] = {dmg->source, now};     // victim -> most recent attacker that damaged it.

  if (setting_verbose.get()) {
    auto src = Zeal::Game::get_entity_by_id(dmg->source);
    auto dst = Zeal::Game::get_entity_by_id(dmg->target);
    const char *src_name = src ? src->Name : "?";
    const char *dst_name = dst ? dst->Name : "?";
    Zeal::Game::print_chat("AssistBar: %s -> %s (%d)", src_name, dst_name, dmg->damage);
  }

  // Opportunistic prune of stale entries (map stays small; only visible combatants).
  if (victim_of.size() + hit_by.size() > 64) {
    const DWORD window = static_cast<DWORD>(setting_window_ms.get());
    for (auto it = victim_of.begin(); it != victim_of.end();)
      it = (now - it->second.timestamp_ms > window) ? victim_of.erase(it) : std::next(it);
    for (auto it = hit_by.begin(); it != hit_by.end();)
      it = (now - it->second.timestamp_ms > window) ? hit_by.erase(it) : std::next(it);
  }
}

void AssistTarget::FireAssistRequest(bool suppress_retarget) {
  if (!Zeal::Game::is_in_game()) return;
  auto self = Zeal::Game::get_self();
  auto target = Zeal::Game::get_target();
  if (!self || !target) return;

  const DWORD now = GetTickCount();
  if (suppress_retarget) {
    ++pending_suppress_count;  // Each in-flight request owes us one swallowed response, so a
    suppress_until_ms = now + 5000;  // double-pressed hotkey or poll+hotkey overlap can't leak
                                    // through and retarget on the second answer.
  } else {
    pending_suppress_count = 0;     // Manual refresh always behaves like typing /assist: its
    suppress_until_ms = 0;          // response must reach native retarget handling.
  }

  auto do_assist_fn = reinterpret_cast<void (*)(Zeal::GameStructures::Entity *, const char *)>(0x004fd7dc);
  do_assist_fn(self, "");
}

// Display-only name cleanup: strip trailing digits (and any spaces left behind), so e.g.
// "Ghorga123" or "Ghorga 123" draw as "Ghorga". Purely cosmetic - the click-to-target path
// still uses the real entity pointer, and names made entirely of digits keep their original form.
static std::string SanitizeDisplayName(const char *name) {
  if (!name || !*name) return {};
  std::string out(name);
  while (!out.empty() && out.back() >= '0' && out.back() <= '9') out.pop_back();
  while (!out.empty() && (out.back() == ' ')) out.pop_back();
  return out.empty() ? std::string(name) : out;
}

void AssistTarget::CallbackRender() {
  candidate_entity = nullptr;
  if (!setting_enabled.get() || !Zeal::Game::is_in_game()) return;
  auto display = Zeal::Game::get_display();
  if (!display || !Zeal::Game::is_gui_visible()) return;

  const DWORD now = GetTickCount();
  auto target = Zeal::Game::get_target();
  const bool have_target = (target && target != Zeal::Game::get_self());

  // Auto-refresh: poll the server for an authoritative answer at a fixed interval. Responses are
  // swallowed so this does not retarget you (unlike typing /assist). Requires a real target.
  bool poll_fired_this_frame = false;
  if (have_target && setting_auto_refresh.get()) {
    const DWORD interval = std::max(5000, setting_refresh_interval_ms.get());
    if (now - last_poll_time >= interval) {
      last_poll_time = now;
      FireAssistRequest(true);
      poll_fired_this_frame = true;
    }
  }

  // Target-change polling: retargeting invalidates Source B (that answer was about a DIFFERENT
  // target), so clear it immediately - the bar shows "none" until a fresh response arrives rather
  // than flashing a stale ToT. Then fire an immediate suppressed request for the new target so the
  // ToT is authoritative within one server round-trip instead of waiting for damage or the next
  // scheduled poll. A cooldown keeps rapid cycling (/ta, etc.) from spamming requests; skipped
  // polls leave Source B cleared until something fresh arrives.
  if (have_target) {
    if (target->SpawnId != tracked_target_id) {
      tracked_target_id = target->SpawnId;
      assist_response_id = 0;
      assist_response_time = 0;
      if (!poll_fired_this_frame && now - last_change_poll_time >= kTargetChangePollCooldownMs) {
        last_change_poll_time = now;
        last_poll_time = now;  // Resync auto-refresh cadence so it does not double-poll.
        FireAssistRequest(true);
      }
    }
  } else {
    tracked_target_id = 0;  // Self/no target: the next real retarget fires again from scratch.
  }

  // Pick the freshest candidate from either source.
  WORD candidate_id = 0;
  DWORD candidate_ts = 0;
  if (have_target) {
    const DWORD window = static_cast<DWORD>(setting_window_ms.get());

    // Source A: damage inference (who my target last hit / who last hit my target).
    auto &map = (setting_mode.get() == 1) ? hit_by : victim_of;
    auto it = map.find(target->SpawnId);
    if (it != map.end()) {
      const DWORD age = now - it->second.timestamp_ms;
      if (age <= window && it->second.timestamp_ms > candidate_ts) {
        candidate_id = it->second.other_id;
        candidate_ts = it->second.timestamp_ms;
      }
    }

    // Source B: authoritative OP_Assist response. Fresh for the damage window, or longer when
    // auto-refresh is on so the bar stays up between polls.
    if (assist_response_time != 0) {
      const DWORD assist_window = setting_auto_refresh.get()
                                      ? std::max(window, static_cast<DWORD>(setting_refresh_interval_ms.get()) + 5000)
                                      : window;
      const DWORD age = now - assist_response_time;
      if (age <= assist_window && assist_response_time > candidate_ts) {
        candidate_id = assist_response_id;
        candidate_ts = assist_response_time;
      }
    }
  }

  auto entity = (have_target && candidate_id) ? Zeal::Game::get_entity_by_id(candidate_id) : nullptr;
  if (entity && (entity->StructType != 0x03 ||
                 entity->Type == Zeal::GameEnums::NPCCorpse || entity->Type == Zeal::GameEnums::PlayerCorpse))
    entity = nullptr;

  LoadBitmapFont();
  if (!bitmap_font || !bitmap_font->is_valid()) return;

  // Always-visible bar: the label prefix tells the user what it is and where it lives. When no
  // fresh candidate exists, a dim placeholder keeps the element in place (and still draggable).
  const char *label = (setting_mode.get() == 1) ? "HitBy: " : "ToT: ";
  std::string line1;
  D3DCOLOR color = D3DCOLOR_XRGB(255, 255, 255);
  if (entity) {
    int hp_percent = 0;
    if (entity->HpMax > 0) hp_percent = static_cast<int>((float)entity->HpCurrent / entity->HpMax * 100.0f);
    bitmap_font->set_hp_percent(hp_percent);
    line1 = std::string(label) + SanitizeDisplayName(entity->Name);  // Cosmetic only: click-to-target
                                                                     // uses the real entity pointer.
  } else {
    color = D3DCOLOR_XRGB(160, 160, 160);  // Dim placeholder.
    line1 = std::string(label) + "none";
  }

  const char healthbar[4] = {'\n', BitmapFontBase::kStatsBarBackground, BitmapFontBase::kHealthBarValue, 0};
  std::string full_text = line1;
  if (entity) full_text += healthbar;  // Placeholder is a single text line.

  const float x = static_cast<float>(setting_position_left.get());
  const float y = static_cast<float>(setting_position_top.get());
  bitmap_font->queue_string(full_text.c_str(), Vec3(x, y, 0), false, color);

  // Cache the drawn rect for click/drag handling. measure_string() doesn't support multi-lines,
  // so measure line 1 only (same approach as raid_bars). The placeholder caches a rect too, so
  // the empty bar can be dragged; clicking it does nothing (candidate_entity stays nullptr).
  candidate_entity = entity;
  candidate_x = x;
  candidate_y = y;
  auto line_size = bitmap_font->measure_string(line1.c_str());
  const float bar_w = entity ? stats_bar_width + 5.f : 0.f;
  const float bar_h = entity ? stats_bar_height + 2.f : 0.f;
  candidate_width = std::max(line_size.x + 0.25f, bar_w);
  candidate_height = std::max(bitmap_font->get_text_height(full_text) + 0.25f, bar_h);

  bitmap_font->flush_queue_to_screen();

  UpdateDrag();  // LMB drag & drop repositioning (uses this frame's cached rect).
}

// Repositions the bar via LMB drag & drop by polling the client's mouse state each frame (no new
// detours needed; ProcessMouseEvent maintains mouse_client_x/y and is_left_mouse_down). Called at
// the end of CallbackRender so the cached rect from this frame's draw is fresh. A completed drag
// suppresses click-to-target because HandleLMouseUp checks drag_active/drag_moved_px.
void AssistTarget::UpdateDrag() {
  const bool lmb = *Zeal::Game::is_left_mouse_down;

  if (drag_active) {
    if (!lmb || *Zeal::Game::is_right_mouse_look_down) {
      // Released (or RMB mouse-look took over): persist the final position once. HandleLMouseUp
      // normally does this on release; this covers releases that happened while the bar was hidden.
      drag_active = false;
      setting_position_left.set(setting_position_left.get(), true);
      setting_position_top.set(setting_position_top.get(), true);
    } else {
      const int16_t mx = *Zeal::Game::mouse_client_x;
      const int16_t my = *Zeal::Game::mouse_client_y;
      if (mx == 32767 || my == 32767) return;  // Invalid mouse position.
      float nx = static_cast<float>(mx) - grab_offset_x;
      float ny = static_cast<float>(my) - grab_offset_y;
      nx = std::clamp(nx, 0.f, static_cast<float>(Zeal::Game::get_screen_resolution_x()));
      ny = std::clamp(ny, 0.f, static_cast<float>(Zeal::Game::get_screen_resolution_y()));
      drag_moved_px += std::abs(static_cast<int>(nx) - setting_position_left.get()) +
                       std::abs(static_cast<int>(ny) - setting_position_top.get());
      // In-memory only while dragging; persisted once on release.
      setting_position_left.set(static_cast<int>(nx), false);
      setting_position_top.set(static_cast<int>(ny), false);
    }
  } else if (lmb && !lmb_was_down) {
    // Rising LMB edge: begin a drag only when pressing directly on the bar drawn this frame.
    const int16_t mx = *Zeal::Game::mouse_client_x;
    const int16_t my = *Zeal::Game::mouse_client_y;
    if (mx != 32767 && my != 32767) {
      const float x = static_cast<float>(setting_position_left.get());
      const float y = static_cast<float>(setting_position_top.get());
      if (static_cast<float>(mx) >= x && static_cast<float>(mx) < x + candidate_width &&
          static_cast<float>(my) >= y && static_cast<float>(my) < y + candidate_height) {
        drag_active = true;
        grab_offset_x = static_cast<float>(mx) - x;
        grab_offset_y = static_cast<float>(my) - y;
        drag_moved_px = 0;
      }
    }
  }
  lmb_was_down = lmb;
}

void AssistTarget::LoadBitmapFont() {
  if (bitmap_font) return;

  IDirect3DDevice8 *device = ZealService::get_instance()->dx->GetDevice();
  if (!device) return;
  bitmap_font = BitmapFont::create_bitmap_font(*device, kDefaultFont);
  if (!bitmap_font) {
    Zeal::Game::print_chat("AssistBar: failed to load font %s", kDefaultFont);
    setting_enabled.set(false);
    return;
  }

  bitmap_font->set_drop_shadow(true);
  bitmap_font->set_full_screen_viewport(true);  // Allow rendering outside the reduced viewport.

  std::string text("Fakenametotest");  // 14 characters as maximum name length with average chars.
  auto text_only_size = bitmap_font->measure_string(text.c_str());

  float bar_width = std::roundf(text_only_size.x * 0.9);
  bar_width = std::max(10.f, std::min(150.f, bar_width));
  stats_bar_width = bar_width;
  bitmap_font->set_stats_bar_width(bar_width);

  float bar_height = std::roundf(text_only_size.y * 0.7);
  bar_height = std::max(4.f, std::min(50.f, bar_height));
  stats_bar_height = bar_height;
  bitmap_font->set_stats_bar_height(bar_height);
}

bool AssistTarget::HandleLMouseUp(short x, short y) {
  if (!setting_enabled.get() || !setting_clickable.get()) return false;
  if (!candidate_entity) return false;

  // A completed drag ends with LMB release over the bar; consume it so we don't also target.
  if (drag_active && drag_moved_px > kDragClickThresholdPx) {
    drag_active = false;
    setting_position_left.set(setting_position_left.get(), true);
    setting_position_top.set(setting_position_top.get(), true);
    return true;
  }

  // Copy some client call behavior to bail out upon certain conditions.
  if (*reinterpret_cast<int *>(0x007d0254) != 0) return false;   // Waiting for server ack to unfreeze UI.
  if (*reinterpret_cast<BYTE *>(0x007985ea) != 0) return false;  // RMB held down.

  if (x < candidate_x || x > candidate_x + candidate_width || y < candidate_y || y > candidate_y + candidate_height)
    return false;

  // Re-validate the cached entity before targeting it.
  auto entity = Zeal::Game::get_entity_by_id(candidate_entity->SpawnId);
  if (!entity || entity != candidate_entity) return false;
  Zeal::Game::set_target(entity);
  return true;
}

void AssistTarget::ParseArgs(const std::vector<std::string> &args) {
  if (args.size() < 2) {
    Zeal::Game::print_chat("Usage: /assistbar on|off|toggle");
    Zeal::Game::print_chat("       /assistbar position <left> <top>  (or drag the bar with LMB)");
    Zeal::Game::print_chat("       /assistbar mode <assist|defend>");
    Zeal::Game::print_chat("       /assistbar window <ms>");
    Zeal::Game::print_chat("       /assistbar clickable <on|off>");
    Zeal::Game::print_chat("       /assistbar refresh <on|off> [interval_ms]");
    Zeal::Game::print_chat("       /assistbar verbose  (log OP_Damage events)");
    return;
  }

  if (Zeal::String::compare_insensitive(args[1], "on") || Zeal::String::compare_insensitive(args[1], "toggle")) {
    setting_enabled.set(true);
    Zeal::Game::print_chat("AssistBar enabled");
    return;
  }
  if (Zeal::String::compare_insensitive(args[1], "off")) {
    setting_enabled.set(false);
    Zeal::Game::print_chat("AssistBar disabled");
    return;
  }

  if (args.size() >= 4 && Zeal::String::compare_insensitive(args[1], "position")) {
    const int left = atoi(args[2].c_str());
    const int top = atoi(args[3].c_str());
    setting_position_left.set(left);
    setting_position_top.set(top);
    Zeal::Game::print_chat("AssistBar position set to (%d, %d)", left, top);
    return;
  }

  if (args.size() >= 3 && Zeal::String::compare_insensitive(args[1], "mode")) {
    bool assist = Zeal::String::compare_insensitive(args[2], "assist");
    bool defend = !assist && Zeal::String::compare_insensitive(args[2], "defend");
    if (assist || defend) {
      setting_mode.set(defend ? 1 : 0);
      Zeal::Game::print_chat("AssistBar mode: %s", defend ? "defend (who hit my target)" : "assist (target of my target)");
    } else {
      Zeal::Game::print_chat("Invalid mode, use assist or defend");
    }
    return;
  }

  if (args.size() >= 3 && Zeal::String::compare_insensitive(args[1], "window")) {
    const int ms = std::max(1000, std::min(60000, atoi(args[2].c_str())));
    setting_window_ms.set(ms);
    Zeal::Game::print_chat("AssistBar freshness window set to %d ms", ms);
    return;
  }

  if (args.size() >= 3 && Zeal::String::compare_insensitive(args[1], "clickable")) {
    const bool on = Zeal::String::compare_insensitive(args[2], "on");
    setting_clickable.set(on);
    Zeal::Game::print_chat("AssistBar clickable: %s", on ? "ON" : "OFF");
    return;
  }

  if (args.size() >= 3 && Zeal::String::compare_insensitive(args[1], "refresh")) {
    const bool on = Zeal::String::compare_insensitive(args[2], "on");
    setting_auto_refresh.set(on);
    if (on) {
      int interval = setting_refresh_interval_ms.get();
      if (args.size() >= 4) interval = atoi(args[3].c_str());
      interval = std::max(5000, std::min(60000, interval));
      setting_refresh_interval_ms.set(interval);
      Zeal::Game::print_chat("AssistBar auto-refresh ON every %d ms (sends a /assist request each poll)", interval);
    } else {
      Zeal::Game::print_chat("AssistBar auto-refresh OFF");
    }
    return;
  }

  if (Zeal::String::compare_insensitive(args[1], "verbose")) {
    setting_verbose.toggle();
    Zeal::Game::print_chat("AssistBar verbose logging: %s", setting_verbose.get() ? "ON" : "OFF");
    return;
  }

  if (Zeal::String::compare_insensitive(args[1], "refresh-now")) {
    FireAssistRequest(false);  // Behaves exactly like typing /assist.
    Zeal::Game::print_chat("AssistBar: sent assist request for current target");
    return;
  }

  Zeal::Game::print_chat("Unknown command, type /assistbar with no args for usage");
}

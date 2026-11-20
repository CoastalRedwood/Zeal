#define NOMINMAX
#include "assist_target.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "callbacks.h"
#include "commands.h"
#include "game_addresses.h"
#include "game_functions.h"
#include "game_packets.h"
#include "hook_wrapper.h"
#include "string_util.h"
#include "zeal.h"

// Zeal native original-command forwarder, defined in commands.cpp.
void ForwardCommand(std::string cmd);

static_assert(sizeof(Zeal::Packets::Animation_Struct) == 12,
              "Zeal animation packet layout changed; recheck target field offsets");

// Chains onto the existing LMouseUp detour (raid_bars owns "LMouseUp" at this address). Our hook wraps it, so
// calling original() falls through to the raid bars handler and then into the game code.
static void __fastcall LMouseUp(void *game, int unused_edx, short x, short y) {
  auto zeal = ZealService::get_instance();
  if (zeal->assist_target->HandleLMouseUp(x, y)) return;
  zeal->hooks->hook_map["AssistTargetLMouseUp"]->original(LMouseUp)(game, unused_edx, x, y);
}

AssistTarget::AssistTarget(ZealService *zeal) {
  // Observe passive packets and the response to our optional one-in-flight
  // fallback. Only replies expected from our background poll can be swallowed.
  zeal->callbacks->AddPacket(
      [this](UINT opcode, char *buffer, UINT len) {
        if (opcode == Zeal::Packets::Assist &&
            len >= sizeof(Zeal::Packets::EntityId_Struct)) {
          uint16_t source = 0;
          if (hybrid_.receive(source)) {
            OnAssistResponse(reinterpret_cast<const Zeal::Packets::EntityId_Struct *>(buffer));
            assist_response_source_ = source;
            return true;
          }
        }
        if (!setting_enabled.get()) return false;
        if (opcode == Zeal::Packets::Damage && len >= sizeof(Zeal::Packets::Damage_Struct)) {
          HandleDamagePacket(reinterpret_cast<const Zeal::Packets::Damage_Struct *>(buffer));
        } else if (opcode == Zeal::Packets::Animation &&
                   len >= sizeof(Zeal::Packets::Animation_Struct)) {
          HandleAnimationPacket(reinterpret_cast<const Zeal::Packets::Animation_Struct *>(buffer));
        }
        return false;  // Never swallow combat packets or normal assist replies.
      }, callback_type::WorldMessage);

  zeal->callbacks->AddGeneric([this]() { CallbackRender(); }, callback_type::RenderUI);
  zeal->callbacks->AddGeneric([this]() { Clean(); }, callback_type::EnterZone);
  zeal->callbacks->AddGeneric([this]() { ReleaseDisplay(); }, callback_type::CleanUI);
  zeal->callbacks->AddGeneric([this]() { ReleaseDisplay(); }, callback_type::DXReset);
  zeal->callbacks->AddGeneric([this]() { ReleaseDisplay(); }, callback_type::DXCleanDevice);

#if 0 // Temporarily disable until lmb drag conflict with mouselook is sorted out.
  zeal->hooks->Add("AssistTargetLMouseUp", 0x00531614, LMouseUp, hook_type_detour);
#endif

  zeal->commands_hook->Add("/assistbar", {}, "Hybrid Target-of-Target (selection assist + passive animation + idle fallback)",
                           [this](std::vector<std::string> &args) {
                             ParseArgs(args);
                             return true;
                           });
}

AssistTarget::~AssistTarget() {}

void AssistTarget::ReleaseDisplay() {
  candidate_entity = nullptr;
  drag_active = false;
  lmb_was_down = false;
  if (bitmap_font) {
    bitmap_font->release();
    bitmap_font.reset();
  }
}

void AssistTarget::Clean() {
  // Only zone transitions reset Spawn IDs and network response associations.
  ReleaseDisplay();
  observations_.clear();
  hybrid_.reset();
  assist_response_id_ = 0;
  assist_response_source_ = 0;
  assist_response_time_ = 0;
}

bool AssistTarget::ValidLivingEntity(WORD id) {
  if (!id || id == 0xffff) return false;
  auto ent = Zeal::Game::get_entity_by_id(id);
  return ent && ent->StructType == 0x03 && ent->Type != Zeal::GameEnums::NPCCorpse &&
         ent->Type != Zeal::GameEnums::PlayerCorpse;
}

void AssistTarget::HandleDamagePacket(const Zeal::Packets::Damage_Struct *dmg) {
  if (!dmg || !setting_enabled.get()) return;
  // Only physical attack packets: spells, DoTs, procs and damage shields may
  // indicate a victim but not the attacker's current selected target.
  if (dmg->spellid >= 0 || !ValidLivingEntity(dmg->source) || !ValidLivingEntity(dmg->target) ||
      dmg->source == dmg->target) return;
  const DWORD now = GetTickCount();
  observations_.observe(dmg->source, dmg->target, PassiveTargetTracker::Evidence::Melee, now);
  // Only a NEW combat event by our actual selected target resets its idle timer.
  // Other nearby attackers cannot indefinitely suppress the assist fallback.
  auto selected = Zeal::Game::get_target();
  if (setting_mode.get() == 0 && setting_hybrid_fallback.get() && selected &&
      selected != Zeal::Game::get_self() && selected->SpawnId == dmg->source) {
    SyncSelection(selected->SpawnId, now);
    hybrid_.observed_passive(dmg->source, now);
  }

}

void AssistTarget::HandleAnimationPacket(const Zeal::Packets::Animation_Struct *anim) {
  if (!anim || !setting_animation_enabled.get()) return;
  if (!ValidLivingEntity(anim->spawnid) || !ValidLivingEntity(anim->target) ||
      anim->spawnid == anim->target) return;
  const DWORD now = GetTickCount();
  // Zeal's packet declaration has `target` and `action`, but whether those
  // carry reliable targeting data must be validated against live observations.
  // Accept only suspected melee swing animations, not emotes/movement/hit reactions.
  const bool suspected_swing = anim->action >= 1 && anim->action <= 11;
  if (setting_animation_enabled.get() && suspected_swing) {
    observations_.observe(anim->spawnid, anim->target,
                          PassiveTargetTracker::Evidence::Animation, now);
    auto selected = Zeal::Game::get_target();
    if (setting_mode.get() == 0 && setting_hybrid_fallback.get() && selected &&
        selected != Zeal::Game::get_self() && selected->SpawnId == anim->spawnid) {
      SyncSelection(selected->SpawnId, now);
      hybrid_.observed_passive(anim->spawnid, now);
    }
  }

}

void AssistTarget::SyncSelection(WORD id, DWORD now) {
  if (!hybrid_.selection(id, now)) return;
  // Ordinary refresh has no immediate-on-selection assist. Start its interval
  // from the new selection instead of leaving the hybrid initial request due.
  if (id && !setting_hybrid_fallback.get()) hybrid_.observed_passive(id, now);
  // Old assist replies and displayed candidates must not be reused after
  // switching targets (or clearing the selection).
  assist_response_id_ = 0;
  assist_response_source_ = 0;
  assist_response_time_ = 0;
  candidate_entity = nullptr;
}

void AssistTarget::OnAssistResponse(const Zeal::Packets::EntityId_Struct *pkt) {
  const DWORD now = GetTickCount();
  // Negative return means no target / out of range; do not reuse a stale result.
  assist_response_id_ = (pkt && pkt->entity_id > 0) ? static_cast<WORD>(pkt->entity_id) : 0;
  assist_response_time_ = now;
}

bool AssistTarget::DeferTypedAssist(const std::vector<std::string>& args) {
  if (args.empty() || (!hybrid_.pending() &&
      (!setting_enabled.get() || !setting_hybrid_fallback.get()))) return false;
  // Preserve the base game's /assist on|off behavior.
  if (args.size() > 1 && (Zeal::String::compare_insensitive(args[1], "on") ||
                          Zeal::String::compare_insensitive(args[1], "off"))) return false;
  std::string command = "/assist";
  for (size_t i = 1; i < args.size(); ++i) command += " " + args[i];
  return hybrid_.defer_manual(command, GetTickCount());
}

bool AssistTarget::DeferKeybindAssist() {
  if (!hybrid_.pending() && (!setting_enabled.get() || !setting_hybrid_fallback.get())) return false;
  return hybrid_.defer_manual(std::string(1, '\x01'), GetTickCount());
}

void AssistTarget::RunDeferredManualAssist() {
  if (hybrid_.pending() || !hybrid_.has_deferred_manual()) return;
  const std::string command = hybrid_.take_deferred_manual();
  if (!Zeal::Game::is_in_game()) return;
  auto self = Zeal::Game::get_self();
  if (!self) return;
  if (command == std::string(1, '\x01')) {
    auto native_assist = reinterpret_cast<void (*)(Zeal::GameStructures::Entity *, const char *)>(0x004fd7dc);
    native_assist(self, "");
  } else {
    // Forward directly into the original /assist interpreter. Do not reenter
    // Zeal's /assist hook and accidentally re-defer this same command.
    ForwardCommand(command);
  }
}

void AssistTarget::FireAssistRequest(bool suppress_retarget) {
  // The Assist Refresh keybind is a silent update only in explicit hybrid mode.
  // Never send a native unsuppressed assist through the bar; /assist handles that.
  if (!suppress_retarget || !setting_enabled.get() || !setting_hybrid_fallback.get() ||
      !Zeal::Game::is_in_game() || setting_mode.get() != 0) return;
  auto target = Zeal::Game::get_target();
  auto self = Zeal::Game::get_self();
  if (!target || !self || target == self) return;
  const DWORD now = GetTickCount();
  SyncSelection(target->SpawnId, now);
  const DWORD interval = static_cast<DWORD>(std::clamp(setting_refresh_interval_ms.get(), 500, 60000));
  if (!hybrid_.try_start_initial(target->SpawnId, now) &&
      !hybrid_.try_start_idle(target->SpawnId, now, interval)) return;
  auto native_assist = reinterpret_cast<void (*)(Zeal::GameStructures::Entity *, const char *)>(0x004fd7dc);
  native_assist(self, "");
}

// Sprite fonts available under uifiles/zeal/fonts (arial_NN).
static bool IsAvailableFontSize(int size) {
  for (const int s : {8, 9, 10, 12, 14, 16, 20, 24, 28, 32})
    if (s == size) return true;
  return false;
}

void AssistTarget::CallbackRender() {
  // Resolve in-flight replies and queued manual assists even if the bar has
  // been hidden since sending the background request.
  const DWORD now = GetTickCount();
  hybrid_.check_timeout(now);  // Keep recovery/cooldown; stay silent.
  RunDeferredManualAssist();
  if (!Zeal::Game::is_in_game()) return;
  if (!setting_enabled.get()) return;
  candidate_entity = nullptr;
  auto target = Zeal::Game::get_target();
  auto self = Zeal::Game::get_self();
  const bool have_target = (target && self && target != self);
  SyncSelection(have_target ? target->SpawnId : 0, now);
  if (have_target && !setting_hybrid_fallback.get() && hybrid_.initial_due())
    hybrid_.observed_passive(target->SpawnId, now);

  // Both modes share the same one-in-flight request gate. Hybrid sends one
  // initial assist and waits for silence between combat/animation observations;
  // ordinary refresh sends one assist each configured interval instead.
  // Neither mode changes the player's selected target.
  if (setting_mode.get() == 0 && have_target &&
      (setting_hybrid_fallback.get() || setting_auto_refresh.get())) {
    const DWORD interval = static_cast<DWORD>(std::clamp(setting_refresh_interval_ms.get(), 500, 60000));
    const bool initial = setting_hybrid_fallback.get() && hybrid_.try_start_initial(target->SpawnId, now);
    if (initial || hybrid_.try_start_idle(target->SpawnId, now, interval)) {
      auto native_assist = reinterpret_cast<void (*)(Zeal::GameStructures::Entity *, const char *)>(0x004fd7dc);
      native_assist(self, "");
    }
  }

  auto display = Zeal::Game::get_display();
  if (!display || !Zeal::Game::is_gui_visible()) return;

  PassiveTargetTracker::Observation observation{};
  if (have_target) {
    observation = observations_.find(target->SpawnId, setting_mode.get() == 1, now,
                                     static_cast<DWORD>(std::max(1000, setting_window_ms.get())));
  }

  WORD candidate_id = observation.other_id;
  DWORD candidate_ts = observation.timestamp_ms;
  // Cache each successful response for its original selected Spawn ID. Retargeting
  // while waiting cannot make another entity inherit this response.
  if (have_target && setting_mode.get() == 0 && assist_response_source_ == target->SpawnId &&
      assist_response_time_ && now - assist_response_time_ <= 30000 &&
      (!candidate_id || now - candidate_ts > 3000) && assist_response_id_) {
    candidate_id = assist_response_id_;
    candidate_ts = assist_response_time_;
  }

  auto entity = (have_target && candidate_id)
                    ? Zeal::Game::get_entity_by_id(candidate_id) : nullptr;
  if (entity && (entity->StructType != 0x03 || entity->Type == Zeal::GameEnums::NPCCorpse ||
                 entity->Type == Zeal::GameEnums::PlayerCorpse)) entity = nullptr;

  LoadBitmapFont();
  if (!bitmap_font || !bitmap_font->is_valid()) return;

  // The bar label identifies the target. When no
  // fresh candidate exists, a dim placeholder keeps the element in place (and still draggable).
  const char *label = (setting_mode.get() == 1) ? "HitBy~: " : "ToT~: ";
  std::string line1;
  D3DCOLOR color = D3DCOLOR_XRGB(255, 255, 255);
  if (entity) {
    int hp_percent = 0;
    if (entity->HpMax > 0) hp_percent = static_cast<int>((float)entity->HpCurrent / entity->HpMax * 100.0f);
    bitmap_font->set_hp_percent(hp_percent);
    // Client's own proven name cleaning (global buffer - consume immediately, as below).
    const char *clean_name = Zeal::Game::strip_name(entity->Name);
    line1 = std::string(label) + (*clean_name ? clean_name : entity->Name);
    // Cosmetic label only; the display never changes the player's target.
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
#if 0 // Temporarily disable until lmb drag conflict with mouselook is sorted out.
  UpdateDrag();  // LMB drag & drop repositioning (uses this frame's cached rect).
#endif
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
  // Font size is a setting (arial_NN sprite fonts); fall back to the default for invalid values.
  const int font_size = IsAvailableFontSize(setting_font_size.get()) ? setting_font_size.get() : 8;
  char font_name[16];
  std::snprintf(font_name, sizeof(font_name), "arial_%02d", font_size);
  bitmap_font = BitmapFont::create_bitmap_font(*device, std::string(font_name));
  if (!bitmap_font) {
    setting_enabled.set(false);
    return;
  }
  bitmap_font->set_drop_shadow(true);
  bitmap_font->set_outlined(true);  // Black outline keeps the bar readable over busy backgrounds.
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

// Bar visibility and hybrid targeting are independent settings.
// Routine feature activity is silent; bare /assistbar still displays usage.
void AssistTarget::ParseArgs(const std::vector<std::string> &args) {
  if (args.size() < 2) {
    Zeal::Game::print_chat("Usage: /assistbar on | off | toggle");
    Zeal::Game::print_chat("       /assistbar hybrid on | off");
    Zeal::Game::print_chat("       /assistbar refresh on <ms> | off");
    Zeal::Game::print_chat("       /assistbar mode assist | defend");
    Zeal::Game::print_chat("       /assistbar position <left> <top> | font <size> | window <ms>");
    return;
  }
  if (Zeal::String::compare_insensitive(args[1], "hybrid")) {
    if (args.size() != 3) {
      Zeal::Game::print_chat("Usage: /assistbar hybrid on | off");
      return;
    }
    const bool on = Zeal::String::compare_insensitive(args[2], "on");
    const bool off = Zeal::String::compare_insensitive(args[2], "off");
    if (!on && !off) {
      Zeal::Game::print_chat("Usage: /assistbar hybrid on | off");
      return;
    }
    if (on == setting_hybrid_fallback.get()) {
      // Confirm repeat commands without resetting a custom refresh interval.
      Zeal::Game::print_chat("AssistBar hybrid is already %s (delay %d ms).",
                             on ? "ON" : "OFF", setting_refresh_interval_ms.get());
      return;
    }

    // Do not abandon a request that has already been transmitted. Its reply
    // contains no request ID and must be drained by the existing callback.
    if (!hybrid_.pending()) hybrid_.reset();
    else hybrid_.selection(0, GetTickCount());  // Cancel any UNSENT initial request.
    observations_.clear();
    assist_response_id_ = 0;
    assist_response_source_ = 0;
    assist_response_time_ = 0;
    candidate_entity = nullptr;

    if (on) {
      // Hybrid is an ADDITION to AssistBar, not a replacement for its commands.
      // Save original options; don't persist temporary overrides to zeal.ini.
      refresh_before_hybrid_ = setting_auto_refresh.get();
      animation_before_hybrid_ = setting_animation_enabled.get();
      mode_before_hybrid_ = setting_mode.get();
      setting_mode.set(0);  // Target of target, rather than defensive tracking.
      setting_animation_enabled.set(true, false);
      setting_auto_refresh.set(false, false);  // Prevent a competing legacy poller.
      setting_refresh_interval_ms.set(1000);  // One second until inactivity fallback.
      setting_hybrid_fallback.set(true);
    } else {
      setting_hybrid_fallback.set(false);
      setting_animation_enabled.set(animation_before_hybrid_, false);
      setting_auto_refresh.set(refresh_before_hybrid_);  // Restore ordinary refresh state.
      if (setting_mode.get() == 0 && mode_before_hybrid_ != 0)
        setting_mode.set(mode_before_hybrid_);
      // Deliberately do NOT alter RefreshIntervalMs: keep the latest value.
    }
    // /assistbar on|off continues to control only bar visibility.
    Zeal::Game::print_chat("AssistBar hybrid: %s (delay %d ms).",
                           on ? "ON" : "OFF", setting_refresh_interval_ms.get());
    return;
  }

  // Original visibility commands control only whether the bar is displayed.
  if (args.size() == 2 && Zeal::String::compare_insensitive(args[1], "on")) {
    setting_enabled.set(true);
    Zeal::Game::print_chat("AssistBar: ON");
    return;
  }
  if (args.size() == 2 && Zeal::String::compare_insensitive(args[1], "off")) {
    setting_enabled.set(false);
    observations_.clear();
    // Reset the selected target, but retain any request already sent so its
    // eventual reply cannot be mistaken for a different command.
    SyncSelection(0, GetTickCount());
    candidate_entity = nullptr;
    Zeal::Game::print_chat("AssistBar: OFF");
    return;
  }
  if (args.size() == 2 && Zeal::String::compare_insensitive(args[1], "toggle")) {
    const bool on = !setting_enabled.get();
    setting_enabled.set(on);
    if (!on) {
      observations_.clear();
      SyncSelection(0, GetTickCount());
      candidate_entity = nullptr;
    }
    Zeal::Game::print_chat("AssistBar: %s", on ? "ON" : "OFF");
    return;
  }
  if (args.size() == 4 && Zeal::String::compare_insensitive(args[1], "position")) {
    setting_position_left.set(atoi(args[2].c_str()));
    setting_position_top.set(atoi(args[3].c_str()));
    Zeal::Game::print_chat("AssistBar position: %d, %d.",
                           setting_position_left.get(), setting_position_top.get());
    return;
  }
  if (args.size() == 3 && Zeal::String::compare_insensitive(args[1], "font")) {
    const int size = atoi(args[2].c_str());
    if (!IsAvailableFontSize(size)) {
      Zeal::Game::print_chat("AssistBar: invalid font size %d.", size);
      return;
    }
    setting_font_size.set(size);
    if (bitmap_font) {
      bitmap_font->release();
      bitmap_font.reset();
    }
    Zeal::Game::print_chat("AssistBar font size: %d.", setting_font_size.get());
    return;
  }
  if (args.size() == 3 && Zeal::String::compare_insensitive(args[1], "mode")) {
    if (Zeal::String::compare_insensitive(args[2], "assist")) setting_mode.set(0);
    else if (Zeal::String::compare_insensitive(args[2], "defend")) setting_mode.set(1);
    else {
      Zeal::Game::print_chat("Usage: /assistbar mode assist | defend");
      return;
    }
    Zeal::Game::print_chat("AssistBar mode: %s.", setting_mode.get() == 0 ? "assist" : "defend");
    return;
  }
  if (args.size() == 3 && Zeal::String::compare_insensitive(args[1], "window")) {
    setting_window_ms.set(std::clamp(atoi(args[2].c_str()), 1000, 60000));
    Zeal::Game::print_chat("AssistBar evidence window: %d ms.", setting_window_ms.get());
    return;
  }
  // Keep the original refresh on/off and millisecond control. While hybrid
  // runs, a changed interval affects the next quiet-time fallback immediately;
  // we remember the desired ordinary refresh state without enabling its poller.
  if (args.size() >= 3 && Zeal::String::compare_insensitive(args[1], "refresh")) {
    const bool hybrid_active = setting_hybrid_fallback.get();
    if (args.size() == 3 && Zeal::String::compare_insensitive(args[2], "off")) {
      if (hybrid_active) refresh_before_hybrid_ = false;
      else setting_auto_refresh.set(false);
      Zeal::Game::print_chat(hybrid_active
          ? "AssistBar normal refresh: OFF after hybrid (hybrid still active at %d ms)."
          : "AssistBar refresh: OFF (delay retained at %d ms).",
          setting_refresh_interval_ms.get());
      return;
    }
    if (Zeal::String::compare_insensitive(args[2], "on") && (args.size() == 3 || args.size() == 4)) {
      if (args.size() == 4)
        setting_refresh_interval_ms.set(std::clamp(atoi(args[3].c_str()), 500, 60000));
      if (hybrid_active) refresh_before_hybrid_ = true;
      else setting_auto_refresh.set(true);
      Zeal::Game::print_chat(hybrid_active
          ? "AssistBar hybrid delay: %d ms (normal refresh saved as ON for when hybrid is OFF)."
          : "AssistBar refresh: ON (%d ms).",
          setting_refresh_interval_ms.get());
      return;
    }
    // Support the /assistbar refresh <ms> shorthand from previous test builds.
    // This only changes the interval; it does not change refresh on/off.
    if (args.size() == 3 && !Zeal::String::compare_insensitive(args[2], "off")) {
      const char* value = args[2].c_str();
      char* end = nullptr;
      const long ms = std::strtol(value, &end, 10);
      if (end != value && *end == '\0') {
        setting_refresh_interval_ms.set(static_cast<int>(std::clamp(ms, 500L, 60000L)));
        Zeal::Game::print_chat("AssistBar delay: %d ms.", setting_refresh_interval_ms.get());
        return;
      }
    }
    Zeal::Game::print_chat("Usage: /assistbar refresh on <ms> | off | <ms>");
    return;
  }
  Zeal::Game::print_chat("AssistBar: unknown option. Type /assistbar for commands.");
}

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

static constexpr DWORD kAssistTimeoutMs = 1000;  // Assist requests should get responses from the server in < 1 second.

AssistTarget::AssistTarget(ZealService *zeal) {
  // Monitors outgoing packets to intercept outgoing assist requests to log the source and defer if busy.
  zeal->callbacks->AddPacket(
      [this](UINT opcode, char *buffer, UINT len) { return HandleOutgoingPacket(opcode, buffer, len); },
      callback_type::SendMessage_);

  // Monitors packets to intercept pending assist queries as well as update the observer based on damage / animations.
  zeal->callbacks->AddPacket(
      [this](UINT opcode, char *buffer, UINT len) { return HandleIncomingPacket(opcode, buffer, len); },
      callback_type::WorldMessage);

  // Main processing handler that performs any deferred / refresh processing and renders the results.
  zeal->callbacks->AddGeneric([this]() { CallbackRender(); }, callback_type::RenderUI);

  // Hooks to clean up the state and history.
  zeal->callbacks->AddGeneric([this]() { Clean(); }, callback_type::EnterZone);
  zeal->callbacks->AddGeneric([this]() { Clean(); }, callback_type::CleanUI);  // Note: new_ui only call.
  zeal->callbacks->AddGeneric([this]() { Clean(); }, callback_type::DXReset);  // Just release all resources.
  zeal->callbacks->AddGeneric([this]() { Clean(); }, callback_type::DXCleanDevice);

  zeal->commands_hook->Add("/assistbar", {},
                           "Hybrid Target-of-Target (selection assist + passive animation + idle fallback)",
                           [this](std::vector<std::string> &args) {
                             ParseArgs(args);
                             return true;
                           });
}

AssistTarget::~AssistTarget() { Clean(); }

// Release all resources and reset dynamic state.
void AssistTarget::Clean() {
  if (bitmap_font) {
    bitmap_font->release();
    bitmap_font.reset();
  }

  // Wipe all tracking state.
  internal_send_request_ = false;
  pending_internal_assist_request_ = false;
  trigger_refresh_ = false;
  pending_assistee_id_ = 0;
  internal_refresh_target_id_ = 0;
  deferred_assistee_id_ = 0;
  pending_assist_timestamp_ = 0;
  internal_refresh_timestamp_ = 0;
  deferred_assist_timestamp_ = 0;

  observations_.clear();
}

// Returns true if this handler consumed the packet and the caller should not proceed with it.
bool AssistTarget::HandleOutgoingPacket(UINT opcode, const char *buffer, UINT len) {
  // Ignore if invalid data or assist targeting isn't enabled.
  if (!setting_enabled.get()) return false;

  // Ignore if not an outgoing valid Assist request.
  if (opcode != Zeal::Packets::Assist || !buffer || len != sizeof(Zeal::Packets::EntityId_Struct)) return false;

  const auto *pEntityId = reinterpret_cast<const Zeal::Packets::EntityId_Struct *>(buffer);
  DWORD now = Zeal::Game::get_game_time();

  // For manual requests we need to check if busy. If busy and not timed out, defer.
  if (!internal_send_request_) {
    bool is_busy = (pending_assistee_id_ != 0);
    bool is_timedout = is_busy && ((now - pending_assist_timestamp_) >= kAssistTimeoutMs);
    if (is_busy && !is_timedout) {
      // The assist handshake is busy so queue the request to send later.
      deferred_assistee_id_ = pEntityId->entity_id;
      deferred_assist_timestamp_ = now;
      return true;  // Swallow this outgoing request (defer it).
    }
    deferred_assistee_id_ = 0;  // Going to send this one now so cancel any previously deferred request.
  }

  // Log and let the the outgoing request proceed.
  pending_internal_assist_request_ = internal_send_request_;
  pending_assistee_id_ = pEntityId->entity_id;
  pending_assist_timestamp_ = now;
  return false;
}

bool AssistTarget::HandleIncomingPacket(UINT opcode, const char *buffer, UINT len) {
  if (!buffer || !setting_enabled.get()) return false;

  // The assist handler may swallow the incoming assist response packet.
  if (opcode == Zeal::Packets::Assist && len >= sizeof(Zeal::Packets::EntityId_Struct))
    return HandleAssistPacket(*reinterpret_cast<const Zeal::Packets::EntityId_Struct *>(buffer));

  if (opcode == Zeal::Packets::Damage && len >= sizeof(Zeal::Packets::Damage_Struct)) {
    HandleDamagePacket(*reinterpret_cast<const Zeal::Packets::Damage_Struct *>(buffer));
  } else if (opcode == Zeal::Packets::Animation && len >= sizeof(Zeal::Packets::Animation_Struct)) {
    HandleAnimationPacket(*reinterpret_cast<const Zeal::Packets::Animation_Struct *>(buffer));
  } else if (opcode == Zeal::Packets::TargetHoTT && len >= sizeof(Zeal::Packets::TargetHoTT_Struct)) {
    HandleTargetHoTTPacket(*reinterpret_cast<const Zeal::Packets::TargetHoTT_Struct *>(buffer));
  }
  return false;  // Other packets are monitored passively and not swallowed.
}

// Processes incoming assist responses to update observations and will swallow responses to internal requests.
bool AssistTarget::HandleAssistPacket(const Zeal::Packets::EntityId_Struct &pkt) {
  // Ignore and let client handle if we somehow missed logging the assistee for the outgoing request.
  if (!pending_assistee_id_) return false;

  // Also ignore if the response time is so late something must have gone wrong and the id isn't trustworthy.
  DWORD now = Zeal::Game::get_game_time();
  if ((now - pending_assist_timestamp_) >= kAssistTimeoutMs) {
    pending_assistee_id_ = 0;
    pending_internal_assist_request_ = false;
    return false;
  }

  // If there is a deferred assist request that matches this request response, cancel the no longer
  // needed deferred and also block swapping the response from an internal request.
  bool deferred_match = (deferred_assistee_id_ == pending_assistee_id_);
  bool swallow_response = pending_internal_assist_request_ && !deferred_match;
  if (deferred_match) deferred_assistee_id_ = 0;

  // Anytime we receive an assist update packet with a logged source request we update our
  // tracking map. An entity_id value of -1 means no target / out of range so set to 0.
  auto target_id = (pkt.entity_id != -1) ? static_cast<WORD>(pkt.entity_id) : 0;
  RecordNewObservation(pending_assistee_id_, target_id, PassiveTargetTracker::Evidence::Assist);
  pending_assistee_id_ = 0;
  pending_internal_assist_request_ = false;

  // If this was an internal request just swallow the packet. Otherwise return false so let the response
  // propagate back to the normal client assist handler. The main loop will handle sending any
  // deferred request (should be a < 1/fps delay).
  return swallow_response;
}

// Checks the animation packet for any useful information about mob attacking a target.
void AssistTarget::HandleDamagePacket(const Zeal::Packets::Damage_Struct &dmg) {
  // Only use damage packets for physical attacks as spells, DoTs, procs, and damage shields
  // may indicate a victim that isn't the attacker's current primary target
  if (dmg.spellid >= 0 || dmg.type == 231) return;  // Spell-based, ignore.

  RecordNewObservation(dmg.source, dmg.target, PassiveTargetTracker::Evidence::Melee);
}

// Returns true if a melee-style attack animation (versus emotes/movement/reactions).
static bool IsMeleeDoAnimation(BYTE action) {
  if (action >= 1 && action <= 11)  // Kick, Piercing, etc, through Round Kick.
    return true;

  if (action >= 45 && action <= 47)  // FlyingKick, TigerClaw, EagleStrike.
    return true;

  return false;
}

// Checks the animation packet for any useful information about mob attacking a target.
void AssistTarget::HandleAnimationPacket(const Zeal::Packets::Animation_Struct &anim) {
  // Filter to DoAnim() sent packets with melee animations.
  const bool is_do_animation = (anim.unknown10 == 16256);  // Server DoAnim() sets this value.
  if (!is_do_animation || !IsMeleeDoAnimation(anim.action)) return;

  RecordNewObservation(anim.spawnid, anim.target, PassiveTargetTracker::Evidence::Animation);
}

// Handles the new custom target of target information packet.
void AssistTarget::HandleTargetHoTTPacket(const Zeal::Packets::TargetHoTT_Struct &packet) {
  RecordNewObservation(packet.source_id, packet.target_id, PassiveTargetTracker::Evidence::Assist);
}

// Returns true if the provided spawn_id has a valid id value that references a living entity.
static bool IsValidLivingEntity(WORD id) {
  if (!id || id == 0xffff) return false;
  auto ent = Zeal::Game::get_entity_by_id(id);
  return ent && ent->StructType == 0x03 && ent->Type != Zeal::GameEnums::NPCCorpse &&
         ent->Type != Zeal::GameEnums::PlayerCorpse;
}

void AssistTarget::RecordNewObservation(const WORD assistee_id, const WORD target_id,
                                        const PassiveTargetTracker::Evidence evidence) {
  // Only process observations with valid source and target spawn ids.
  if (assistee_id == target_id || !IsValidLivingEntity(target_id) || !IsValidLivingEntity(assistee_id)) return;
  const DWORD now = Zeal::Game::get_game_time();
  observations_.add_observation(assistee_id, target_id, evidence, now);
}

// Sends assist request to the server.
void AssistTarget::SendAssistRequest(WORD assistee_id, bool internal) {
  ZealService *zeal = ZealService::get_instance();
  Zeal::Packets::EntityId_Struct entity_id{};
  entity_id.entity_id = assistee_id;
  internal_send_request_ = internal;  // Signals if internal or a deferred request in hooked outgoing handler.
  Zeal::Game::send_message(Zeal::Packets::Assist, reinterpret_cast<char *>(&entity_id), sizeof(entity_id), 1);
  internal_send_request_ = false;  // Always reset to non-internal to allow manual packets through.
}

// Handles sending any deferred or internal refresh assist requests.
void AssistTarget::ProcessAssistRequests(WORD target_id) {
  // First reset state if there are any timeouts.
  const DWORD now = Zeal::Game::get_game_time();
  if (pending_assistee_id_ && (now - pending_assist_timestamp_) >= kAssistTimeoutMs) {
    pending_assistee_id_ = 0;
    pending_internal_assist_request_ = false;
  }

  if (deferred_assistee_id_ && (now - deferred_assist_timestamp_) >= kAssistTimeoutMs) deferred_assistee_id_ = 0;

  // If a request is pending, then we are busy so nothing else to do.
  if (pending_assistee_id_) return;

  // First priority is to send out the deferred_assistee_id_ if queued.
  if (deferred_assistee_id_) {
    SendAssistRequest(deferred_assistee_id_, false);  // HandleOutgoingPacket() clears deferred_assistee_id_.
    return;
  }

  // Next check if it is time to send an internal refresh assist request.
  // Do not refreesh if disabled, in defend mode, or do not have a valid target to assist.
  bool allow_refreshing = trigger_refresh_ || setting_auto_refresh.get();
  if (!allow_refreshing || (setting_mode.get() == 1) || !IsValidLivingEntity(target_id)) {
    trigger_refresh_ = false;
    return;
  }

  // Otherwise send a request if a new target or the request refresh interval is exceeded.
  // The refresh interval is disabled with a value of zero but the target change trigger still works.
  DWORD elapsed_time_ms = now - internal_refresh_timestamp_;
  int refresh_interval = setting_refresh_interval_ms.get();
  DWORD threshold_ms = static_cast<DWORD>(std::max(1000, refresh_interval));
  bool refresh_interval_trigger = refresh_interval && (elapsed_time_ms >= threshold_ms);
  if (trigger_refresh_ || (internal_refresh_target_id_ != target_id) || refresh_interval_trigger) {
    trigger_refresh_ = false;
    internal_refresh_target_id_ = target_id;
    internal_refresh_timestamp_ = now;
    SendAssistRequest(target_id, true);  // Send an internal refresh request.
  }
}

const Zeal::GameStructures::Entity *AssistTarget::UpdateAndGetLatestResult() {
  auto target = Zeal::Game::get_target();
  auto self = Zeal::Game::get_self();
  WORD target_id = (target && self && target != self) ? target->SpawnId : 0;

  // First handle processing the assist request/response state.
  ProcessAssistRequests(target_id);

  if (!target_id) return nullptr;

  const DWORD now = Zeal::Game::get_game_time();
  const bool defend_mode = (setting_mode.get() == 1);
  PassiveTargetTracker::Observation observation =
      observations_.find(target_id, defend_mode, now, static_cast<DWORD>(std::max(1000, setting_window_ms.get())));

  WORD candidate_id = observation.target_id;
  auto entity = (candidate_id) ? Zeal::Game::get_entity_by_id(candidate_id) : nullptr;
  if (entity && (entity->StructType != 0x03 || entity->Type == Zeal::GameEnums::NPCCorpse ||
                 entity->Type == Zeal::GameEnums::PlayerCorpse))
    entity = nullptr;

  return entity;
}

// Processing loop that handles sending deferred/refreh requests and drawing the on-screen results.
void AssistTarget::CallbackRender() {
  if (!setting_enabled.get() || !Zeal::Game::is_in_game()) return;

  auto entity = UpdateAndGetLatestResult();

  auto display = Zeal::Game::get_display();
  if (!entity || !display || !Zeal::Game::is_gui_visible()) return;

  LoadBitmapFont();
  if (!bitmap_font || !bitmap_font->is_valid()) return;

  // Generate the text label, the color, and the hp bar percent and then render it.
  const char *label = (setting_mode.get() == 1) ? "HitBy: " : "ToT: ";
  const char healthbar[4] = {'\n', BitmapFontBase::kStatsBarBackground, BitmapFontBase::kHealthBarValue, 0};
  std::string full_text = std::string(label) + Zeal::Game::strip_name(entity->Name) + healthbar;

  // Color the label based on whether we are close enough to receive passive updates. Range:Anims = 135.
  auto self = Zeal::Game::get_self();
  int distance = self ? static_cast<int>(self->Position.Dist2D(entity->Position)) : 0;
  D3DCOLOR color = (distance <= 135) ? D3DCOLOR_XRGB(255, 255, 255) : D3DCOLOR_XRGB(255, 128, 128);

  const float x = static_cast<float>(setting_position_left.get());
  const float y = static_cast<float>(setting_position_top.get());
  int hp_percent = (entity->HpMax > 0) ? static_cast<int>((float)entity->HpCurrent / entity->HpMax * 100.0f) : 0;
  bitmap_font->set_hp_percent(hp_percent);
  bitmap_font->queue_string(full_text.c_str(), Vec3(x, y, 0), false, color);
  bitmap_font->flush_queue_to_screen();
}

// Sprite fonts available under uifiles/zeal/fonts (arial_NN).
static bool IsAvailableFontSize(int size) {
  for (const int s : {8, 9, 10, 12, 14, 16, 20, 24, 28, 32})
    if (s == size) return true;
  return false;
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
  bitmap_font->set_outlined(true);              // Black outline keeps the bar readable over busy backgrounds.
  bitmap_font->set_full_screen_viewport(true);  // Allow rendering outside the reduced viewport.

  std::string text("Fakenametotest");  // 14 characters as maximum name length with average chars.
  auto text_only_size = bitmap_font->measure_string(text.c_str());
  float bar_width = std::roundf(text_only_size.x * 0.9);
  bar_width = std::max(10.f, std::min(150.f, bar_width));
  bitmap_font->set_stats_bar_width(bar_width);

  float bar_height = std::roundf(text_only_size.y * 0.7);
  bar_height = std::max(4.f, std::min(50.f, bar_height));
  bitmap_font->set_stats_bar_height(bar_height);
}

// Bar visibility and hybrid targeting are independent settings.
// Routine feature activity is silent; bare /assistbar still displays usage.
void AssistTarget::ParseArgs(const std::vector<std::string> &args) {
  if (args.size() < 2) {
    Zeal::Game::print_chat("Usage: /assistbar on | off | toggle");
    Zeal::Game::print_chat("       /assistbar mode assist | defend");
    Zeal::Game::print_chat("       /assistbar position <left> <top> | font <size> | window <ms>");
    Zeal::Game::print_chat("       /assistbar font <size>");
    Zeal::Game::print_chat("       /assistbar refresh on <ms> | off");
    Zeal::Game::print_chat("       /assistbar window <time_ms> (sets history window)");
    return;
  }

  if (args.size() == 2 && Zeal::String::compare_insensitive(args[1], "on")) {
    if (!setting_enabled.get()) {
      setting_enabled.set(true);
      Clean();  // Wipe all state before re-enabling.
    }
    Zeal::Game::print_chat("AssistBar: ON");
    return;
  }
  if (args.size() == 2 && Zeal::String::compare_insensitive(args[1], "off")) {
    setting_enabled.set(false);
    Clean();  // Wipe all state.
    Zeal::Game::print_chat("AssistBar: OFF");
    return;
  }
  if (args.size() == 2 && Zeal::String::compare_insensitive(args[1], "toggle")) {
    const bool on = !setting_enabled.get();
    setting_enabled.set(on);
    Clean();  // Wipe all state when toggling.
    Zeal::Game::print_chat("AssistBar: %s", on ? "ON" : "OFF");
    return;
  }

  if (args.size() >= 2 && Zeal::String::compare_insensitive(args[1], "position")) {
    int left = 0;
    int top = 0;
    if (args.size() == 4 && Zeal::String::tryParse(args[2], &left, true) &&
        Zeal::String::tryParse(args[3], &top, true)) {
      setting_position_left.set(left);
      setting_position_top.set(top);
    } else {
      Zeal::Game::print_chat("Usage: /assistbar position <left> <top>");
    }
    Zeal::Game::print_chat("AssistBar position: Left=%d, Top=%d.", setting_position_left.get(),
                           setting_position_top.get());
    return;
  }

  if (args.size() >= 2 && Zeal::String::compare_insensitive(args[1], "font")) {
    int size = 0;
    if (args.size() == 3 && Zeal::String::tryParse(args[2], &size, true)) {
      if (!IsAvailableFontSize(size)) {
        Zeal::Game::print_chat("AssistBar: invalid font size %d  (available: 8, 9, 10, 12, 14, 16, 20, 24, 28, 32).",
                               size);
        return;
      }
      setting_font_size.set(size);
      if (bitmap_font) {  // Reload with the new size on next render.
        bitmap_font->release();
        bitmap_font.reset();
      }
    } else {
      Zeal::Game::print_chat("Usage: /assistbar font <size>");
    }
    Zeal::Game::print_chat("AssistBar font size: %d.", setting_font_size.get());
    return;
  }

  if (args.size() >= 2 && Zeal::String::compare_insensitive(args[1], "mode")) {
    if (args.size() == 3 && Zeal::String::compare_insensitive(args[2], "assist"))
      setting_mode.set(0);
    else if (args.size() == 3 && Zeal::String::compare_insensitive(args[2], "defend"))
      setting_mode.set(1);
    else {
      Zeal::Game::print_chat("Usage: /assistbar mode assist | defend");
      Zeal::Game::print_chat(" assist: target of my target, defend: who hit my target");
    }
    Zeal::Game::print_chat("AssistBar mode: %s.", setting_mode.get() == 0 ? "assist" : "defend");
    return;
  }

  if (args.size() >= 2 && Zeal::String::compare_insensitive(args[1], "window")) {
    int window = 0;
    if (args.size() == 3 && Zeal::String::tryParse(args[2], &window, true))
      setting_window_ms.set(std::clamp(window, 1000, 60000));
    else
      Zeal::Game::print_chat("Usage: /assistbar window <ms>");
    Zeal::Game::print_chat("AssistBar evidence window: %d ms.", setting_window_ms.get());
    return;
  }
  // Keep the original refresh on/off and millisecond control. While hybrid
  // runs, a changed interval affects the next quiet-time fallback immediately;
  // we remember the desired ordinary refresh state without enabling its poller.
  if (args.size() >= 2 && Zeal::String::compare_insensitive(args[1], "refresh")) {
    int interval = 0;
    if (args.size() == 3 && Zeal::String::compare_insensitive(args[2], "off")) {
      setting_auto_refresh.set(false);
    } else if (args.size() >= 3 && Zeal::String::compare_insensitive(args[2], "on") &&
               (args.size() == 3 || (args.size() == 4 && Zeal::String::tryParse(args[3], &interval, true)))) {
      if (args.size() == 4) {
        if (interval) interval = std::clamp(interval, 1000, 60000);  // Allow 0 or 1000 to 60000.
        setting_refresh_interval_ms.set(interval);
      }
      setting_auto_refresh.set(true);
    } else if (args.size() == 3 && Zeal::String::tryParse(args[2], &interval, true)) {
      if (interval) interval = std::clamp(interval, 1000, 60000);  // Allow 0 or 1000 to 60000.
      setting_refresh_interval_ms.set(interval);
    } else {
      Zeal::Game::print_chat(
          "Usage: /assistbar refresh on <ms> | off | <ms>  (set ms = 0 for only auto-refresh on target change)");
    }
    Zeal::Game::print_chat("AssistBar auto-refresh: %s (%d ms).", setting_auto_refresh.get() ? "ON" : "OFF",
                           setting_refresh_interval_ms.get());
    return;
  }
  Zeal::Game::print_chat("AssistBar: unknown option. Type /assistbar with no args for usage.");
}

#pragma once
#include <Windows.h>

#include <string>
#include <unordered_map>

#include "bitmap_font.h"
#include "game_packets.h"
#include "game_structures.h"
#include "zeal_settings.h"

// Displays an always-up bar showing the assist candidate for your current target:
// - Mode 0 (assist): the entity your target most recently damaged ("target of my target").
// - Mode 1 (defend): the entity that most recently damaged your target.
// The legacy client does not store other entities' targets in memory, so this is derived from
// OP_Damage events (Damage_Struct source/target spawn ids) within a configurable freshness window.
class AssistTarget {
 public:
  static constexpr char kUseDefaultFont[] = "Default";
  static constexpr char kDefaultFont[] = "arial_08";

  explicit AssistTarget(class ZealService *zeal);
  ~AssistTarget();

  // Disable copy.
  AssistTarget(AssistTarget const &) = delete;
  AssistTarget &operator=(AssistTarget const &) = delete;

  ZealSetting<bool> setting_enabled = {false, "AssistBar", "Enabled"};
  ZealSetting<bool> setting_clickable = {true, "AssistBar", "Clickable"};
  ZealSetting<int> setting_position_left = {5, "AssistBar", "Left"};
  ZealSetting<int> setting_position_top = {30, "AssistBar", "Top"};
  // Mode: 0 = assist (who my target last hit), 1 = defend (who last hit my target).
  ZealSetting<int> setting_mode = {0, "AssistBar", "Mode"};
  // Freshness window in ms for damage events to count as the current candidate.
  ZealSetting<int> setting_window_ms = {8000, "AssistBar", "WindowMs"};
  // Verbose logs every OP_Damage event to chat (for verifying packet delivery scope).
  ZealSetting<bool> setting_verbose = {false, "AssistBar", "Verbose"};
  // Opt-in continuous polling: periodically send a synthetic /assist request for an authoritative
  // answer. Responses are swallowed so the client does not retarget you.
  ZealSetting<bool> setting_auto_refresh = {false, "AssistBar", "AutoRefresh"};
  // Poll interval in ms (clamped to [5000, 60000]).
  ZealSetting<int> setting_refresh_interval_ms = {10000, "AssistBar", "RefreshIntervalMs"};

  // Internal callback use only.
  bool HandleLMouseUp(short x, short y);

  // Fires a synthetic /assist (client do_assist with empty name = current target).
  // suppress_retarget=true swallows the OP_Assist response so the client does not retarget you
  // (used by auto-refresh polling and the AssistRefresh hotkey: bar updates, your target stays).
  // false behaves exactly like typing /assist (the response retargets you to the assist entity).
  void FireAssistRequest(bool suppress_retarget);

 private:
  struct HitRecord {
    WORD other_id = 0;      // The counterpart spawn id (victim or attacker).
    DWORD timestamp_ms = 0;  // GetTickCount() when recorded.
  };

  void Clean();  // Resets state and releases all resources.
  void ParseArgs(const std::vector<std::string> &args);
  void LoadBitmapFont();  // Loads the bitmap font for rendering.
  void CallbackRender();  // Displays the assist bar.
  void UpdateDrag();      // LMB drag & drop repositioning, polled at the end of CallbackRender.
  void HandleDamagePacket(const Zeal::Packets::Damage_Struct *dmg);

  std::unique_ptr<BitmapFont> bitmap_font = nullptr;
  float stats_bar_width = 0;
  float stats_bar_height = 0;

  // victim_of: attacker spawn id -> most recent victim it damaged.
  // hit_by:    victim spawn id -> most recent attacker that damaged it.
  std::unordered_map<WORD, HitRecord> victim_of;
  std::unordered_map<WORD, HitRecord> hit_by;

  // Authoritative candidate from the last OP_Assist response (synthetic or user-typed /assist).
  WORD assist_response_id = 0;
  DWORD assist_response_time = 0;  // GetTickCount() of the last response.

  // Auto-refresh polling state.
  DWORD last_poll_time = 0;
  DWORD suppress_until_ms = 0;    // While now < this AND pending count > 0, swallow OP_Assist responses.
  int pending_suppress_count = 0; // In-flight suppressed requests still owed a swallowed response.

  // Target-change polling: an immediate suppressed request fires whenever the target changes so
  // the ToT is authoritative within one server round-trip of each retarget (see CallbackRender).
  WORD tracked_target_id = 0;      // Spawn id last polled for (0 = none / not self).
  DWORD last_change_poll_time = 0; // Timestamp of the last target-change poll.
  static constexpr DWORD kTargetChangePollCooldownMs = 2000;  // Rate limit so rapid cycling
                                                              // (e.g. /ta) does not spam requests.

  // Cached from the last render for click handling (nullptr when nothing is drawn).
  Zeal::GameStructures::Entity *candidate_entity = nullptr;
  float candidate_x = 0;
  float candidate_y = 0;
  float candidate_width = 0;
  float candidate_height = 0;

  // LMB drag & drop state (polled in UpdateDrag, consumed by HandleLMouseUp).
  static constexpr int kDragClickThresholdPx = 4;  // Displacement below which press+release is a click.
  bool drag_active = false;
  bool lmb_was_down = false;  // Previous frame's LMB state for edge detection.
  float grab_offset_x = 0;    // Mouse-to-bar-top-left offset captured at drag start.
  float grab_offset_y = 0;
  int drag_moved_px = 0;      // Total displacement since drag start (distinguishes click from drag).
};

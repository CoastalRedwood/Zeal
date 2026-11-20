#pragma once
#include <Windows.h>

#include <string>

#include "bitmap_font.h"
#include "game_packets.h"
#include "game_structures.h"
#include "zeal_settings.h"
#include "passive_target_tracker.h"
#include "hybrid_assist_gate.h"

// Hybrid Target-of-Target: passive physical combat evidence first, optionally
// one serialized synthetic /assist when evidence is idle. Opt-in experimental
// fallback is not a guarantee against collisions with every native assist path.
class AssistTarget {
 public:
  explicit AssistTarget(class ZealService *zeal);
  ~AssistTarget();

  // Disable copy.
  AssistTarget(AssistTarget const &) = delete;
  AssistTarget &operator=(AssistTarget const &) = delete;

  ZealSetting<bool> setting_enabled = {false, "AssistBar", "Enabled"};
  ZealSetting<bool> setting_clickable = {true, "AssistBar", "Clickable"};
  ZealSetting<int> setting_position_left = {5, "AssistBar", "Left"};
  ZealSetting<int> setting_position_top = {30, "AssistBar", "Top"};
  // Font size for the bar text (arial_NN sprite fonts); see IsAvailableFontSize().
  ZealSetting<int> setting_font_size = {16, "AssistBar", "FontSize"};
  // Mode: 0 = assist (who my target last hit), 1 = defend (who last hit my target).
  ZealSetting<int> setting_mode = {0, "AssistBar", "Mode"};
  // Freshness window in ms for damage events to count as the current candidate.
  ZealSetting<int> setting_window_ms = {8000, "AssistBar", "WindowMs"};
  ZealSetting<bool> setting_animation_enabled = {false, "AssistBar", "AnimationTarget"};
  // Hybrid fallback is session-only; the legacy refresh state is kept off in hybrid mode.
  ZealSetting<bool> setting_auto_refresh = {false, "AssistBar", "AutoRefresh"};
  ZealSetting<bool> setting_hybrid_fallback = {false};  // Session-only; explicit opt-in every launch.
  bool refresh_before_hybrid_ = false;    // Restore legacy refresh setting when hybrid turns off.
  bool animation_before_hybrid_ = false;  // Restore the user's original animation option.
  int mode_before_hybrid_ = 0;           // Restore defend mode if hybrid temporarily changed it.
  // Quiet-time / retry interval in ms (clamped to [500, 60000]).
  ZealSetting<int> setting_refresh_interval_ms = {10000, "AssistBar", "RefreshIntervalMs"};

  // Internal callback use only.
  bool HandleLMouseUp(short x, short y);

  // Existing Assist Refresh keybind: silent only if hybrid fallback explicitly enabled.
  void FireAssistRequest(bool suppress_retarget);
  // Called by Zeal's normal /assist hook and Assist keybind to avoid queuing
  // those two manual entry paths behind a synthetic request.
  bool DeferTypedAssist(const std::vector<std::string>& args);
  bool DeferKeybindAssist();

 private:
  void Clean();  // Clear Spawn ID state on zoning.
  void ReleaseDisplay();  // UI reset: do not drop an in-flight server reply.
  void ParseArgs(const std::vector<std::string> &args);
  void LoadBitmapFont();  // Loads the bitmap font for rendering.
  void CallbackRender();  // Displays the assist bar.
  void UpdateDrag();      // LMB drag & drop repositioning, polled at the end of CallbackRender.
  void HandleDamagePacket(const Zeal::Packets::Damage_Struct *dmg);
  void HandleAnimationPacket(const Zeal::Packets::Animation_Struct *animation);
  void SyncSelection(WORD id, DWORD now);  // Reset cached ToT on a genuine target change.
  static bool ValidLivingEntity(WORD id);

  std::unique_ptr<BitmapFont> bitmap_font = nullptr;
  float stats_bar_width = 0;
  float stats_bar_height = 0;

  PassiveTargetTracker observations_;
  HybridAssistGate hybrid_;
  WORD assist_response_id_ = 0;
  WORD assist_response_source_ = 0;
  DWORD assist_response_time_ = 0;
  void OnAssistResponse(const Zeal::Packets::EntityId_Struct *packet);
  void RunDeferredManualAssist();


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

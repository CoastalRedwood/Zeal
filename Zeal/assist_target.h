#pragma once
#include <Windows.h>

#include <string>
#include <unordered_map>

#include "bitmap_font.h"
#include "game_packets.h"
#include "game_structures.h"
#include "zeal_settings.h"

// Internal helper class for tracking timestamped actor(assistee)->target pairs.
class PassiveTargetTracker {
 public:
  enum class Evidence : unsigned char { Assist, Melee, Animation };

  // Hash map entry.
  struct Observation {
    DWORD timestamp_ms = 0;
    WORD target_id = 0;
    Evidence evidence = Evidence::Melee;
  };

  void clear() { by_actor_.clear(); }

  void add_observation(WORD actor, WORD victim, Evidence evidence, DWORD now_ms) {
    if (!actor || !victim || actor == victim || actor == 0xffff || victim == 0xffff) return;
    by_actor_[actor] = Observation{now_ms, victim, evidence};
  }

  const Observation find(WORD selected, bool defend, DWORD now_ms, DWORD freshness_ms) const {
    if (!selected) return {};
    if (!defend) {
      const auto it = by_actor_.find(selected);
      if (it == by_actor_.end()) return Observation{};
      if (freshness_ms && (now_ms - it->second.timestamp_ms > freshness_ms)) return Observation{};
      return it->second;
    }

    Observation newest{};
    DWORD newest_age = 0;
    bool have_newest = false;
    for (const auto &[actor, obs] : by_actor_) {
      if (obs.target_id != selected) continue;
      const DWORD age = now_ms - obs.timestamp_ms;
      if (age > freshness_ms || (have_newest && age >= newest_age)) continue;
      newest = Observation{obs.timestamp_ms, actor, obs.evidence};
      newest_age = age;
      have_newest = true;
    }
    return newest;
  }

 private:
  // Store a history of timestamped actor(assistee)->target pairs. The key is the assistee / actor spawnId.
  std::unordered_map<WORD, Observation> by_actor_;
};

// Provides a "Target of Target" or "Hit by" UI element that is updated either by internally
// generated refresh polling or by passively observing manual assist responses or damage
// and animation source/victim packets.
class AssistTarget {
 public:
  explicit AssistTarget(class ZealService *zeal);
  ~AssistTarget();

  // Disable copy.
  AssistTarget(AssistTarget const &) = delete;
  AssistTarget &operator=(AssistTarget const &) = delete;

  void TriggerAssistRequest() { trigger_refresh_ = true; }

  ZealSetting<bool> setting_enabled = {false, "AssistBar", "Enabled"};
  ZealSetting<int> setting_position_left = {5, "AssistBar", "Left"};
  ZealSetting<int> setting_position_top = {30, "AssistBar", "Top"};
  // Font size for the bar text (arial_NN sprite fonts); see IsAvailableFontSize().
  ZealSetting<int> setting_font_size = {16, "AssistBar", "FontSize"};
  // Mode: 0 = assist (who my target last hit), 1 = defend (who last hit my target).
  ZealSetting<int> setting_mode = {0, "AssistBar", "Mode"};
  // Freshness window in ms for observations to count as the current candidate.
  ZealSetting<int> setting_window_ms = {10000, "AssistBar", "WindowMs"};
  // Hybrid fallback is session-only; the legacy refresh state is kept off in hybrid mode.
  ZealSetting<bool> setting_auto_refresh = {false, "AssistBar", "AutoRefresh"};
  // Quiet-time / retry interval in ms (clamped to [1000, 60000]).
  ZealSetting<int> setting_refresh_interval_ms = {10000, "AssistBar", "RefreshIntervalMs"};

 private:
  void Clean();  // Resets all assist state and releases resources..
  void ParseArgs(const std::vector<std::string> &args);
  void LoadBitmapFont();                                                 // Loads the bitmap font for rendering.
  void CallbackRender();                                                 // Displays the assist bar.
  bool HandleOutgoingPacket(UINT opcode, const char *buffer, UINT len);  // Monitors outgoing client->server packets.
  bool HandleIncomingPacket(UINT opcode, const char *buffer, UINT len);  // Monitors incoming server->client packets.
  bool HandleAssistPacket(const Zeal::Packets::EntityId_Struct &packet);
  void HandleDamagePacket(const Zeal::Packets::Damage_Struct &dmg);
  void HandleAnimationPacket(const Zeal::Packets::Animation_Struct &animation);
  void HandleTargetHoTTPacket(const Zeal::Packets::TargetHoTT_Struct &packet);
  void RecordNewObservation(WORD assistee_id, WORD target_id, PassiveTargetTracker::Evidence evidence);
  void SendAssistRequest(WORD assistee_id, bool internal);
  void ProcessAssistRequests(WORD target_id);
  const Zeal::GameStructures::Entity *UpdateAndGetLatestResult();

  std::unique_ptr<BitmapFont> bitmap_font = nullptr;

  bool internal_send_request_ = false;            // Short-term flag to communiate with hooked outgoing packet handler.
  bool pending_internal_assist_request_ = false;  // Crrent pending request is an internal refresh request.
  bool trigger_refresh_ = false;                  // Manually triggered internal refresh is pending.
  WORD pending_assistee_id_ = 0;                  // SpawnId of most recently sent Assist request to the server.
  WORD internal_refresh_target_id_ = 0;           // SpawnId of the most recently sent internal Assist request.
  WORD deferred_assistee_id_ = 0;         // Deferred SpawnId of a manual assist request that collided with pending.
  DWORD pending_assist_timestamp_ = 0;    // Game time (ms) of the last request sent to the server.
  DWORD internal_refresh_timestamp_ = 0;  // Game time (ms) of the last internal refresh request sent to the server.
  DWORD deferred_assist_timestamp_ = 0;   // Game time (ms) of when the manual request was deferred.

  PassiveTargetTracker observations_;  // History of observed assistee->target pairs.
};

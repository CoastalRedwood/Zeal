#pragma once
#include <Windows.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include "json.hpp"
#include "zeal_settings.h"

// OtlpExporter emits telemetry from Zeal directly over OTLP/HTTP using the JSON encoding, so the
// game client can talk to an OpenTelemetry backend (e.g. Ourios) or Collector without an external
// pipe-reading sidecar.
//
// This first iteration implements the logs signal only: game log/chat lines are queued on the game
// thread (non-blocking) and flushed by a background worker thread that POSTs an OTLP/HTTP JSON
// payload to <endpoint>/v1/logs. Metrics and traces will follow the same queue+worker pattern.
class OtlpExporter {
 public:
  OtlpExporter(class ZealService *zeal);
  ~OtlpExporter();

  bool is_enabled() const { return setting_enabled.get(); }

  // Queues a single log record. Thread-safe and non-blocking; returns immediately (and drops the
  // record) when disabled so callers can gate cheaply on is_enabled(). color_index is the EQ chat
  // color, recorded as a log attribute.
  void log(const std::string &body, int color_index);

 private:
  struct LogRecord {
    unsigned long long time_unix_nano = 0;
    std::string body;
    int color_index = 0;
    int zone_id = -1;
  };

  // Cached game-state read on the game thread and consumed by the sender thread, so the sender never
  // touches live game memory (which the game thread may be mutating/freeing during zone/camp/logout).
  struct Snapshot {
    bool in_game = false;
    std::string name;
    std::string zone;  // full zone name, e.g. "Mons Letalis"
    const char *class_name = "Unknown";
    int level = 0, deity = 0, aa_unspent = 0;
    int str = 0, sta = 0, dex = 0, agi = 0, wis = 0, intel = 0, cha = 0;
    bool have_attack = false;
    long long attack = 0;
    bool have_haste = false;
    long long haste = 0;
  };

  static constexpr int kMinFlushMs = 100;  // Floor on the flush interval; metrics are periodic snapshots.

  // Runs on the game thread (MainLoop callback); refreshes `snapshot`.
  void sample_game_state();

  void worker_loop();
  bool post_json(const std::string &path, const std::string &json_body);
  std::string build_logs_payload(const std::vector<LogRecord> &records) const;
  nlohmann::json build_resource_attributes() const;

  // Accumulates the `eq.combat.damage` counter, keyed by
  // {source, source_type, direction, damage_type, zone, target}. `target` is the raw spawn name
  // (instance digits included, e.g. "a_temple_guard00") so individual mobs are distinguishable.
  void record_combat_damage(const std::string &source, const std::string &source_type, const std::string &direction,
                            const std::string &type, const std::string &target, long long amount);
  // Parses caster-side DoT ticks and heal messages from a chat line (they have no hit event).
  void parse_dot_or_heal(const std::string &line);
  // Accumulates the `eq.combat.heal` counter, keyed by {source, direction, zone}.
  void record_heal(const std::string &source, const std::string &direction, long long amount);
  // True if `source` should be recorded given the current scope setting (self+pet vs all attackers).
  bool in_combat_scope(const std::string &source) const;
  // Serializes the current cumulative counter snapshot as an OTLP metrics payload ("" if empty).
  std::string build_metrics_payload();

  // --- Fight/zone tracing (spans) -------------------------------------------------------------
  struct FightSpan {  // a completed span, queued for export
    std::string trace_id, span_id, parent_span_id, name;
    unsigned long long start_ns = 0, end_ns = 0;
    std::vector<std::pair<std::string, std::string>> str_attrs;
    std::vector<std::pair<std::string, long long>> int_attrs;
  };
  struct ActiveFight {
    std::string span_id;
    std::string display;  // normalized display name, for matching "slain" chat lines
    unsigned long long start_ns = 0, last_ns = 0;
    long long dmg_out = 0, dmg_in = 0;
  };

  // Called from the hit event (game thread); opens the fight span on first damage.
  void note_fight_damage(const std::string &raw_target, bool outgoing, long long dmg);
  // Game-thread tick: zone-session transitions and idle-fight sweep.
  void fight_tick();
  // Ends one active fight into completed_spans. Caller holds no lock (game thread only state).
  void end_fight(const std::string &key, const char *outcome, unsigned long long end_ns);
  // Chat hook: "X has been slain..." / "You have slain X!" closes the matching fight.
  void handle_slain_line(const std::string &line);
  // Drains completed spans into an OTLP traces payload ("" if none).
  std::string build_traces_payload();

  ZealSetting<bool> setting_enabled = {false, "Zeal", "OtlpEnabled", false};
  ZealSetting<std::string> setting_endpoint = {"http://127.0.0.1:4318", "Zeal", "OtlpEndpoint", false};
  ZealSetting<int> setting_flush_ms = {2000, "Zeal", "OtlpFlushMs", false};
  ZealSetting<int> setting_max_batch = {512, "Zeal", "OtlpMaxBatch", false};

  std::deque<LogRecord> queue;
  mutable std::mutex queue_mutex;
  std::condition_variable queue_cv;
  std::thread worker;
  bool end_thread = false;

  // Fixed start time for cumulative metric streams (set at construction).
  unsigned long long start_time_unix_nano = 0;
  static constexpr unsigned long long kSeriesIdleMs = 10 * 60 * 1000;  // stop exporting fights idle >10min

  struct CombatTotal {
    long long total = 0;
    unsigned long long last_ms = 0;  // GetTickCount64 of last hit; prunes stale per-target series
  };

  std::mutex metrics_mutex;
  // {source, source_type, direction, type, zone, target} -> running total.
  std::map<std::tuple<std::string, std::string, std::string, std::string, std::string, std::string>, CombatTotal>
      combat_damage;
  // {source, direction(outgoing=done, incoming=received), zone} -> total hitpoints.
  std::map<std::tuple<std::string, std::string, std::string>, long long> combat_heal;

  // "self" = record only the local character + its pet (authoritative per-player, no double counting
  // when the whole guild reports). "all" = record every attacker seen in the log (solo experiment).
  ZealSetting<std::string> setting_combat_scope = {"self", "Zeal", "OtlpCombatScope", false};

  // Lightweight send stats surfaced by `/otlp status`.
  std::atomic<unsigned long long> logs_posted{0};
  std::atomic<unsigned long long> metrics_posted{0};
  std::atomic<unsigned long long> failed_posts{0};
  std::atomic<int> last_http_status{0};
  std::mutex last_error_mutex;
  std::string last_error;  // response body / failure reason of the most recent failed post

  mutable std::mutex snapshot_mutex;
  Snapshot snapshot;

  // Fight tracing state. active_* is touched ONLY on the game thread; completed_spans is the
  // handoff queue to the sender thread (guarded by spans_mutex).
  static constexpr unsigned long long kFightIdleNs = 30ULL * 1000000000ULL;  // fight ends after 30s without damage
  std::string zone_trace_id, zone_span_id, zone_active;
  unsigned long long zone_start_ns = 0;
  std::map<std::string, ActiveFight> active_fights;  // key: raw target name
  std::mutex spans_mutex;
  std::vector<FightSpan> completed_spans;
};

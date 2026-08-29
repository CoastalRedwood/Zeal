#pragma once
#include <Windows.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <memory>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include "json.hpp"
#include "otlp_client.h"
#include "zeal_settings.h"

// OtlpExporter is the instrumentation half of the exporter: it hooks the game, decides what a hit,
// heal, or fight means, and encodes those into OTLP payloads. Everything that touches the network -
// the worker thread, the flush timer, delivery and send statistics - lives in OtlpClient, which
// knows nothing about EverQuest.
//
// All three signals are emitted: logs (chat lines), metrics (combat, character, group) and traces
// (zone sessions parenting fight spans). Game-thread callbacks only ever append to state; the
// worker thread reads it through collect().
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
    // The group's leader names the group; empty when ungrouped. Members includes the local
    // character, and names people who are not running Zeal at all - which is the point: without it
    // a group only ever appears as the subset of it that reports telemetry.
    std::string group_leader;
    std::vector<std::string> group_members;
  };

  // Runs on the game thread (MainLoop callback); refreshes `snapshot`.
  void sample_game_state();

  // Called on the worker thread each flush: drains the log queue and snapshots the metric/span
  // state into ready-to-send payloads.
  std::vector<OtlpClient::Payload> collect();
  std::string build_logs_payload(const std::vector<LogRecord> &records) const;
  nlohmann::json build_resource_attributes() const;

  // Accumulates the `eq.combat.damage` counter, keyed by
  // {source, source_type, direction, damage_type, zone, target}. `target` is the raw spawn name
  // (instance digits included, e.g. "a_temple_guard00") so individual mobs are distinguishable.
  void record_combat_damage(const std::string &source, const std::string &source_type, const std::string &direction,
                            const std::string &type, const std::string &target, const std::string &pet,
                            long long amount);
  // Parses caster-side DoT ticks and heal messages from a chat line (they have no hit event).
  void parse_dot_or_heal(const std::string &line);
  // Parses a raid lockout notice ("... lockout for <target> that expires in <n> Hours.").
  void parse_lockout(const std::string &line);
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
    // Per attacker: when they last landed a hit, and how long they have been engaged. This is the
    // community parser's "Sec" column - the denominator that turns damage into DPS rather than
    // SDPS, and the reason those two numbers differ for a caster who nukes twice in a long fight.
    std::map<std::string, std::pair<unsigned long long, double>> attacker_activity;
  };

  // Called from the hit event (game thread); opens the fight span on first damage.
  void note_fight_damage(const std::string &raw_target, const std::string &attacker, bool outgoing,
                         long long dmg);
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
  // Drives both transports. 5s is a deliberate middle: the dashboard refreshes at 10s, and every
  // export re-sends every series, so faster mostly buys upstream bandwidth nobody sees.
  ZealSetting<int> setting_flush_ms = {5000, "Zeal", "OtlpFlushMs", false};
  ZealSetting<int> setting_max_batch = {512, "Zeal", "OtlpMaxBatch", false};
  // The ingest token, sealed with DPAPI so what lands in zeal.ini is inert on
  // any other machine or Windows account. That matters because the ini is the
  // file people paste when asking for help with their UI.
  ZealSetting<std::string> setting_token_sealed = {"", "Zeal", "OtlpTokenSealed", false};

  // Unsealed once, lazily, and kept in memory for the transport.
  mutable std::string token_plain;
  mutable bool token_loaded = false;

  // DPAPI, both directions. Either failing yields an empty string rather than
  // an error: an ini copied from a friend degrades to unauthenticated uploads
  // instead of breaking the client.
  static std::string seal_token(const std::string &plain);
  static std::string unseal_token(const std::string &sealed);
  const std::string &auth_token() const;
  // Drops the SDK providers so the next sampler tick rebuilds them with the current endpoint and
  // token. Both are captured at construction, so nothing else makes a change take effect.
  void restart_pipeline();

  std::deque<LogRecord> queue;
  mutable std::mutex queue_mutex;

  // Transport: owns the worker thread, the flush timer and delivery. Constructed last so the worker
  // never observes a half-built exporter, and destroyed first so collect() cannot run during
  // teardown.
  std::unique_ptr<OtlpClient> client;

  // Fixed start time for cumulative metric streams (set at construction).
  unsigned long long start_time_unix_nano = 0;
  // Longer than a slow two-hander, short enough that a pause reads as downtime.
  static constexpr unsigned long long kAttackerIdleNs = 12ULL * 1000000000ULL;
  static constexpr unsigned long long kSeriesIdleMs = 10 * 60 * 1000;  // stop exporting fights idle >10min

  struct CombatTotal {
    long long total = 0;
    unsigned long long last_ms = 0;  // GetTickCount64 of last hit; prunes stale per-target series
  };

  std::mutex metrics_mutex;
  // {source, source_type, direction, type, zone, target, group_leader} -> running total.
  // The group is part of the key rather than stamped on at export time so damage stays attributed to
  // the group it was dealt in, even if the player regroups before the next flush.
  std::map<std::tuple<std::string, std::string, std::string, std::string, std::string, std::string, std::string>,
           CombatTotal>
      combat_damage;
  // {source, direction(outgoing=done, incoming=received), zone, group_leader} -> total hitpoints.
  std::map<std::tuple<std::string, std::string, std::string, std::string>, long long> combat_heal;

  // "self" = record only the local character + its pet (authoritative per-player, no double counting
  // when the whole guild reports). "all" = record every attacker seen in the log (solo experiment).
  ZealSetting<std::string> setting_combat_scope = {"self", "Zeal", "OtlpCombatScope", false};

  // `/otlp debug`: echo the raw fields of every recorded hit to chat, so a classification can be
  // checked against what the server actually sent rather than against what we believe it sends.
  // Deliberately not persisted - it is spammy, and should not survive a session by accident.
  std::atomic<bool> debug_hits{false};

  // Raid lockouts: target -> unix seconds at which the lockout expires. The expiry is stored as an
  // absolute instant rather than a remaining duration, so a dashboard can keep counting down from
  // the last reported value even after the player logs off and the series goes stale.
  std::map<std::string, long long> lockouts;
  // target -> unix seconds the target died. The server sends the lockout notice from
  // NPC::CreateCorpse, so the moment it arrives is the time of death - which is the number guilds
  // currently ask people to type into Discord by hand.
  std::map<std::string, long long> raid_kills;

  // Damage shield pairing state (game thread only, via the chat callback): the amount from the most
  // recent "was hit by non-melee" message, claimed by a shield flavour line naming the same target.
  std::string ds_pending_target;
  long long ds_pending_amount = 0;
  unsigned long long ds_pending_ms = 0;

  // Log lines delivered, counted here because the client only counts payloads, not records.
  std::atomic<unsigned long long> logs_posted{0};

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

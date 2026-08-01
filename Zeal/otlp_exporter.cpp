#include "otlp_exporter.h"

#include "everquest_semconv.h"  // generated from the semconv registry (see everquest-semconv/generate.sh)

#include <winhttp.h>

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <random>

#include "callbacks.h"
#include "chat.h"
#include "commands.h"
#include "entity_manager.h"
#include "game_functions.h"
#include "game_structures.h"
#include "json.hpp"
#include "labels.h"
#include "string_util.h"
#include "zeal.h"

#pragma comment(lib, "winhttp.lib")

namespace {
// OTLP/HTTP JSON encodes 64-bit integer fields (timeUnixNano, AnyValue.intValue) as strings.
nlohmann::json string_attr(const char *key, const std::string &value) {
  return {{"key", key}, {"value", {{"stringValue", value}}}};
}
nlohmann::json int_attr(const char *key, long long value) {
  return {{"key", key}, {"value", {{"intValue", std::to_string(value)}}}};
}

unsigned long long now_unix_nano() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch())
      .count();
}

// Random hex id for traces/spans (game thread only; single static generator).
std::string hex_id(int bytes) {
  static std::mt19937_64 rng(std::random_device{}() ^ GetTickCount64());
  static const char *digits = "0123456789abcdef";
  std::string out;
  out.reserve(bytes * 2);
  for (int i = 0; i < bytes; i += 8) {
    unsigned long long v = rng();
    for (int b = 0; b < 8 && (i + b) < bytes; b++) {
      out.push_back(digits[(v >> (b * 8 + 4)) & 0xF]);
      out.push_back(digits[(v >> (b * 8)) & 0xF]);
    }
  }
  return out;
}

// "a_temple_guard00" -> "a temple guard" (lowercase, underscores to spaces, trailing digits dropped)
std::string display_of(const std::string &raw) {
  std::string d = raw;
  while (!d.empty() && isdigit(static_cast<unsigned char>(d.back()))) d.pop_back();
  for (auto &c : d) c = (c == '_') ? ' ' : static_cast<char>(tolower(static_cast<unsigned char>(c)));
  return d;
}

// Full zone name of the local player ("" when not resolvable). Game thread only.
std::string current_zone_name() {
  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  if (!Zeal::Game::is_in_game() || !self) return "";
  return Zeal::Game::get_full_zone_name(self->ZoneId);
}

const char *class_name(int class_id) {
  switch (class_id) {
    case 1: return "Warrior";
    case 2: return "Cleric";
    case 3: return "Paladin";
    case 4: return "Ranger";
    case 5: return "Shadowknight";
    case 6: return "Druid";
    case 7: return "Monk";
    case 8: return "Bard";
    case 9: return "Rogue";
    case 10: return "Shaman";
    case 11: return "Necromancer";
    case 12: return "Wizard";
    case 13: return "Magician";
    case 14: return "Enchanter";
    case 15: return "Beastlord";
    default: return "Unknown";
  }
}

// Parses an EverQuest combat log line for damage the player is directly involved in and, if found,
// fills direction ("outgoing"/"incoming"), type (the melee verb or "spell"), and amount. Returns
// false for lines that aren't self-involved damage (e.g. a group member's hits) so they aren't
// double counted. EQ renders the local player as "You"/"YOU".
// Maps an EQ melee SkillType (Damage_Struct.type) to a canonical damage-type label.
const char *melee_type_name(unsigned short skill) {
  using S = Zeal::GameEnums::SkillType;
  switch (skill) {
    case S::Skill1HSlashing:
    case S::Skill2HSlashing:
      return "slash";
    case S::Skill1HBlunt:
    case S::Skill2HBlunt:
      return "crush";
    case S::Skill1HPiercing:
      return "pierce";
    case S::SkillBash:
      return "bash";
    case S::SkillKick:
    case S::SkillFlyingKick:
    case S::SkillRoundKick:
      return "kick";
    case S::SkillHandtoHand:
      return "hth";
    case S::SkillBackstab:
      return "backstab";
    case S::SkillArchery:
      return "archery";
    case S::SkillDragonPunch:
    case S::SkillEagleStrike:
      return "special";
    default:
      return "melee";
  }
}

}  // namespace

OtlpExporter::OtlpExporter(ZealService *zeal) {
  start_time_unix_nano = now_unix_nano();
  worker = std::thread([this]() { worker_loop(); });

  // Emit every print-to-chat line as a log record and mine it for combat damage. This is the live
  // source (the pipe uses the same callback); registering our own keeps OTLP independent of whether
  // the pipe has a reader connected.
  zeal->chat_hook->add_print_chat_callback([this](const char *data, int color_index) {
    if (!is_enabled() || !data) return;
    log(data, color_index);
    // DoT ticks and heal amounts never fire the hit event and are only messaged to the involved
    // client, so text is the correct (and self-reporting, non-duplicating) channel for them.
    parse_dot_or_heal(data);
    handle_slain_line(data);
  });

  // (see parse_dot_or_heal below for DoT/heal text handling)
  // Combat damage from the actual hit event (spawn IDs resolved to entities) — far more reliable than
  // parsing chat text: exact attacker/target, damage, and skill/spell, incl. charmed pets.
  zeal->callbacks->AddReportSuccessfulHit([this](Zeal::GameStructures::Entity *source,
                                                 Zeal::GameStructures::Entity *target, WORD type, short spell_id,
                                                 short damage, char) {
    if (!is_enabled() || !source || damage <= 0) return;
    Zeal::GameStructures::Entity *attacker = source;
    if (attacker->PetOwnerSpawnId != 0) {  // credit a pet's damage to its owner (charmed or summoned)
      auto *owner = ZealService::get_instance()->entity_manager->Get(attacker->PetOwnerSpawnId);
      if (owner) attacker = owner;
    }
    std::string src = attacker->Name;
    const char *src_type = (attacker->Type == Zeal::GameEnums::EntityTypes::Player) ? "player" : "npc";
    const char *direction = (target && target == Zeal::Game::get_self()) ? "incoming" : "outgoing";
    const char *dtype = (spell_id > 0) ? "spell" : melee_type_name(type);
    const std::string tgt = target ? target->Name : "";  // raw spawn name, e.g. "a_temple_guard00"
    if (in_combat_scope(src)) record_combat_damage(src, src_type, direction, dtype, tgt, damage);
    // Fight spans: track encounters with NPCs (the mob is the target when we hit it, the source
    // when it hits us).
    const bool outgoing = (direction[0] == 'o');
    const std::string mob = outgoing ? tgt : std::string(source->Name);
    if (!mob.empty()) note_fight_damage(mob, outgoing, damage);
  });

  // Sample live game state on the game thread (MainLoop); the sender thread only ever serializes the
  // cached snapshot, so it never races the game thread reading/freeing character structures.
  zeal->callbacks->AddGeneric([this]() {
    sample_game_state();
    if (is_enabled()) fight_tick();
  });

  zeal->commands_hook->Add("/otlp", {}, "OTLP/HTTP telemetry export. Usage: /otlp on|off|status|endpoint <url>",
                           [this](std::vector<std::string> &args) {
                             if (args.size() == 2 && Zeal::String::compare_insensitive(args[1], "on")) {
                               setting_enabled.set(true);
                               Zeal::Game::print_chat("OTLP export enabled -> %s/v1/logs",
                                                      setting_endpoint.get().c_str());
                               return true;
                             }
                             if (args.size() == 2 && Zeal::String::compare_insensitive(args[1], "off")) {
                               setting_enabled.set(false);
                               Zeal::Game::print_chat("OTLP export disabled.");
                               return true;
                             }
                             if (args.size() == 3 && Zeal::String::compare_insensitive(args[1], "endpoint")) {
                               setting_endpoint.set(args[2]);
                               Zeal::Game::print_chat("OTLP endpoint set to %s", args[2].c_str());
                               return true;
                             }
                             if (args.size() == 3 && Zeal::String::compare_insensitive(args[1], "flush")) {
                               int ms = 0;
                               if (!Zeal::String::tryParse(args[2], &ms)) {
                                 Zeal::Game::print_chat("Usage: /otlp flush <milliseconds>");
                                 return true;
                               }
                               if (ms < kMinFlushMs) ms = kMinFlushMs;  // Clamp: metrics are periodic; 0 would just hammer.
                               setting_flush_ms.set(ms);
                               Zeal::Game::print_chat("OTLP flush interval set to %ims", ms);
                               return true;
                             }
                             if (args.size() == 3 && Zeal::String::compare_insensitive(args[1], "scope")) {
                               if (Zeal::String::compare_insensitive(args[2], "self") ||
                                   Zeal::String::compare_insensitive(args[2], "all")) {
                                 setting_combat_scope.set(Zeal::String::compare_insensitive(args[2], "all") ? "all"
                                                                                                           : "self");
                                 Zeal::Game::print_chat("OTLP combat scope: %s", setting_combat_scope.get().c_str());
                               } else {
                                 Zeal::Game::print_chat("Usage: /otlp scope self|all  (self=you+pet, all=everyone)");
                               }
                               return true;
                             }
                             Zeal::Game::print_chat("OTLP: %s, endpoint %s, flush %ims, scope %s",
                                                    setting_enabled.get() ? "enabled" : "disabled",
                                                    setting_endpoint.get().c_str(), setting_flush_ms.get(),
                                                    setting_combat_scope.get().c_str());
                             Zeal::Game::print_chat("  sent: %llu log batches-worth, %llu metric posts, %llu failed",
                                                    logs_posted.load(), metrics_posted.load(), failed_posts.load());
                             Zeal::Game::print_chat("  last HTTP status: %i", last_http_status.load());
                             {
                               std::lock_guard<std::mutex> lock(last_error_mutex);
                               if (!last_error.empty())
                                 Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "  last error: %s",
                                                        last_error.c_str());
                             }
                             Zeal::Game::print_chat("Usage: /otlp on|off|status|endpoint <url>|flush <ms>|scope self|all");
                             return true;
                           });
}

OtlpExporter::~OtlpExporter() {
  {
    std::lock_guard<std::mutex> lock(queue_mutex);
    end_thread = true;
  }
  queue_cv.notify_all();
  if (worker.joinable()) worker.join();
}

void OtlpExporter::log(const std::string &body, int color_index) {
  if (!setting_enabled.get() || body.empty()) return;

  LogRecord record;
  record.time_unix_nano = now_unix_nano();
  record.body = body;
  record.color_index = color_index;
  if (Zeal::Game::is_in_game() && Zeal::Game::get_self()) record.zone_id = Zeal::Game::get_self()->ZoneId;

  {
    std::lock_guard<std::mutex> lock(queue_mutex);
    queue.push_back(std::move(record));
  }
  queue_cv.notify_one();
}

void OtlpExporter::worker_loop() {
  while (true) {
    std::vector<LogRecord> batch;
    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      const int configured = setting_flush_ms.get();
      const int interval = configured > 0 ? (configured < kMinFlushMs ? kMinFlushMs : configured) : 2000;
      queue_cv.wait_for(lock, std::chrono::milliseconds(interval),
                        [this]() { return end_thread || !queue.empty(); });
      if (end_thread && queue.empty()) return;

      const int max_batch = setting_max_batch.get() > 0 ? setting_max_batch.get() : 512;
      while (!queue.empty() && static_cast<int>(batch.size()) < max_batch) {
        batch.push_back(std::move(queue.front()));
        queue.pop_front();
      }
    }

    if (!setting_enabled.get()) continue;

    try {
      if (!batch.empty()) {
        if (post_json("/v1/logs", build_logs_payload(batch)))
          logs_posted += batch.size();
        else
          failed_posts++;
      }
      // Metrics use cumulative temporality, so emit the current snapshot every flush even when no
      // new log lines arrived this cycle.
      std::string metrics = build_metrics_payload();
      if (!metrics.empty()) {
        if (post_json("/v1/metrics", metrics))
          metrics_posted++;
        else
          failed_posts++;
      }
      std::string traces = build_traces_payload();
      if (!traces.empty() && !post_json("/v1/traces", traces)) failed_posts++;
    } catch (const std::exception &) {
      // Never let telemetry take down the game; drop this cycle on failure.
    }
  }
}

void OtlpExporter::sample_game_state() {
  if (!setting_enabled.get()) return;  // No need to read game memory while export is off.

  Snapshot s;
  Zeal::GameStructures::GAMECHARINFO *ci = Zeal::Game::get_char_info();
  if (Zeal::Game::is_in_game() && ci) {
    s.in_game = true;
    s.name = ci->Name;
    s.zone = current_zone_name();
    s.class_name = class_name(ci->Class);
    s.level = ci->Level;
    s.deity = ci->Deity;
    s.aa_unspent = ci->AlternateAdvancementUnspent;
    s.str = ci->BaseSTR;
    s.sta = ci->BaseSTA;
    s.dex = ci->BaseDEX;
    s.agi = ci->BaseAGI;
    s.wis = ci->BaseWIS;
    s.intel = ci->BaseINT;
    s.cha = ci->BaseCHA;

    ZealService *zeal = ZealService::get_instance();
    std::string offense;
    if (zeal->labels_hook && zeal->labels_hook->GetLabel(23, offense)) {  // 23 = CurrentOffense (attack rating).
      s.have_attack = true;
      s.attack = atoll(offense.c_str());
    }
    Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
    if (self) {
      // ModifyAttackSpeed applies total effective haste (worn + spell + overhaste) to a reference
      // delay; derive the haste percentage from the ratio.
      unsigned int modified = self->ModifyAttackSpeed(1000, 0);
      long long haste = (modified > 0) ? static_cast<long long>((1000.0 - modified) * 100.0 / modified + 0.5) : 0;
      s.have_haste = true;
      s.haste = haste < 0 ? 0 : haste;
    }
  }

  std::lock_guard<std::mutex> lock(snapshot_mutex);
  snapshot = std::move(s);
}

// Builds the OTLP resource attributes shared by all signals from the cached snapshot: the service
// identity plus, when in game, the character context (identity + slowly-changing stats). Per OTLP
// guidance this belongs on the Resource — low cardinality, describes the entity producing telemetry,
// and enriches logs and metrics alike without inflating metric attribute cardinality.
nlohmann::json OtlpExporter::build_resource_attributes() const {
  nlohmann::json attrs = nlohmann::json::array();
  attrs.push_back(string_attr("service.name", "everquest"));
  attrs.push_back(string_attr("service.version", ZEAL_VERSION));
  attrs.push_back(string_attr("telemetry.sdk.name", "zeal"));

  Snapshot s;
  {
    std::lock_guard<std::mutex> lock(snapshot_mutex);
    s = snapshot;
  }
  if (s.in_game) {
    // Character name doubles as the service instance id so a shared backend can tell players apart.
    attrs.push_back(string_attr("service.instance.id", s.name));
    attrs.push_back(string_attr(everquest_semconv::kEverquestCharacterName, s.name));
    attrs.push_back(string_attr(everquest_semconv::kEverquestCharacterClass, s.class_name));
    attrs.push_back(int_attr(everquest_semconv::kEverquestCharacterLevel, s.level));
    attrs.push_back(int_attr(everquest_semconv::kEverquestCharacterDeity, s.deity));
    attrs.push_back(int_attr(everquest_semconv::kEverquestCharacterAaUnspent, s.aa_unspent));
    attrs.push_back(int_attr("everquest.character.stat.strength", s.str));
    attrs.push_back(int_attr("everquest.character.stat.stamina", s.sta));
    attrs.push_back(int_attr("everquest.character.stat.dexterity", s.dex));
    attrs.push_back(int_attr("everquest.character.stat.agility", s.agi));
    attrs.push_back(int_attr("everquest.character.stat.wisdom", s.wis));
    attrs.push_back(int_attr("everquest.character.stat.intelligence", s.intel));
    attrs.push_back(int_attr("everquest.character.stat.charisma", s.cha));
  }
  return attrs;
}

std::string OtlpExporter::build_logs_payload(const std::vector<LogRecord> &records) const {
  nlohmann::json log_records = nlohmann::json::array();
  for (const auto &r : records) {
    // Character identity now lives on the Resource; keep only per-line context here.
    nlohmann::json attributes = nlohmann::json::array();
    attributes.push_back(int_attr(everquest_semconv::kEverquestChatColor, r.color_index));
    if (r.zone_id >= 0) attributes.push_back(int_attr(everquest_semconv::kEverquestZoneId, r.zone_id));

    log_records.push_back({{"timeUnixNano", std::to_string(r.time_unix_nano)},
                           {"severityNumber", 9},  // INFO
                           {"severityText", "INFO"},
                           {"body", {{"stringValue", r.body}}},
                           {"attributes", attributes}});
  }

  nlohmann::json payload = {
      {"resourceLogs",
       {{{"resource", {{"attributes", build_resource_attributes()}}},
         {"scopeLogs",
          {{{"scope", {{"name", "zeal"}, {"version", ZEAL_VERSION}}}, {"logRecords", log_records}}}}}}}};
  // EQ log lines can contain stray non-printable bytes; replace invalid UTF-8 rather than letting
  // dump() throw and drop the whole batch.
  return payload.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}

bool OtlpExporter::in_combat_scope(const std::string &source) const {
  if (setting_combat_scope.get() != "self") return true;  // "all" (or anything non-"self") records everyone.
  // Self scope: only the local character and its pet (runs on the game thread via the chat callback).
  Zeal::GameStructures::GAMECHARINFO *ci = Zeal::Game::get_char_info();
  if (ci && source == ci->Name) return true;
  std::string pet;
  if (ZealService::get_instance()->labels_hook &&
      ZealService::get_instance()->labels_hook->GetLabel(68, pet) && !pet.empty() && source == pet)
    return true;  // 68 = PlayerPetName
  return false;
}

void OtlpExporter::record_combat_damage(const std::string &source, const std::string &source_type,
                                        const std::string &direction, const std::string &type,
                                        const std::string &target, long long amount) {
  const std::string zone = current_zone_name();  // where the hit happened (game thread)
  std::lock_guard<std::mutex> lock(metrics_mutex);
  CombatTotal &entry = combat_damage[{source, source_type, direction, type, zone, target}];
  entry.total += amount;
  entry.last_ms = GetTickCount64();
}

void OtlpExporter::record_heal(const std::string &source, const std::string &direction, long long amount) {
  const std::string zone = current_zone_name();
  std::lock_guard<std::mutex> lock(metrics_mutex);
  combat_heal[{source, direction, zone}] += amount;
}

// Extracts the positive integer that starts at line[pos] (returns 0 if none).
static long long read_number(const std::string &line, size_t pos) {
  size_t end = pos;
  while (end < line.size() && isdigit(static_cast<unsigned char>(line[end]))) end++;
  return (end > pos) ? atoll(line.substr(pos, end - pos).c_str()) : 0;
}

void OtlpExporter::parse_dot_or_heal(const std::string &line) {
  const char *self = Zeal::Game::get_self() ? Zeal::Game::get_self()->Name : nullptr;
  if (!self) return;

  // DoT tick (caster-side): "<target> has taken <N> damage from your <spell>."
  // Observer form:          "<target> has taken <N> damage from <caster>'s <spell>."
  size_t p = line.find(" has taken ");
  if (p != std::string::npos) {
    long long amount = read_number(line, p + 11);
    size_t from = line.find(" damage from ", p);
    if (amount > 0 && from != std::string::npos) {
      const std::string dot_target = line.substr(0, p);  // display name; chat has no instance digits
      std::string rest = line.substr(from + 13);
      if (rest.rfind("your ", 0) == 0) {
        record_combat_damage(self, "player", "outgoing", "dot", dot_target, amount);
      } else {
        size_t apos = rest.find("'s ");  // "<caster>'s <spell>"
        if (apos != std::string::npos && setting_combat_scope.get() != "self") {
          std::string caster = rest.substr(0, apos);
          auto *e = ZealService::get_instance()->entity_manager->Get(caster);
          const char *ct = (e && e->Type == Zeal::GameEnums::EntityTypes::Player) ? "player" : "npc";
          record_combat_damage(caster, ct, "outgoing", "dot", dot_target, amount);
        }
      }
    }
    return;
  }

  // Heal done (caster-side): "You have healed <target> for <N> points."
  p = line.find("You have healed ");
  if (p == 0) {
    size_t f = line.find(" for ", 16);
    if (f != std::string::npos) {
      long long amount = read_number(line, f + 5);
      if (amount > 0) record_heal(self, "outgoing", amount);
    }
    return;
  }

  // Heal received (target-side): "You have been healed for <N> points."
  p = line.find("You have been healed for ");
  if (p == 0) {
    long long amount = read_number(line, 25);
    if (amount > 0) record_heal(self, "incoming", amount);
  }
}

// Builds a single-value gauge metric as an OTLP metric object (zone attached when known).
static nlohmann::json gauge_metric(const char *name, const char *unit, const std::string &now, long long value,
                                   const std::string &zone) {
  nlohmann::json point = {{"timeUnixNano", now}, {"asInt", std::to_string(value)}};
  // NOTE: must be an explicit array — a braced-init with a single object collapses into an object,
  // which the OTLP receiver rejects ("ReadArray: expect [ ..."), failing the whole payload.
  if (!zone.empty()) point["attributes"] = nlohmann::json::array({string_attr(everquest_semconv::kEverquestZoneName, zone)});
  nlohmann::json metric;
  metric["name"] = name;
  metric["unit"] = unit;
  metric["gauge"] = {{"dataPoints", nlohmann::json::array({point})}};
  return metric;
}

std::string OtlpExporter::build_metrics_payload() {
  const std::string now = std::to_string(now_unix_nano());
  const std::string start = std::to_string(start_time_unix_nano);
  nlohmann::json metrics = nlohmann::json::array();

  // Combat damage counter (cumulative monotonic Sum).
  nlohmann::json data_points = nlohmann::json::array();
  {
    const unsigned long long now_ms = GetTickCount64();
    std::lock_guard<std::mutex> lock(metrics_mutex);
    for (auto it = combat_damage.begin(); it != combat_damage.end();) {
      // Per-target series are unbounded over a session; stop exporting (and free) fights that have
      // been idle. Prometheus keeps the history; if the same mob reappears rate() sees a reset.
      if (now_ms - it->second.last_ms > kSeriesIdleMs) {
        it = combat_damage.erase(it);
        continue;
      }
      const auto &key = it->first;
      data_points.push_back({{"attributes",
                              {string_attr(everquest_semconv::kEverquestCombatSource, std::get<0>(key)),
                               string_attr(everquest_semconv::kEverquestCombatSourceType, std::get<1>(key)),
                               string_attr(everquest_semconv::kEverquestCombatDirection, std::get<2>(key)),
                               string_attr(everquest_semconv::kEverquestCombatDamageType, std::get<3>(key)),
                               string_attr(everquest_semconv::kEverquestZoneName, std::get<4>(key)),
                               string_attr(everquest_semconv::kEverquestCombatTarget, std::get<5>(key))}},
                             {"startTimeUnixNano", start},
                             {"timeUnixNano", now},
                             {"asInt", std::to_string(it->second.total)}});
      ++it;
    }
  }
  if (!data_points.empty()) {
    nlohmann::json metric;
    metric["name"] = everquest_semconv::kEverquestCombatDamageMetric;
    metric["unit"] = "{hitpoint}";
    metric["sum"] = {{"dataPoints", data_points}, {"aggregationTemporality", 2}, {"isMonotonic", true}};
    metrics.push_back(metric);
  }

  // Healing counter (cumulative monotonic Sum), keyed by {source, direction}.
  nlohmann::json heal_points = nlohmann::json::array();
  {
    std::lock_guard<std::mutex> lock(metrics_mutex);
    for (const auto &[key, total] : combat_heal) {
      heal_points.push_back({{"attributes",
                              {string_attr(everquest_semconv::kEverquestCombatSource, std::get<0>(key)),
                               string_attr(everquest_semconv::kEverquestCombatDirection, std::get<1>(key)),
                               string_attr(everquest_semconv::kEverquestZoneName, std::get<2>(key))}},
                             {"startTimeUnixNano", start},
                             {"timeUnixNano", now},
                             {"asInt", std::to_string(total)}});
    }
  }
  if (!heal_points.empty()) {
    nlohmann::json metric;
    metric["name"] = everquest_semconv::kEverquestCombatHealMetric;
    metric["unit"] = "{hitpoint}";
    metric["sum"] = {{"dataPoints", heal_points}, {"aggregationTemporality", 2}, {"isMonotonic", true}};
    metrics.push_back(metric);
  }

  // Attack (offense) and haste gauges for DPS correlation, read from the game-thread snapshot.
  {
    Snapshot s;
    {
      std::lock_guard<std::mutex> lock(snapshot_mutex);
      s = snapshot;
    }
    if (s.have_attack) metrics.push_back(gauge_metric(everquest_semconv::kEverquestCharacterAttackMetric, "1", now, s.attack, s.zone));
    if (s.have_haste) metrics.push_back(gauge_metric(everquest_semconv::kEverquestCharacterHasteMetric, "%", now, s.haste, s.zone));
  }

  if (metrics.empty()) return "";

  nlohmann::json scope_metrics;
  scope_metrics["scope"] = {{"name", "zeal"}, {"version", ZEAL_VERSION}};
  scope_metrics["metrics"] = metrics;

  nlohmann::json resource_metrics;
  resource_metrics["resource"] = {{"attributes", build_resource_attributes()}};
  resource_metrics["scopeMetrics"] = nlohmann::json::array({scope_metrics});

  nlohmann::json payload;
  payload["resourceMetrics"] = nlohmann::json::array({resource_metrics});
  return payload.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}


void OtlpExporter::note_fight_damage(const std::string &raw_target, bool outgoing, long long dmg) {
  const unsigned long long now = now_unix_nano();
  ActiveFight &f = active_fights[raw_target];
  if (f.start_ns == 0) {
    f.span_id = hex_id(8);
    f.display = display_of(raw_target);
    f.start_ns = now;
  }
  f.last_ns = now;
  (outgoing ? f.dmg_out : f.dmg_in) += dmg;
}

void OtlpExporter::end_fight(const std::string &key, const char *outcome, unsigned long long end_ns) {
  auto it = active_fights.find(key);
  if (it == active_fights.end()) return;
  const ActiveFight &f = it->second;
  FightSpan sp;
  sp.trace_id = zone_trace_id.empty() ? hex_id(16) : zone_trace_id;
  sp.span_id = f.span_id;
  sp.parent_span_id = zone_span_id;  // child of the zone-session span ("" when none)
  sp.name = "fight " + f.display;
  sp.start_ns = f.start_ns;
  sp.end_ns = end_ns ? end_ns : f.last_ns;
  sp.str_attrs = {{everquest_semconv::kEverquestCombatTarget, key}, {everquest_semconv::kEverquestZoneName, zone_active}, {"everquest.fight.outcome", outcome}};
  sp.int_attrs = {{"everquest.fight.damage.dealt", f.dmg_out}, {"everquest.fight.damage.taken", f.dmg_in}};
  {
    std::lock_guard<std::mutex> lock(spans_mutex);
    completed_spans.push_back(std::move(sp));
  }
  active_fights.erase(it);
}

void OtlpExporter::fight_tick() {
  const unsigned long long now = now_unix_nano();
  const std::string zone = current_zone_name();

  if (zone != zone_active) {  // zone transition (incl. entering/leaving the world)
    for (auto it = active_fights.begin(); it != active_fights.end();) {
      const std::string key = it->first;
      ++it;
      end_fight(key, "zoned", 0);
    }
    if (!zone_active.empty() && zone_start_ns) {  // close the previous zone-session span
      FightSpan sp;
      sp.trace_id = zone_trace_id;
      sp.span_id = zone_span_id;
      sp.name = "zone " + zone_active;
      sp.start_ns = zone_start_ns;
      sp.end_ns = now;
      sp.str_attrs = {{everquest_semconv::kEverquestZoneName, zone_active}};
      std::lock_guard<std::mutex> lock(spans_mutex);
      completed_spans.push_back(std::move(sp));
    }
    zone_active = zone;
    if (!zone.empty()) {
      zone_trace_id = hex_id(16);
      zone_span_id = hex_id(8);
      zone_start_ns = now;
    } else {
      zone_trace_id.clear();
      zone_span_id.clear();
      zone_start_ns = 0;
    }
    return;
  }

  // Idle sweep: a fight with no damage for kFightIdleNs is over (mob fled/reset/we moved on).
  for (auto it = active_fights.begin(); it != active_fights.end();) {
    const std::string key = it->first;
    const bool idle = (now - it->second.last_ns) > kFightIdleNs;
    ++it;
    if (idle) end_fight(key, "idle", 0);
  }
}

void OtlpExporter::handle_slain_line(const std::string &line) {
  if (active_fights.empty()) return;
  std::string dead;
  size_t p = line.find(" has been slain");
  if (p != std::string::npos) {
    dead = line.substr(0, p);
  } else if (line.rfind("You have slain ", 0) == 0) {
    size_t bang = line.find('!');
    dead = line.substr(15, (bang == std::string::npos ? line.size() : bang) - 15);
  } else {
    return;
  }
  for (auto &c : dead) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  for (auto &[key, f] : active_fights) {
    if (f.display == dead) {
      end_fight(key, "killed", now_unix_nano());
      return;
    }
  }
}

std::string OtlpExporter::build_traces_payload() {
  std::vector<FightSpan> batch;
  {
    std::lock_guard<std::mutex> lock(spans_mutex);
    if (completed_spans.empty()) return "";
    batch.swap(completed_spans);
  }
  nlohmann::json spans = nlohmann::json::array();
  for (const auto &sp : batch) {
    nlohmann::json attrs = nlohmann::json::array();
    for (const auto &[k, v] : sp.str_attrs) attrs.push_back(string_attr(k.c_str(), v));
    for (const auto &[k, v] : sp.int_attrs) attrs.push_back(int_attr(k.c_str(), v));
    nlohmann::json j;
    j["traceId"] = sp.trace_id;
    j["spanId"] = sp.span_id;
    if (!sp.parent_span_id.empty()) j["parentSpanId"] = sp.parent_span_id;
    j["name"] = sp.name;
    j["kind"] = 1;  // SPAN_KIND_INTERNAL
    j["startTimeUnixNano"] = std::to_string(sp.start_ns);
    j["endTimeUnixNano"] = std::to_string(sp.end_ns);
    j["attributes"] = attrs;
    j["status"] = nlohmann::json::object();
    spans.push_back(j);
  }
  nlohmann::json scope_spans;
  scope_spans["scope"] = {{"name", "zeal"}, {"version", ZEAL_VERSION}};
  scope_spans["spans"] = spans;
  nlohmann::json resource_spans;
  resource_spans["resource"] = {{"attributes", build_resource_attributes()}};
  resource_spans["scopeSpans"] = nlohmann::json::array({scope_spans});
  nlohmann::json payload;
  payload["resourceSpans"] = nlohmann::json::array({resource_spans});
  return payload.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}

bool OtlpExporter::post_json(const std::string &path, const std::string &json_body) {
  std::string url = setting_endpoint.get() + path;
  std::wstring wurl(url.begin(), url.end());

  URL_COMPONENTS uc = {0};
  uc.dwStructSize = sizeof(uc);
  wchar_t host[256] = {0};
  wchar_t url_path[1024] = {0};
  uc.lpszHostName = host;
  uc.dwHostNameLength = _countof(host);
  uc.lpszUrlPath = url_path;
  uc.dwUrlPathLength = _countof(url_path);
  if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc)) return false;

  bool ok = false;
  HINTERNET session = WinHttpOpen(L"Zeal-OTLP/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                                  WINHTTP_NO_PROXY_BYPASS, 0);
  if (session) {
    // Bound every phase so a slow/unreachable endpoint can't hang the sender thread (and thus the
    // join() on shutdown). Values in ms: resolve, connect, send, receive.
    WinHttpSetTimeouts(session, 2000, 2000, 2000, 3000);
    HINTERNET connect = WinHttpConnect(session, host, uc.nPort, 0);
    if (connect) {
      DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
      HINTERNET request = WinHttpOpenRequest(connect, L"POST", url_path, nullptr, WINHTTP_NO_REFERER,
                                             WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
      if (request) {
        const wchar_t *headers = L"Content-Type: application/json";
        if (WinHttpSendRequest(request, headers, -1L, (LPVOID)json_body.data(),
                               static_cast<DWORD>(json_body.size()), static_cast<DWORD>(json_body.size()), 0) &&
            WinHttpReceiveResponse(request, nullptr)) {
          DWORD status = 0, size = sizeof(status);
          WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                              &status, &size, WINHTTP_NO_HEADER_INDEX);
          last_http_status.store(static_cast<int>(status));
          ok = (status >= 200 && status < 300);
          if (!ok) {
            // Keep the server's explanation — it names the exact problem (e.g. a malformed field),
            // which is otherwise invisible from in-game.
            std::string body;
            char buf[512];
            DWORD read = 0;
            while (body.size() < 400 && WinHttpReadData(request, buf, sizeof(buf), &read) && read > 0)
              body.append(buf, read);
            std::lock_guard<std::mutex> lock(last_error_mutex);
            last_error = path + " -> " + std::to_string(status) + " " + body.substr(0, 300);
          }
        }
        WinHttpCloseHandle(request);
      }
      WinHttpCloseHandle(connect);
    }
    WinHttpCloseHandle(session);
  }
  return ok;
}

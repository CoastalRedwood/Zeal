#include "otlp_exporter.h"

#include "everquest_semconv.h"  // generated from the semconv registry (see everquest-semconv/generate.sh)

#ifdef ZEAL_OTEL_SDK
#include "instrumentation.h"  // API-only gameplay instrumentation
#include "telemetry.h"       // SDK setup, this file is the application owner
#endif

#include <winhttp.h>
#include <wincrypt.h>  // DPAPI: the ingest token is sealed to this Windows account

#pragma comment(lib, "crypt32.lib")

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include "callbacks.h"
#include "chat.h"
#include "commands.h"
#include "entity_manager.h"
#include "game_addresses.h"  // Zeal::Game::GroupInfo
#include "game_functions.h"
#include "game_structures.h"
#include "json.hpp"
#include "labels.h"
#include "string_util.h"
#include "zeal.h"

#pragma comment(lib, "winhttp.lib")

#ifdef ZEAL_OTEL_SDK
// Defined below, next to the chat handling it belongs with.
static bool parse_ch_announce(const std::string &line, std::string &caster, std::string &target,
                              int &position, int &mana_percent);
// "GO - <name>": the cue the macro sends to whoever is next in the chain.
static bool parse_ch_handoff(const std::string &line, std::string &next_caster);
#endif

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

// "a_temple_guard00" -> "Transient Patroller" style: underscores to spaces, instance digits dropped,
// case preserved. The instance suffix is a spawn ID, and OpenTelemetry is explicit that IDs do not
// belong in metric attributes - each one mints a new timeseries, so a camp of respawning mobs walks
// the metric toward the cardinality limit for no analytical gain. It also makes the same mob look
// like strangers across pulls, when "damage onto that mob" is the actual question. Chat-parsed names
// (DoT ticks) already arrive in this form, so this makes both sources agree.
std::string target_display(const std::string &raw) {
  std::string d = raw;
  while (!d.empty() && isdigit(static_cast<unsigned char>(d.back()))) d.pop_back();
  for (auto &c : d)
    if (c == '_') c = ' ';
  return d;
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

// The group's leader names the group ("" when ungrouped). Game thread only.
//
// Leader rather than a roster hash: a roster identifier changes the moment anyone joins or leaves,
// which splits a timeseries mid-fight, whereas leadership rarely changes hands.
std::string current_group_leader() {
  const Zeal::GameStructures::GroupInfo *gi = Zeal::Game::GroupInfo;
  if (!Zeal::Game::is_in_game() || !gi || !gi->is_in_group()) return "";
  return gi->LeaderName;
}

// True if `name` is the local character or one of their group-mates. Game thread only.
bool in_my_group(const std::string &name) {
  const Zeal::GameStructures::GroupInfo *gi = Zeal::Game::GroupInfo;
  if (!Zeal::Game::is_in_game() || !gi || !gi->is_in_group()) return false;
  Zeal::GameStructures::GAMECHARINFO *ci = Zeal::Game::get_char_info();
  if (ci && name == ci->Name) return true;
  for (int i = 0; i < GAME_NUM_GROUP_MEMBERS; ++i)
    if (gi->IsValidList[i] && name == gi->Names[i]) return true;
  return false;
}

// The group a damage/heal record belongs to, or "" for none.
//
// Under `/otlp scope all` we record every attacker we can see, including players who are not grouped
// with us - and stamping our own leader on their damage would fold strangers into our group's total.
// So the label is only attached when the damage is genuinely the group's: dealt by a group member
// (pets already resolve to their owner before this point), or taken by us, which is by definition a
// group member taking damage.
std::string group_for(const std::string &source, const std::string &direction) {
  const bool ours = (direction == "incoming") || in_my_group(source);
  return ours ? current_group_leader() : "";
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

// Maps the damage packet's `type` field to a canonical label.
//
// This field is NOT a SkillType, despite looking like one. The server sends
// SkillDamageTypes[skill] (EQMacEmu common/eq_constants.h), a lookup that collapses several skills
// onto one value:
//
//   1HBlunt, 2HBlunt      -> 0        HandtoHand              -> 4
//   1HSlashing, 2HSlashing-> 1        Kick, RoundKick, Intimidation -> 30
//   Archery               -> 7        1HPiercing              -> 36
//   Backstab              -> 8        Throwing                -> 51
//   Bash                  -> 10       DragonPunch             -> 21
//   EagleStrike, TigerClaw-> 23       spell skills            -> 0xE7
//                                     no damage type          -> 0xFF
//
// Reading it as a SkillType happened to work for slash (1) and bash (10), because those values
// coincide, and quietly mislabelled everything else - hand-to-hand (4) landed in the "melee"
// catch-all, which is how a bite showed up as generic melee.
const char *damage_type_name(unsigned short damage_type) {
  switch (damage_type) {
    case 0: return "crush";
    case 1: return "slash";
    case 4: return "hth";
    case 7: return "archery";
    case 8: return "backstab";
    case 10: return "bash";
    case 21: return "special";  // dragon punch
    case 23: return "special";  // eagle strike / tiger claw
    case 30: return "kick";     // kick, round kick, intimidation
    case 36: return "pierce";
    case 51: return "throwing";
    case 0xE7: return "spell";  // DamageTypeSpell
    default: return "melee";    // includes DamageTypeUnknown (0xFF)
  }
}

// Damage shield hits arrive as ordinary damage packets, but the server puts a DmgShieldType into the
// field where a SkillType would normally go, so they cannot be classified with damage_type_name:
//
//   Mob::DamageShield()                    (EQMacEmu zone/attack.cpp, which Quarm derives from)
//     cds->type    = spellbonuses.DamageShieldType;   // DS_DECAY(244) .. DS_THORNS(249)
//     cds->spellid = spellid;                         // the DS spell, or SPELL_UNKNOWN
//
// NOTE: on Project Quarm this never fires - verified in game with `/otlp debug`, where shield hits
// produced chat messages but no hit event at all (see parse_dot_or_heal, which handles them from
// text). It is kept because it costs nothing and is correct for servers that do deliver a shield as
// a damage packet, where the alternative is silently filing it under "spell" or "melee".
bool is_damage_shield_type(unsigned short type) {
  constexpr unsigned short kDsDecay = 244;
  constexpr unsigned short kDsThorns = 249;
  return type >= kDsDecay && type <= kDsThorns;
}

}  // namespace

OtlpExporter::OtlpExporter(ZealService *zeal) {
  start_time_unix_nano = now_unix_nano();

  // Emit every print-to-chat line as a log record and mine it for combat damage. This is the live
  // source (the pipe uses the same callback); registering our own keeps OTLP independent of whether
  // the pipe has a reader connected.
  zeal->chat_hook->add_print_chat_callback([this](const char *data, int color_index) {
    if (!is_enabled() || !data) return;
    log(data, color_index);
#ifdef ZEAL_OTEL_SDK
    {
      // Complete Heal chains. The announcement is a cleric's macro line, not a combat message, so it
      // is read here rather than from the damage packets.
      std::string ch_caster, ch_target;
      std::string ch_next;
      if (parse_ch_handoff(data, ch_next)) {
        zeal::instrumentation::NoteChainHandoff(ch_next);
        if (debug_hits.load()) Zeal::Game::print_chat("OTLP CH handoff -> %s", ch_next.c_str());
      }
      int ch_mana = -1, ch_pos = -1;
      if (parse_ch_announce(data, ch_caster, ch_target, ch_pos, ch_mana)) {
        zeal::instrumentation::RecordCompleteHeal(ch_caster, ch_target, current_zone_name(), ch_pos, ch_mana);
        if (debug_hits.load())
          Zeal::Game::print_chat("OTLP CH%i: caster=%s target=%s mana=%i%%", ch_pos, ch_caster.c_str(),
                                 ch_target.c_str(), ch_mana);
      }
    }
#endif
    // DoT ticks and heal amounts never fire the hit event and are only messaged to the involved
    // client, so text is the correct (and self-reporting, non-duplicating) channel for them.
    parse_dot_or_heal(data);
    parse_lockout(data);
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
    std::string pet_name;  // kept alongside the owner rather than replaced by it
    if (attacker->PetOwnerSpawnId != 0) {  // credit a pet's damage to its owner (charmed or summoned)
      auto *owner = ZealService::get_instance()->entity_manager->Get(attacker->PetOwnerSpawnId);
      if (owner) {
        pet_name = attacker->Name;
        attacker = owner;
      }
    }
    std::string src = attacker->Name;
    const char *src_type = (attacker->Type == Zeal::GameEnums::EntityTypes::Player) ? "player" : "npc";
    const char *direction = (target && target == Zeal::Game::get_self()) ? "incoming" : "outgoing";
    // Tested before spell_id: a buff-granted damage shield carries one, and would otherwise be
    // indistinguishable from a nuke.
    const char *dtype = is_damage_shield_type(type) ? "damage_shield"
                        : (spell_id > 0)            ? "spell"
                                                    : damage_type_name(type);
    // Two forms deliberately: the metric attribute is normalised (see target_display), while fight
    // spans keep the raw spawn name so two mobs of the same name pulled at once stay separate
    // encounters - a trace is about one fight, a metric is about damage onto that kind of target.
    const std::string raw_tgt = target ? target->Name : "";
    const std::string tgt = target_display(raw_tgt);
    if (debug_hits.load()) {
      // The raw packet fields, before interpretation: `type` is a SkillType for melee but a
      // DmgShieldType (244..249) for a damage shield, and spell_id is -1 (SPELL_UNKNOWN) when the
      // server sends no spell. Printing both is what makes a misclassification visible.
      Zeal::Game::print_chat("[otlp] %s -> %s  type=%u spell=%d dmg=%d  => %s (%s)", src.c_str(), tgt.c_str(),
                             static_cast<unsigned int>(type), static_cast<int>(spell_id), static_cast<int>(damage),
                             dtype, direction);
    }
    // Damage *taken* is always recorded, whatever the scope. The scope filter exists to stop several
    // clients reporting the same attacker, but only the victim is told about damage landing on it, so
    // there is nobody to duplicate - and filtering on the attacker meant a tank running the default
    // scope=self dropped its own damage taken, which is precisely the number a raid wants next to
    // healing received.
    const bool incoming = (direction[0] == 'i');
    if (incoming || in_combat_scope(src))
      record_combat_damage(src, src_type, direction, dtype, tgt, pet_name, damage);
    // Fight spans: track encounters with NPCs (the mob is the target when we hit it, the source
    // when it hits us).
    const bool outgoing = (direction[0] == 'o');
    const std::string mob = outgoing ? raw_tgt : std::string(source->Name);
    if (!mob.empty()) note_fight_damage(mob, src, outgoing, damage);
  });

  // Sample live game state on the game thread (MainLoop); the sender thread only ever serializes the
  // cached snapshot, so it never races the game thread reading/freeing character structures.
  zeal->callbacks->AddGeneric([this]() {
    sample_game_state();
    if (is_enabled()) fight_tick();
#ifdef ZEAL_OTEL_SDK
    zeal::instrumentation::SweepCompleteHealChains();
#endif
#ifdef ZEAL_OTEL_SDK
    // The providers are built once and live for the process; only the character changes, and it is
    // a measurement attribute now, so keeping it current is a string assignment rather than a
    // teardown and rebuild of the whole pipeline.
    if (is_enabled()) {
      if (!zeal::telemetry::Running()) {
        std::string err;
        if (!zeal::telemetry::Start(setting_endpoint.get(), auth_token(), setting_flush_ms.get(),
                                    ZEAL_VERSION "+" ZEAL_BUILD_VERSION, err))
          Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "OpenTelemetry failed to start: %s", err.c_str());
      }
      std::lock_guard<std::mutex> lock(snapshot_mutex);
      zeal::instrumentation::SetCharacter(snapshot.in_game ? snapshot.name : std::string());
      if (snapshot.in_game) {
        zeal::instrumentation::SetCharacterState(snapshot.zone, snapshot.attack, snapshot.have_attack,
                                                 snapshot.haste, snapshot.have_haste);
        zeal::instrumentation::SetCharacterStats(snapshot.str, snapshot.sta, snapshot.dex, snapshot.agi,
                                                 snapshot.wis, snapshot.intel, snapshot.cha);
      }
    }
#endif
  });

  zeal->commands_hook->Add("/otlp", {},
                           "OTLP/HTTP telemetry export. Usage: /otlp on|off|status|endpoint <url>|flush <ms>|"
                           "scope self|all|debug",
                           [this](std::vector<std::string> &args) {
                             if (args.size() == 2 && Zeal::String::compare_insensitive(args[1], "on")) {
                               setting_enabled.set(true);
                               client->set_enabled(true);
                               // Names the paths actually used. Logs are deliberately not
                               // exported - chat carries private conversation - and this line used
                               // to advertise the one path nothing posts to.
                               Zeal::Game::print_chat("OTLP export enabled -> %s/v1/{metrics,traces}",
                                                      setting_endpoint.get().c_str());
                               return true;
                             }
                             if (args.size() == 2 && Zeal::String::compare_insensitive(args[1], "off")) {
                               setting_enabled.set(false);
                               client->set_enabled(false);
                               Zeal::Game::print_chat("OTLP export disabled.");
                               return true;
                             }
                             if (args.size() == 3 && Zeal::String::compare_insensitive(args[1], "endpoint")) {
                               setting_endpoint.set(args[2]);
                               client->set_endpoint(args[2]);
                               restart_pipeline();
                               Zeal::Game::print_chat("OTLP endpoint set to %s", args[2].c_str());
                               return true;
                             }
                             // Deliberately never echoes the value, and does not
                             // fall through to the generic usage line on a bad
                             // argument. print_chat output is written to
                             // eqlog_*.txt whenever logging is on - the same file
                             // people hand over when asking for help with their UI,
                             // and the one pq companion reads. A token that reaches
                             // a log is a token to reissue.
                             if (args.size() >= 2 && Zeal::String::compare_insensitive(args[1], "token")) {
                               if (args.size() == 3 &&
                                   Zeal::String::compare_insensitive(args[2], "clear")) {
                                 setting_token_sealed.set("");
                                 token_plain.clear();
                                 token_loaded = true;
                                 client->set_auth_token("");
                                 restart_pipeline();
                                 Zeal::Game::print_chat("OTLP token cleared.");
                                 return true;
                               }
                               if (args.size() != 3) {
                                 Zeal::Game::print_chat("Usage: /otlp token <token> | /otlp token clear");
                                 return true;
                               }
                               const std::string sealed = seal_token(args[2]);
                               if (sealed.empty()) {
                                 Zeal::Game::print_chat("OTLP token could not be stored (DPAPI failed).");
                                 return true;
                               }
                               setting_token_sealed.set(sealed);
                               token_plain = args[2];
                               token_loaded = true;
                               client->set_auth_token(token_plain);
                               restart_pipeline();
                               Zeal::Game::print_chat("OTLP token stored (sealed to this Windows account).");
                               return true;
                             }
                             if (args.size() == 3 && Zeal::String::compare_insensitive(args[1], "flush")) {
                               int ms = 0;
                               if (!Zeal::String::tryParse(args[2], &ms)) {
                                 Zeal::Game::print_chat("Usage: /otlp flush <milliseconds>");
                                 return true;
                               }
                               if (ms < OtlpClient::kMinFlushMs)
                                 ms = OtlpClient::kMinFlushMs;  // Clamp: metrics are periodic; 0 would just hammer.
                               setting_flush_ms.set(ms);
                               client->set_flush_ms(ms);
#ifdef ZEAL_OTEL_SDK
                               // The SDK reads this when its providers are built, so the setting
                               // only becomes real on a rebuild. Without this the command changed
                               // a number in the ini and nothing else - the export interval stayed
                               // wherever it started.
                               restart_pipeline();
                               // Clamped with comparisons rather than std::min/max: windows.h is
                               // in scope here and defines min/max as macros, and NOMINMAX cannot
                               // be set for this file (see telemetry.cpp - 164 call sites rely on
                               // the macros).
                               int effective = ms;
                               if (effective < zeal::telemetry::kMinExportIntervalMs)
                                 effective = zeal::telemetry::kMinExportIntervalMs;
                               if (effective > zeal::telemetry::kMaxExportIntervalMs)
                                 effective = zeal::telemetry::kMaxExportIntervalMs;
                               if (effective != ms)
                                 Zeal::Game::print_chat(
                                     "OTLP flush interval set to %ims (clamped from %ims: every export "
                                     "re-sends every series)",
                                     effective, ms);
                               else
                                 Zeal::Game::print_chat("OTLP flush interval set to %ims", effective);
#else
                               Zeal::Game::print_chat("OTLP flush interval set to %ims", ms);
#endif
                               return true;
                             }
                             if (args.size() == 2 && Zeal::String::compare_insensitive(args[1], "debug")) {
                               const bool on = !debug_hits.load();
                               debug_hits.store(on);
                               Zeal::Game::print_chat(
                                   on ? "OTLP hit debug ON - every recorded hit prints its raw packet fields. "
                                        "Spammy; /otlp debug again to stop."
                                      : "OTLP hit debug off.");
                               return true;
                             }
#ifdef ZEAL_OTEL_SDK
                             // Reports what the SDK pipeline is doing. The probe command this
                             // replaces existed to start the SDK by hand; the providers now come up
                             // with the exporter, so there is nothing left to trigger.
                             if (args.size() == 2 && Zeal::String::compare_insensitive(args[1], "sdk")) {
                               Zeal::Game::print_chat("OpenTelemetry SDK: %s",
                                                      zeal::telemetry::Running() ? "running" : "not started");
                               Zeal::Game::print_chat("  metrics, logs and traces over WinHTTP -> %s",
                                                      setting_endpoint.get().c_str());
                               return true;
                             }
#endif
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
                             // Reports the interval actually in force, not the number in the ini:
                             // the two differ whenever the setting is below the floor.
                             Zeal::Game::print_chat("OTLP: %s, endpoint %s, flush %ims, scope %s",
                                                    setting_enabled.get() ? "enabled" : "disabled",
                                                    setting_endpoint.get().c_str(), setting_flush_ms.get(),
                                                    setting_combat_scope.get().c_str());
#ifdef ZEAL_OTEL_SDK
                             // The SDK owns delivery here, so report *its* transport. The
                             // hand-rolled client's counters sit at zero in this build and reading
                             // them as health is how an exporter that sent nothing looked fine.
                             {
                               const auto st = zeal::telemetry::Stats();
                               Zeal::Game::print_chat("  sent: %llu payloads, %llu failed", st.posted,
                                                      st.failed);
                               Zeal::Game::print_chat("  last HTTP status: %i", st.last_status);
                               if (!st.last_error.empty())
                                 Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "  last error: %s",
                                                        st.last_error.c_str());
                             }
#else
                             Zeal::Game::print_chat("  sent: %llu log lines, %llu payloads, %llu failed",
                                                    logs_posted.load(), client->posted(), client->failed());
                             Zeal::Game::print_chat("  last HTTP status: %i", client->last_status());
#endif
                             // Presence and the last four characters: enough to
                             // tell two tokens apart without putting a working
                             // one in the chat log.
                             {
                               // "Stored but unreadable" is a different problem from "never set",
                               // and reporting both as "not set" is what hid a truncating ini read:
                               // the token was there, the client could not use it, and nothing said
                               // so while uploads went out unauthenticated.
                               const std::string &tok = auth_token();
                               if (!tok.empty())
                                 Zeal::Game::print_chat("  token: set (ends %s)",
                                                        tok.substr(tok.size() - 4).c_str());
                               else if (setting_token_sealed.get().empty())
                                 Zeal::Game::print_chat("  token: not set");
                               else
                                 Zeal::Game::print_chat(
                                     USERCOLOR_SPELL_FAILURE,
                                     "  token: stored but could not be read - set it again with "
                                     "/otlp token <token>");
                             }
#ifndef ZEAL_OTEL_SDK
                             {
                               const std::string err = client->last_error();
                               if (!err.empty())
                                 Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "  last error: %s", err.c_str());
                             }
#endif
                             Zeal::Game::print_chat("Usage: /otlp on|off|status|endpoint <url>|token <token>|flush <ms>|scope self|all");
                             return true;
                           });


  // Constructed last: the worker calls collect() as soon as it starts, so every piece of state it
  // reads must already exist.
  client = std::make_unique<OtlpClient>([this]() { return collect(); });
  client->set_endpoint(setting_endpoint.get());
  client->set_flush_ms(setting_flush_ms.get());
  client->set_enabled(setting_enabled.get());
  client->set_auth_token(auth_token());
}

OtlpExporter::~OtlpExporter() {
#ifdef ZEAL_OTEL_SDK
  // Shut the SDK down first: the metric reader and batch processors own threads that would
  // otherwise still be running when this DLL unloads, exporting from memory that no longer exists.
  zeal::telemetry::Stop();
#endif
  client.reset();  // stops and joins the worker before any state it collects from goes away
}

// Pulls the caster, target and reported mana out of a Complete Heal announcement.
//
// The macro is "/4 <n> - CH - %t (%n)", where the parenthesised value is the caster's *mana*, not
// their name - so the caster has to come from the channel line's speaker prefix. Matching on the
// " CH - " marker rather than the whole line keeps this working across channel formats and whatever
// numbering convention a guild puts in front.
static bool parse_ch_announce(const std::string &line, std::string &caster, std::string &target,
                              int &position, int &mana_percent) {
  const size_t marker = line.find(" CH - ");
  if (marker == std::string::npos) return false;
  const size_t pos = marker + 6;

  // Target runs to the mana parenthesis, the closing quote, or end of line.
  size_t stop = line.find(" (", pos);
  const size_t quote = line.find('\'', pos);
  if (quote != std::string::npos && (stop == std::string::npos || quote < stop)) stop = quote;
  if (stop == std::string::npos) stop = line.size();
  target = Zeal::String::trim_and_reduce_spaces(line.substr(pos, stop - pos));
  if (target.empty()) return false;

  // The rotation slot is whatever number sits immediately before the marker: "1 - CH - ...".
  // Scanning back from the marker rather than anchoring to the line start keeps it independent of
  // the channel prefix the client puts in front.
  position = -1;
  {
    size_t scan = marker;
    while (scan > 0 && !isdigit(static_cast<unsigned char>(line[scan - 1]))) {
      const char c = line[scan - 1];
      if (c != ' ' && c != '-') break;  // only whitespace and the dash may sit between
      scan--;
    }
    size_t digits_end = scan;
    while (scan > 0 && isdigit(static_cast<unsigned char>(line[scan - 1]))) scan--;
    if (digits_end > scan) {
      int parsed = 0;
      if (Zeal::String::tryParse(line.substr(scan, digits_end - scan), &parsed, true)) position = parsed;
    }
  }

  mana_percent = -1;
  const size_t open = line.find(" (", pos);
  if (open != std::string::npos) {
    const size_t close = line.find(')', open);
    if (close != std::string::npos) {
      int parsed = 0;
      if (Zeal::String::tryParse(Zeal::String::trim_and_reduce_spaces(line.substr(open + 2, close - open - 2)),
                                 &parsed, true))
        mana_percent = parsed;  // 0-100; validated where it is recorded
    }
  }

  // The speaker prefix the client adds to channel lines: "<name> tells ...".
  const size_t tells = line.find(" tells ");
  if (tells != std::string::npos && tells < marker)
    caster = Zeal::String::trim_and_reduce_spaces(line.substr(0, tells));
  return !caster.empty();
}

// "/4 GO - Abuelo" - the last line of the macro, telling the next cleric to start. Anchored on
// " GO - " for the same reason as the CH marker: the channel prefix varies and the guild's own
// numbering sits in front of it.
static bool parse_ch_handoff(const std::string &line, std::string &next_caster) {
  const size_t marker = line.find(" GO - ");
  if (marker == std::string::npos) return false;
  size_t stop = line.find('\'', marker + 6);
  if (stop == std::string::npos) stop = line.size();
  next_caster = Zeal::String::trim_and_reduce_spaces(line.substr(marker + 6, stop - marker - 6));
  // A name, not a sentence: anything with spaces is someone talking about the chain, not a cue.
  return !next_caster.empty() && next_caster.find(' ') == std::string::npos;
}

// ---------------------------------------------------------------------------
// Token storage
//
// The value is a bearer credential for the guild's ingest gateway. It is
// sealed with CryptProtectData, which ties it to this Windows account, and
// base64'd so it survives an ini round-trip. A blob lifted out of someone
// else's zeal.ini decrypts to nothing on this machine.
// ---------------------------------------------------------------------------

std::string OtlpExporter::seal_token(const std::string &plain) {
  if (plain.empty()) return "";
  DATA_BLOB in = {static_cast<DWORD>(plain.size()), (BYTE *)plain.data()};
  DATA_BLOB out = {0, nullptr};
  if (!CryptProtectData(&in, L"Zeal OTLP token", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN,
                        &out))
    return "";
  DWORD chars = 0;
  std::string encoded;
  if (CryptBinaryToStringA(out.pbData, out.cbData, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr,
                           &chars) &&
      chars > 0) {
    encoded.resize(chars);
    if (!CryptBinaryToStringA(out.pbData, out.cbData, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                              &encoded[0], &chars))
      encoded.clear();
    else
      encoded.resize(strlen(encoded.c_str()));
  }
  LocalFree(out.pbData);
  return encoded;
}

std::string OtlpExporter::unseal_token(const std::string &sealed) {
  if (sealed.empty()) return "";
  DWORD bytes = 0;
  if (!CryptStringToBinaryA(sealed.c_str(), 0, CRYPT_STRING_BASE64, nullptr, &bytes, nullptr, nullptr) ||
      bytes == 0)
    return "";
  std::vector<BYTE> raw(bytes);
  if (!CryptStringToBinaryA(sealed.c_str(), 0, CRYPT_STRING_BASE64, raw.data(), &bytes, nullptr, nullptr))
    return "";
  DATA_BLOB in = {bytes, raw.data()};
  DATA_BLOB out = {0, nullptr};
  if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out))
    return "";  // another account's blob, or a corrupted ini: upload unauthenticated
  std::string plain(reinterpret_cast<char *>(out.pbData), out.cbData);
  LocalFree(out.pbData);
  return plain;
}

// Rebuild the SDK pipeline so an endpoint or token change actually takes effect.
//
// The providers capture both when they are constructed - the exporters hold their own copies of the
// URL and the headers - and telemetry.h says so plainly: Start() is a no-op while running. Without
// this, /otlp endpoint and /otlp token updated a transport that, in an SDK build, sends nothing,
// and the real exporter carried on posting to wherever it was first pointed. Stop() here and the
// periodic sampler starts it again with the new values on its next tick.
void OtlpExporter::restart_pipeline() {
#ifdef ZEAL_OTEL_SDK
  if (zeal::telemetry::Running()) zeal::telemetry::Stop();
#endif
}

const std::string &OtlpExporter::auth_token() const {
  if (!token_loaded) {
    token_plain = unseal_token(setting_token_sealed.get());
    token_loaded = true;
  }
  return token_plain;
}

void OtlpExporter::log(const std::string &body, int color_index) {
  // Chat is no longer collected at all.
  //
  // It only ever reached the player's own collector, whose logs pipeline dropped it - so it produced
  // no observability while carrying the most sensitive content in the game, private tells among it.
  // Not collecting it is a stronger guarantee than a pipeline that promises to discard it, and one
  // fewer thing anyone has to take on trust when they install this.
  //
  // The chat hook itself stays: it is what combat damage is parsed from.
  (void)body;
  (void)color_index;
}

std::vector<OtlpClient::Payload> OtlpExporter::collect() {
  std::vector<OtlpClient::Payload> payloads;

  std::vector<LogRecord> batch;
  {
    std::lock_guard<std::mutex> lock(queue_mutex);
    const int max_batch = setting_max_batch.get() > 0 ? setting_max_batch.get() : 512;
    while (!queue.empty() && static_cast<int>(batch.size()) < max_batch) {
      batch.push_back(std::move(queue.front()));
      queue.pop_front();
    }
  }
  if (!batch.empty()) {
    // Chat is not collected and spans are the SDK's now; nothing produces these any more.
    logs_posted += batch.size();
  }
  // Metrics use cumulative temporality, so emit the current snapshot every flush even when no new
  // log lines arrived this cycle.
  payloads.emplace_back("/v1/metrics", build_metrics_payload());
  payloads.emplace_back("/v1/traces", build_traces_payload());
  return payloads;
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
    // Group roster. GroupInfo.Names is a fixed array gated by IsValidList, and does not include the
    // local character, so add it explicitly - otherwise the reporting player is missing from their
    // own group.
    const Zeal::GameStructures::GroupInfo *gi = Zeal::Game::GroupInfo;
    if (gi && gi->is_in_group()) {
      s.group_leader = gi->LeaderName;
      s.group_members.push_back(s.name);
      for (int i = 0; i < GAME_NUM_GROUP_MEMBERS; ++i) {  // 5 slots: the group minus yourself
        if (!gi->IsValidList[i] || gi->Names[i][0] == 0) continue;
        if (s.name == gi->Names[i]) continue;  // belt and braces against a duplicate self entry
        s.group_members.push_back(gi->Names[i]);
      }
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
  // ZEAL_VERSION alone cannot distinguish two builds of the same release, and every build of this
  // branch reports 1.4.5 - so "which build is that member running?" was unanswerable, which is
  // exactly the question that comes up when a feature appears to be missing for one person. The
  // short git hash the build workflow compiles in (or "UNOFFICIAL" for a local build) is appended as
  // semver build metadata, which is what that field is for.
  attrs.push_back(string_attr("service.version", ZEAL_VERSION "+" ZEAL_BUILD_VERSION));
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
#ifndef ZEAL_OTEL_SDK
    // Migrated to a change-driven gauge (see instrumentation.cpp). These cannot live on the Resource
    // any more: it is fixed for the process, and a stat that changes when you swap gear would either
    // go stale or force the whole pipeline to be rebuilt.
    attrs.push_back(int_attr("everquest.character.stat.strength", s.str));
    attrs.push_back(int_attr("everquest.character.stat.stamina", s.sta));
    attrs.push_back(int_attr("everquest.character.stat.dexterity", s.dex));
    attrs.push_back(int_attr("everquest.character.stat.agility", s.agi));
    attrs.push_back(int_attr("everquest.character.stat.wisdom", s.wis));
    attrs.push_back(int_attr("everquest.character.stat.intelligence", s.intel));
    attrs.push_back(int_attr("everquest.character.stat.charisma", s.cha));
#endif
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
                                        const std::string &target, const std::string &pet,
                                        long long amount) {
  const std::string zone = current_zone_name();     // where the hit happened (game thread)
  const std::string group = group_for(source, direction);  // "" unless it is genuinely our group's
#ifdef ZEAL_OTEL_SDK
  // Migrated: the SDK owns this metric now, so the hand-rolled aggregation below is skipped rather
  // than run alongside - both emit everquest.combat.damage and would otherwise collide. Parity was
  // verified against live combat first (12 series, 12 labels, identical values).
  zeal::instrumentation::RecordDamage(source, source_type, direction, type, zone, target, group, pet, amount);
#else
  std::lock_guard<std::mutex> lock(metrics_mutex);
  CombatTotal &entry = combat_damage[{source, source_type, direction, type, zone, target, group}];
  entry.total += amount;
  entry.last_ms = GetTickCount64();
#endif
}

void OtlpExporter::record_heal(const std::string &source, const std::string &direction, long long amount) {
  const std::string zone = current_zone_name();
  const std::string group = group_for(source, direction);  // "" unless it is genuinely our group's
#ifdef ZEAL_OTEL_SDK
  zeal::instrumentation::RecordHeal(source, direction, zone, group, amount);
#else
  std::lock_guard<std::mutex> lock(metrics_mutex);
  combat_heal[{source, direction, zone, group}] += amount;
#endif
}

// Extracts the positive integer that starts at line[pos] (returns 0 if none).
static long long read_number(const std::string &line, size_t pos) {
  size_t end = pos;
  while (end < line.size() && isdigit(static_cast<unsigned char>(line[end]))) end++;
  return (end > pos) ? atoll(line.substr(pos, end - pos).c_str()) : 0;
}

// Damage shield damage never reaches the ReportSuccessfulHit hook on this server - verified in game
// with `/otlp debug`, where every melee swing printed a hit line but the shield produced only chat:
//
//   A vampyre bat was hit by non-melee for 30 points of damage.   <- message 434, the amount
//   A vampyre bat was chilled to the bone.                        <- message 12140, the cause
//
// Neither line is sufficient alone: 434 is also emitted for ordinary spell damage and names no
// attacker, and the flavour line carries no number. They are paired by arriving back to back for the
// same target, which is what this parser keys on. The six flavour messages (12132..12142) correspond
// one-to-one with the six DmgShieldType values the server tracks internally.
namespace {
const char *const kDamageShieldSuffixes[] = {
    " was pierced by thorns.", " was burned.", " was tormented.",
    " was chilled to the bone.", " is burning.", " is freezing!",
};

// "<target> was chilled to the bone." -> "<target>", or "" if the line is not a shield message.
std::string damage_shield_target(const std::string &line) {
  for (const char *suffix : kDamageShieldSuffixes) {
    const size_t len = strlen(suffix);
    if (line.size() > len && line.compare(line.size() - len, len, suffix) == 0)
      return line.substr(0, line.size() - len);
  }
  return "";
}
}  // namespace

void OtlpExporter::parse_dot_or_heal(const std::string &line) {
  const char *self = Zeal::Game::get_self() ? Zeal::Game::get_self()->Name : nullptr;
  if (!self) return;

  // Damage shield, part 2: a flavour line claims the amount buffered from the line before it. The
  // pending slot holds only the most recent 434 message, so an unrelated nuke on the same target
  // cannot be claimed unless it landed immediately before the shield fired.
  const std::string ds_target = damage_shield_target(line);
  if (!ds_target.empty()) {
    const bool claimed = !ds_pending_target.empty() && ds_pending_target == ds_target &&
                         GetTickCount64() - ds_pending_ms < 1000;
    if (claimed) {
      record_combat_damage(self, "player", "outgoing", "damage_shield", ds_pending_target, "", ds_pending_amount);
    }
    if (debug_hits.load()) {
      // Shields take the text path, so without this `/otlp debug` would stay silent for them and a
      // pairing that never fires would look identical to one that does.
      if (claimed)
        Zeal::Game::print_chat("[otlp] damage_shield %s dmg=%lld (paired with the preceding non-melee line)",
                               ds_target.c_str(), ds_pending_amount);
      else
        Zeal::Game::print_chat("[otlp] damage_shield %s UNPAIRED - no non-melee amount to claim", ds_target.c_str());
    }
    ds_pending_target.clear();
    return;
  }

  // Damage shield, part 1: buffer "<target> was hit by non-melee for <N> points of damage." Kept
  // rather than recorded, because this message is also how ordinary spell damage is announced.
  static const char kNonMelee[] = " was hit by non-melee for ";
  const size_t nm = line.find(kNonMelee);
  if (nm != std::string::npos) {
    const long long amount = read_number(line, nm + sizeof(kNonMelee) - 1);
    if (amount > 0) {
      ds_pending_target = line.substr(0, nm);
      ds_pending_amount = amount;
      ds_pending_ms = GetTickCount64();
    }
    return;
  }

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
        if (debug_hits.load())
          Zeal::Game::print_chat("[otlp] dot %s -> %s dmg=%lld", self, dot_target.c_str(), amount);
        record_combat_damage(self, "player", "outgoing", "dot", dot_target, "", amount);
      } else {
        size_t apos = rest.find("'s ");  // "<caster>'s <spell>"
        if (apos != std::string::npos && setting_combat_scope.get() != "self") {
          std::string caster = rest.substr(0, apos);
          auto *e = ZealService::get_instance()->entity_manager->Get(caster);
          const char *ct = (e && e->Type == Zeal::GameEnums::EntityTypes::Player) ? "player" : "npc";
          record_combat_damage(caster, ct, "outgoing", "dot", dot_target, "", amount);
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
      if (amount > 0) {
        if (debug_hits.load()) Zeal::Game::print_chat("[otlp] heal outgoing %lld", amount);
        record_heal(self, "outgoing", amount);
      }
    }
    return;
  }

  // Heal received (target-side): "You have been healed for <N> points."
  p = line.find("You have been healed for ");
  if (p == 0) {
    long long amount = read_number(line, 25);
    if (amount > 0) {
      // Healing is only ever messaged to the target, so this is the line that carries every heal
      // landing on you - including your own self-heals.
      if (debug_hits.load()) Zeal::Game::print_chat("[otlp] heal received %lld", amount);
      record_heal(self, "incoming", amount);
    }
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

// Raid lockouts. The server builds this message in zone/corpse.cpp:
//
//   "You have incurred a lockout for " + npc->GetCleanName()
//     + " that expires in " + Strings::SecondsToTime(loot_lockout_timer) + "."
//
// SecondsToTime emits a *compound* duration - "18 Hours", but equally "1 Day, 2 Hours, 3 Minutes,
// and 4 Seconds", singular or plural per unit - so every number/unit pair has to be summed. Reading
// only the first pair (as this did at first) silently truncates: "17 Hours, 45 Minutes" would
// become 17 hours and the countdown would finish 45 minutes early.
//
// The expiry is recorded as an absolute instant, not a remaining duration: a countdown computed
// once would be wrong the moment it was exported, whereas a deadline stays correct for as long as
// the value survives, including after the player logs off.
void OtlpExporter::parse_lockout(const std::string &line) {
  static const char kFor[] = " lockout for ";
  static const char kExpires[] = " that expires in ";
  const size_t f = line.find(kFor);
  if (f == std::string::npos) return;
  const size_t e = line.find(kExpires, f);
  if (e == std::string::npos) return;

  const std::string target = line.substr(f + sizeof(kFor) - 1, e - (f + sizeof(kFor) - 1));
  if (target.empty()) return;

  // Sum every "<n> <unit>" pair in the tail. SecondsToTime returns "Unknown" for a non-positive
  // duration, which yields no pairs and is skipped.
  long long seconds = 0;
  for (size_t i = e + sizeof(kExpires) - 1; i < line.size();) {
    if (!isdigit(static_cast<unsigned char>(line[i]))) {
      ++i;
      continue;
    }
    const long long amount = read_number(line, i);
    while (i < line.size() && isdigit(static_cast<unsigned char>(line[i]))) ++i;
    while (i < line.size() && line[i] == ' ') ++i;
    std::string unit;
    while (i < line.size() && isalpha(static_cast<unsigned char>(line[i])))
      unit.push_back(static_cast<char>(tolower(static_cast<unsigned char>(line[i++]))));
    // "Milli..." before "Min...": both start with 'm'.
    if (unit.rfind("mil", 0) == 0) continue;  // sub-second precision is noise for an 18h lockout
    else if (unit.rfind("d", 0) == 0) seconds += amount * 86400;
    else if (unit.rfind("h", 0) == 0) seconds += amount * 3600;
    else if (unit.rfind("m", 0) == 0) seconds += amount * 60;
    else if (unit.rfind("s", 0) == 0) seconds += amount;
  }
  if (seconds <= 0) return;

  const long long now_s = static_cast<long long>(now_unix_nano() / 1000000000ULL);
  const long long expiry = now_s + seconds;
  {
    std::lock_guard<std::mutex> lock(metrics_mutex);
    lockouts[target] = expiry;
    // The server emits this notice from NPC::CreateCorpse, i.e. at the kill, so this is the time of
    // death rather than the time of looting.
    raid_kills[target] = now_s;
  }
  if (debug_hits.load())
    Zeal::Game::print_chat("[otlp] lockout %s in %lld s (expires at unix %lld)", target.c_str(), seconds, expiry);
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
      nlohmann::json attrs = nlohmann::json::array(
          {string_attr(everquest_semconv::kEverquestCombatSource, std::get<0>(key)),
           string_attr(everquest_semconv::kEverquestCombatSourceType, std::get<1>(key)),
           string_attr(everquest_semconv::kEverquestCombatDirection, std::get<2>(key)),
           string_attr(everquest_semconv::kEverquestCombatDamageType, std::get<3>(key)),
           string_attr(everquest_semconv::kEverquestZoneName, std::get<4>(key)),
           string_attr(everquest_semconv::kEverquestCombatTarget, std::get<5>(key))});
      // Omitted rather than sent empty when ungrouped: an absent attribute is a missing label in
      // Prometheus, which keeps solo play out of every group-scoped query.
      if (!std::get<6>(key).empty())
        attrs.push_back(string_attr(everquest_semconv::kEverquestGroupLeader, std::get<6>(key)));
      data_points.push_back({{"attributes", attrs},
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
      nlohmann::json attrs =
          nlohmann::json::array({string_attr(everquest_semconv::kEverquestCombatSource, std::get<0>(key)),
                                 string_attr(everquest_semconv::kEverquestCombatDirection, std::get<1>(key)),
                                 string_attr(everquest_semconv::kEverquestZoneName, std::get<2>(key))});
      if (!std::get<3>(key).empty())
        attrs.push_back(string_attr(everquest_semconv::kEverquestGroupLeader, std::get<3>(key)));
      heal_points.push_back({{"attributes", attrs},
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


void OtlpExporter::note_fight_damage(const std::string &raw_target, const std::string &attacker,
                                     bool outgoing, long long dmg) {
  const unsigned long long now = now_unix_nano();
  ActiveFight &f = active_fights[raw_target];
  if (f.start_ns == 0) {
    f.span_id = hex_id(8);
    f.display = display_of(raw_target);
    f.start_ns = now;
  }
  f.last_ns = now;
  (outgoing ? f.dmg_out : f.dmg_in) += dmg;

  if (!attacker.empty()) {
    // Engaged time, accumulated between consecutive hits. A gap longer than kAttackerIdleNs is
    // downtime rather than slow swings - counting it would flatter a caster who nuked once at the
    // start and once at the end into looking continuously engaged.
    auto &entry = f.attacker_activity[attacker];
    if (entry.first != 0 && now > entry.first) {
      const unsigned long long gap = now - entry.first;
      if (gap <= kAttackerIdleNs) entry.second += gap / 1e9;
    }
    entry.first = now;
  }
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
#ifdef ZEAL_OTEL_SDK
  {
    // Fight-scoped totals, so damage can be divided by a time that means something: the fight's
    // duration gives SDPS, each attacker's engaged seconds gives DPS. Both are what the community
    // parser prints, and matching its arithmetic is the difference between a number people use and
    // a number people argue with.
    const unsigned long long finished_ns = end_ns ? end_ns : f.last_ns;
    const double duration_s = (finished_ns > f.start_ns) ? (finished_ns - f.start_ns) / 1e9 : 0.0;
    std::vector<std::pair<std::string, double>> active;
    active.reserve(f.attacker_activity.size());
    for (const auto &[who, act] : f.attacker_activity) {
      // A single hit has no measurable span; credit it with one swing so a one-shot kill does not
      // divide by zero and report an infinite rate.
      active.emplace_back(who, act.second > 0.0 ? act.second : 1.0);
    }
    zeal::instrumentation::RecordFightTotals(key, zone_active, duration_s, active);
  }
  (void)sp;
#else
  sp.str_attrs = {{everquest_semconv::kEverquestCombatTarget, key}, {everquest_semconv::kEverquestZoneName, zone_active}, {"everquest.fight.outcome", outcome}};
  sp.int_attrs = {{"everquest.fight.damage.dealt", f.dmg_out}, {"everquest.fight.damage.taken", f.dmg_in}};
  {
    std::lock_guard<std::mutex> lock(spans_mutex);
    completed_spans.push_back(std::move(sp));
  }
#endif
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
#ifndef ZEAL_OTEL_SDK
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
#endif
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


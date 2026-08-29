#include "instrumentation.h"

#include "everquest_semconv.h"  // generated from the registry: a typo becomes a compile error

// API only - no sdk/ or exporters/ headers. See instrumentation.h.
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max

#include "opentelemetry/metrics/provider.h"
#include "opentelemetry/trace/provider.h"

#pragma pop_macro("max")
#pragma pop_macro("min")

#include <chrono>
#include <map>
#include <mutex>
#include <utility>
#include <vector>

namespace metrics_api = opentelemetry::metrics;
namespace trace_api = opentelemetry::trace;

namespace {

// The instrumentation scope: name and version identify *what produced the telemetry*, which is how
// a consumer tells our measurements from anything else in the process and reports problems against
// the right thing.
constexpr const char *kScopeName = "zeal.everquest";
constexpr const char *kScopeVersion = "1.0.0";

std::mutex g_mutex;
std::string g_character;

// What the observable gauges report. Sampled on the game thread, read on the SDK's collection
// thread - which is exactly why asynchronous instruments suit these: the value is behind an
// accessor, so it is observed when the SDK asks rather than polled on a timer of our own.
struct CharacterState {
  std::string zone;
  // Base stats, observed rather than pushed: the synchronous Gauge the spec would point at here is
  // gated behind OPENTELEMETRY_ABI_VERSION_NO >= 2, a preview ABI, and moving every instrument in a
  // client other people run onto unstable interfaces is not worth the distinction. A gauge sampled
  // on the export interval still shows a clean step when gear changes, which is the question worth
  // asking of a stat.
  int stats[7] = {0, 0, 0, 0, 0, 0, 0};
  long long attack = 0;
  long long haste = 0;
  bool have_attack = false;
  bool have_haste = false;
  bool valid = false;
};
CharacterState g_state;

using Counter = opentelemetry::nostd::shared_ptr<metrics_api::Counter<uint64_t>>;

// Created on first use. If no SDK is registered the API hands back a no-op instrument, which is
// valid to call - so this never needs a null check at the call site.
Counter &DamageCounter() {
  static Counter counter = metrics_api::Provider::GetMeterProvider()
                               ->GetMeter(kScopeName, kScopeVersion)
                               ->CreateUInt64Counter(everquest_semconv::kEverquestCombatDamageMetric,
                                                     "damage dealt or taken", "{hitpoint}");
  return counter;
}

// Seconds, as doubles: a fight is not a whole number of seconds and rounding every encounter down
// would bias every rate upward.
using DoubleCounter = opentelemetry::nostd::shared_ptr<metrics_api::Counter<double>>;

DoubleCounter &FightDurationCounter() {
  static DoubleCounter counter = metrics_api::Provider::GetMeterProvider()
                                     ->GetMeter(kScopeName, kScopeVersion)
                                     ->CreateDoubleCounter(everquest_semconv::kEverquestFightDurationMetric,
                                                           "seconds spent fighting a target", "s");
  return counter;
}

DoubleCounter &FightActiveCounter() {
  static DoubleCounter counter = metrics_api::Provider::GetMeterProvider()
                                     ->GetMeter(kScopeName, kScopeVersion)
                                     ->CreateDoubleCounter(everquest_semconv::kEverquestFightActiveMetric,
                                                           "seconds an attacker was dealing damage", "s");
  return counter;
}

Counter &HealCounter() {
  static Counter counter = metrics_api::Provider::GetMeterProvider()
                               ->GetMeter(kScopeName, kScopeVersion)
                               ->CreateUInt64Counter(everquest_semconv::kEverquestCombatHealMetric,
                                                     "hitpoints restored", "{hitpoint}");
  return counter;
}

using Attributes = std::vector<std::pair<std::string, opentelemetry::common::AttributeValue>>;

// Complete Heal's cast time. The span's whole value is showing whether these fixed windows overlap.
constexpr int kCompleteHealCastSeconds = 10;
// A chain is over once no cleric has announced for this long - two missed casts' worth.
constexpr int kChainIdleSeconds = 25;

// One open chain span per tank being chained. Children hang off it, so a chain is one trace.
struct Chain {
  opentelemetry::nostd::shared_ptr<trace_api::Span> span;
  std::chrono::steady_clock::time_point last;
  int casts = 0;
};
std::map<std::string, Chain> g_chains;


// Outstanding "GO" cues, keyed by the name that was called. A cue older than one cast is stale:
// whoever it was for either answered or was skipped, and attributing a 30s latency to the next
// person who happens to share the name would be worse than reporting nothing.
std::map<std::string, std::chrono::steady_clock::time_point> g_handoff_cues;

std::string LowerName(const std::string &s) {
  std::string out = s;
  for (auto &c : out) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  return out;
}

opentelemetry::nostd::shared_ptr<trace_api::Tracer> Tracer() {
  return trace_api::Provider::GetTracerProvider()->GetTracer(kScopeName, kScopeVersion);
}

// Observing nothing is meaningful: an asynchronous instrument only exports the attribute sets its
// callback reports, so a character who is not in game simply stops producing points rather than
// flatlining at a stale value.
void ObserveGauge(opentelemetry::metrics::ObserverResult result, bool haste) {
  CharacterState state;
  std::string character;
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    state = g_state;
    character = g_character;
  }
  if (!state.valid || character.empty()) return;
  if (haste ? !state.have_haste : !state.have_attack) return;

  Attributes attrs = {
      {everquest_semconv::kEverquestCharacterName, character.c_str()},
      {everquest_semconv::kEverquestZoneName, state.zone.c_str()},
  };
  auto observer = opentelemetry::nostd::get<
      opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<int64_t>>>(result);
  observer->Observe(haste ? state.haste : state.attack,
                    opentelemetry::common::KeyValueIterableView<Attributes>(attrs));
}

constexpr const char *kStatNames[7] = {"strength", "stamina",      "dexterity", "agility",
                                       "wisdom",   "intelligence", "charisma"};

void StatCallback(opentelemetry::metrics::ObserverResult result, void *) {
  CharacterState state;
  std::string character;
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    state = g_state;
    character = g_character;
  }
  if (!state.valid || character.empty()) return;
  auto observer = opentelemetry::nostd::get<
      opentelemetry::nostd::shared_ptr<opentelemetry::metrics::ObserverResultT<int64_t>>>(result);
  for (int i = 0; i < 7; ++i) {
    if (state.stats[i] <= 0) continue;  // not sampled yet
    Attributes attrs = {
        {everquest_semconv::kEverquestCharacterStatName, kStatNames[i]},
        {everquest_semconv::kEverquestCharacterName, character.c_str()},
    };
    observer->Observe(state.stats[i], opentelemetry::common::KeyValueIterableView<Attributes>(attrs));
  }
}

void AttackCallback(opentelemetry::metrics::ObserverResult result, void *) { ObserveGauge(result, false); }
void HasteCallback(opentelemetry::metrics::ObserverResult result, void *) { ObserveGauge(result, true); }

// Registered once. Holding the instruments alive matters: dropping them would unregister the
// callbacks and the gauges would silently stop reporting.
void EnsureGauges() {
  static auto attack = [] {
    auto g = metrics_api::Provider::GetMeterProvider()
                 ->GetMeter(kScopeName, kScopeVersion)
                 ->CreateInt64ObservableGauge(everquest_semconv::kEverquestCharacterAttackMetric,
                                              "current offense rating", "1");
    g->AddCallback(AttackCallback, nullptr);
    return g;
  }();
  static auto haste = [] {
    auto g = metrics_api::Provider::GetMeterProvider()
                 ->GetMeter(kScopeName, kScopeVersion)
                 ->CreateInt64ObservableGauge(everquest_semconv::kEverquestCharacterHasteMetric,
                                              "total effective melee haste", "%");
    g->AddCallback(HasteCallback, nullptr);
    return g;
  }();
  static auto stats = [] {
    auto g = metrics_api::Provider::GetMeterProvider()
                 ->GetMeter(kScopeName, kScopeVersion)
                 ->CreateInt64ObservableGauge(everquest_semconv::kEverquestCharacterStatMetric,
                                              "character base stat", "1");
    g->AddCallback(StatCallback, nullptr);
    return g;
  }();
  (void)attack;
  (void)haste;
  (void)stats;
}

void Add(Counter &counter, long long amount, Attributes &attrs) {
  if (amount <= 0) return;
  std::string character;
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    character = g_character;
  }
  if (!character.empty()) attrs.push_back({everquest_semconv::kEverquestCharacterName, character.c_str()});
  counter->Add(static_cast<uint64_t>(amount),
               opentelemetry::common::KeyValueIterableView<Attributes>(attrs));
}

}  // namespace

namespace zeal::instrumentation {

void SetCharacter(const std::string &name) {
  std::lock_guard<std::mutex> lock(g_mutex);
  g_character = name;
}

void SetCharacterState(const std::string &zone, long long attack, bool have_attack, long long haste,
                       bool have_haste) {
  EnsureGauges();
  std::lock_guard<std::mutex> lock(g_mutex);
  g_state.zone = zone;
  g_state.attack = attack;
  g_state.have_attack = have_attack;
  g_state.haste = haste;
  g_state.have_haste = have_haste;
  g_state.valid = true;
}

void RecordDamage(const std::string &source, const std::string &source_type, const std::string &direction,
                  const std::string &damage_type, const std::string &zone, const std::string &target,
                  const std::string &group_leader, const std::string &pet, long long amount) {
  Attributes attrs = {
      {everquest_semconv::kEverquestCombatSource, source.c_str()},
      {everquest_semconv::kEverquestCombatSourceType, source_type.c_str()},
      {everquest_semconv::kEverquestCombatDirection, direction.c_str()},
      {everquest_semconv::kEverquestCombatDamageType, damage_type.c_str()},
      {everquest_semconv::kEverquestZoneName, zone.c_str()},
      {everquest_semconv::kEverquestCombatTarget, target.c_str()},
  };
  if (!group_leader.empty()) attrs.push_back({everquest_semconv::kEverquestGroupLeader, group_leader.c_str()});
  if (!pet.empty()) attrs.push_back({everquest_semconv::kEverquestCombatPetName, pet.c_str()});
  Add(DamageCounter(), amount, attrs);
}

void SetCharacterStats(int strength, int stamina, int dexterity, int agility, int wisdom,
                       int intelligence, int charisma) {
  EnsureGauges();
  std::lock_guard<std::mutex> lock(g_mutex);
  const int values[7] = {strength, stamina, dexterity, agility, wisdom, intelligence, charisma};
  for (int i = 0; i < 7; ++i) g_state.stats[i] = values[i];
  g_state.valid = true;
}

void RecordFightTotals(const std::string &target, const std::string &zone, double duration_s,
                       const std::vector<std::pair<std::string, double>> &active_seconds_by_source) {
  if (duration_s <= 0) return;
  std::string character;
  {
    std::lock_guard<std::mutex> lock(g_mutex);
    character = g_character;
  }

  Attributes fight = {
      {everquest_semconv::kEverquestCombatTarget, target.c_str()},
      {everquest_semconv::kEverquestZoneName, zone.c_str()},
  };
  if (!character.empty()) fight.push_back({everquest_semconv::kEverquestCharacterName, character.c_str()});
  FightDurationCounter()->Add(duration_s,
                              opentelemetry::common::KeyValueIterableView<Attributes>(fight));

  for (const auto &[source, seconds] : active_seconds_by_source) {
    if (seconds <= 0) continue;
    Attributes attrs = {
        {everquest_semconv::kEverquestCombatSource, source.c_str()},
        {everquest_semconv::kEverquestCombatTarget, target.c_str()},
        {everquest_semconv::kEverquestZoneName, zone.c_str()},
    };
    if (!character.empty()) attrs.push_back({everquest_semconv::kEverquestCharacterName, character.c_str()});
    FightActiveCounter()->Add(seconds, opentelemetry::common::KeyValueIterableView<Attributes>(attrs));
  }
}

void RecordCompleteHeal(const std::string &caster, const std::string &target, const std::string &zone,
                        int chain_position, int caster_mana_percent) {
  const auto now = std::chrono::steady_clock::now();

  std::lock_guard<std::mutex> lock(g_mutex);
  auto &chain = g_chains[target];
  if (!chain.span) {
    Attributes chain_attrs = {
        {everquest_semconv::kEverquestCombatTarget, target.c_str()},
        {everquest_semconv::kEverquestZoneName, zone.c_str()},
    };
    if (!g_character.empty())
      chain_attrs.push_back({everquest_semconv::kEverquestCharacterName, g_character.c_str()});
    chain.span = Tracer()->StartSpan("CH chain: " + target,
                                     opentelemetry::common::KeyValueIterableView<Attributes>(chain_attrs));
  }
  chain.last = now;
  chain.casts++;

  Attributes attrs = {
      {everquest_semconv::kEverquestCombatSource, caster.c_str()},
      {everquest_semconv::kEverquestCombatTarget, target.c_str()},
      {everquest_semconv::kEverquestZoneName, zone.c_str()},
  };
  if (!g_character.empty()) attrs.push_back({everquest_semconv::kEverquestCharacterName, g_character.c_str()});
  // If this caster was cued, how long they took to answer it. Consumed either way, so a stale cue
  // cannot attach itself to a later cast.
  {
    auto cue = g_handoff_cues.find(LowerName(caster));
    if (cue != g_handoff_cues.end()) {
      const auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(now - cue->second).count();
      if (waited >= 0 && waited <= kCompleteHealCastSeconds * 3 * 1000)
        attrs.push_back({everquest_semconv::kEverquestHealChainHandoffMs, static_cast<long long>(waited)});
      g_handoff_cues.erase(cue);
    }
  }

  // The rotation slot the cleric was assigned. Absent rather than zero when the macro omits it.
  if (chain_position > 0)
    attrs.push_back({everquest_semconv::kEverquestHealChainPosition, chain_position});
  // Out of range means the macro did not report it, or reported something that is not a percentage;
  // an absent attribute beats a fabricated number that a chart would happily plot.
  if (caster_mana_percent >= 0 && caster_mana_percent <= 100)
    attrs.push_back({everquest_semconv::kEverquestHealCasterManaPercent, caster_mana_percent});

  // Start now, end 10s from now. Complete Heal's cast time is fixed, so the span's extent is known
  // at the announcement - which means it can be emitted immediately instead of held open for ten
  // seconds waiting for something that is not reported anyway.
  trace_api::StartSpanOptions options;
  options.parent = chain.span->GetContext();
  options.start_steady_time = opentelemetry::common::SteadyTimestamp(now);
  options.start_system_time = opentelemetry::common::SystemTimestamp(std::chrono::system_clock::now());
  const std::string span_name =
      chain_position > 0 ? ("CH" + std::to_string(chain_position) + ": " + caster) : ("CH: " + caster);
  auto span = Tracer()->StartSpan(span_name,
                                  opentelemetry::common::KeyValueIterableView<Attributes>(attrs), options);
  trace_api::EndSpanOptions end;
  end.end_steady_time = opentelemetry::common::SteadyTimestamp(now + std::chrono::seconds(kCompleteHealCastSeconds));
  span->End(end);
}

void NoteChainHandoff(const std::string &next_caster) {
  if (next_caster.empty()) return;
  std::lock_guard<std::mutex> lock(g_mutex);
  g_handoff_cues[LowerName(next_caster)] = std::chrono::steady_clock::now();
}

void SweepCompleteHealChains() {
  const auto now = std::chrono::steady_clock::now();
  std::lock_guard<std::mutex> lock(g_mutex);
  for (auto it = g_handoff_cues.begin(); it != g_handoff_cues.end();) {
    if (now - it->second > std::chrono::seconds(kCompleteHealCastSeconds * 3))
      it = g_handoff_cues.erase(it);
    else
      ++it;
  }
  for (auto it = g_chains.begin(); it != g_chains.end();) {
    if (now - it->second.last > std::chrono::seconds(kChainIdleSeconds)) {
      if (it->second.span) it->second.span->End();
      it = g_chains.erase(it);
    } else {
      ++it;
    }
  }
}

void RecordHeal(const std::string &source, const std::string &direction, const std::string &zone,
                const std::string &group_leader, long long amount) {
  Attributes attrs = {
      {everquest_semconv::kEverquestCombatSource, source.c_str()},
      {everquest_semconv::kEverquestCombatDirection, direction.c_str()},
      {everquest_semconv::kEverquestZoneName, zone.c_str()},
  };
  if (!group_leader.empty()) attrs.push_back({everquest_semconv::kEverquestGroupLeader, group_leader.c_str()});
  Add(HealCounter(), amount, attrs);
}

}  // namespace zeal::instrumentation

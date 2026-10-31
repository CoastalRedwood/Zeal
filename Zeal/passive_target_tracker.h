#pragma once
// Passive observations for AssistBar. The tracker never sends game commands.
// 'assist' view: most recent victim of the selected actor.
// 'defend' view: most recent actor that struck the selected victim.

#include <cstdint>
#include <unordered_map>

class PassiveTargetTracker {
 public:
  enum class Evidence { Melee, Animation };

  struct Observation {
    uint16_t other_id = 0;
    uint32_t timestamp_ms = 0;
    Evidence evidence = Evidence::Melee;
  };

  void clear() { by_actor_.clear(); }

  void observe(uint16_t actor, uint16_t victim, Evidence evidence, uint32_t now_ms) {
    if (!actor || !victim || actor == victim || actor == 0xffff || victim == 0xffff) return;
    by_actor_[actor] = Observation{victim, now_ms, evidence};
  }

  Observation find(uint16_t selected, bool defend, uint32_t now_ms,
                   uint32_t freshness_ms) const {
    if (!selected) return {};
    if (!defend) {
      const auto it = by_actor_.find(selected);
      if (it == by_actor_.end()) return {};
      if (now_ms - it->second.timestamp_ms > freshness_ms) return {};
      return it->second;
    }

    Observation newest{};
    uint32_t newest_age = 0;
    bool have_newest = false;
    for (const auto& [actor, obs] : by_actor_) {
      if (obs.other_id != selected) continue;
      const uint32_t age = now_ms - obs.timestamp_ms;
      if (age > freshness_ms || (have_newest && age >= newest_age)) continue;
      newest = Observation{actor, obs.timestamp_ms, obs.evidence};
      newest_age = age;
      have_newest = true;
    }
    return newest;
  }

 private:
  std::unordered_map<uint16_t, Observation> by_actor_;
};

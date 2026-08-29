// Instrumentation for EverQuest gameplay, written against the OpenTelemetry **API** only.
//
// Nothing here includes an sdk/ or exporters/ header, and nothing here knows where telemetry goes.
// If no SDK has been registered (see telemetry.h) every call is a no-op that costs a virtual call -
// so call sites need no #ifdef and no "is telemetry on?" check.
//
// Instruments are created once, on first use, and cached. That is only sound because the providers
// live for the process: the Resource is fixed at startup and everything that varies while the game
// runs - the character above all - travels as a measurement attribute.
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace zeal::instrumentation {

// The character the local player is currently on. Recorded as an attribute on every measurement
// rather than as part of the Resource, because it changes when camping to character select and
// identifying attributes of a Resource must not change during its lifetime.
void SetCharacter(const std::string &name);

// Slowly-changing character state, read by the observable gauges when the SDK collects. Called from
// the game thread; the gauges' callbacks run on the SDK's collection thread and read the snapshot
// this keeps, so neither touches game memory the other might be freeing.
void SetCharacterState(const std::string &zone, long long attack, bool have_attack, long long haste,
                       bool have_haste);

// The seven base stats. Recorded only when one actually changes - swapping gear, a buff landing,
// levelling - because a stat that has not moved is not news, and emitting it on a timer would make
// "when did this change?" impossible to answer. Safe and cheap to call every tick.
void SetCharacterStats(int strength, int stamina, int dexterity, int agility, int wisdom,
                       int intelligence, int charisma);

// Damage dealt or taken. `source_type` distinguishes player/pet/npc; `group_leader` is empty when
// solo, and is then omitted entirely - an absent attribute is a missing label in Prometheus, which
// keeps solo play out of group-scoped queries.
// `source` is the owner even when a pet swung - that is how raiders count, and how the parser they
// compare against reports it. `pet` names the pet underneath, empty when the owner swung themselves.
void RecordDamage(const std::string &source, const std::string &source_type, const std::string &direction,
                  const std::string &damage_type, const std::string &zone, const std::string &target,
                  const std::string &group_leader, const std::string &pet, long long amount);

// A Complete Heal announced by a cleric's macro ("<n> CH - <target> (<mana %>)").
//
// Each CH is a span of fixed 10s - the cast time - starting at the announcement, so the end is known
// the moment it begins and no timer is needed. Casts on the same target become children of one chain
// span, which is the whole point: on a timeline you can see immediately whether the chain overlaps
// cleanly or leaves the tank uncovered between heals.
void RecordCompleteHeal(const std::string &caster, const std::string &target, const std::string &zone,
                        int chain_position, int caster_mana_percent);

// The "GO - <name>" cue at the end of a cleric's macro, telling the next cleric to start. Recording
// when it was said lets the next announcement be measured against it: how long someone took to
// answer their cue is the earliest sign a chain is drifting.
void NoteChainHandoff(const std::string &next_caster);

// Closes chains that have gone quiet, so a chain span does not stay open all night. Called from the
// game-thread tick.
void SweepCompleteHealChains();

// A finished encounter, recorded when the fight closes.
//
// `duration_s` is the whole fight; `active` is per attacker - the seconds each one was actually
// dealing damage. Damage divided by one gives SDPS, by the other DPS, which are the two rates the
// community's parser prints side by side and the two the guild argues about.
void RecordFightTotals(const std::string &target, const std::string &zone, double duration_s,
                       const std::vector<std::pair<std::string, double>> &active_seconds_by_source);

// Healing delivered or received.
void RecordHeal(const std::string &source, const std::string &direction, const std::string &zone,
                const std::string &group_leader, long long amount);

}  // namespace zeal::instrumentation

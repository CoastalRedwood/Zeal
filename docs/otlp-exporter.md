# OTLP exporter

Zeal can export telemetry directly over **OTLP/HTTP (JSON)** — metrics, traces, and logs — instead
of writing to the named pipe and relying on a sidecar app to read it. The exporter lives in
`Zeal/otlp_exporter.{h,cpp}` and is off unless you turn it on.

```
/otlp on                      # start exporting (persists across sessions)
/otlp off
/otlp status                  # endpoint, flush interval, scope, posts sent/failed, last error
/otlp endpoint <url>          # default http://127.0.0.1:4318
/otlp flush <milliseconds>    # default 2000
/otlp scope self|all          # default self
```

Settings persist in the Zeal ini (`OtlpEnabled`, `OtlpEndpoint`, `OtlpFlushMs`, `OtlpMaxBatch`,
`OtlpCombatScope`).

## Design

A worker thread owns a queue; the game thread only ever appends to it and samples state into a
snapshot. Payloads are posted with WinHTTP (`https` supported), so nothing blocks the render loop
and no game structure is read off-thread.

Names come from [everquest-semconv](https://github.com/jensholdgaard/everquest-semconv) via
Weaver-generated constants in `Zeal/everquest_semconv.h` — a typo becomes a compile error rather
than a silently split timeseries. The JSON is hand-built with nlohmann rather than the OpenTelemetry
C++ SDK; see [docs/decisions/otlp-sdk-vs-hand-rolled.md](decisions/otlp-sdk-vs-hand-rolled.md) for
why. `ci/otlp_validate.py` validates the payload fixtures against the official `opentelemetry-proto`
schema on every build, which is what keeps a hand-rolled encoder honest.

## What it emits

**Metrics** — `everquest.combat.damage` and `.heal` (cumulative counters), `everquest.character.attack`
and `.haste` (gauges), and `everquest.group.member` (a non-monotonic sum: the group roster, one point
of value 1 per member).

**Traces** — a `zone session` span parenting a `fight` span per encounter. A fight opens on first
damage and closes on a kill message, 30s of no damage, or zoning.

**Logs** — chat lines, only if you route them somewhere. The reference client config drops them
locally and never uploads them.

## Combat scope

`scope self` (the default) records only damage dealt by you and your pet. This is the correct
setting when a whole guild reports: each player is the authority on their own output, and nothing
is double-counted. `scope all` records every attacker visible in the log, which is useful solo but
double-counts as soon as two people in the same group report.

A consequence worth knowing: under `scope self`, *incoming* damage from a mob is filtered out
(its source is the mob, not you), so damage-taken panels stay empty unless someone runs `scope all`.

## Damage classification

`everquest.combat.damage.type` is the melee skill (`slash`, `crush`, `pierce`, …), `spell`, `dot`,
or `damage_shield`.

Damage shields need an explicit test because the server puts a `DmgShieldType` where a `SkillType`
normally goes:

```cpp
// EQMacEmu zone/attack.cpp, Mob::DamageShield()
cds->type    = spellbonuses.DamageShieldType;   // DS_DECAY(244) .. DS_THORNS(249)
cds->spellid = spellid;                         // the DS spell, or SPELL_UNKNOWN
```

so the 244–249 test runs *before* the spell-id check — a buff-granted shield carries a spell id and
would otherwise be indistinguishable from a nuke, while item and innate shields carry none and fall
through into `melee`. Chat text cannot substitute: the client's damage shield strings (`12133 YOU
are pierced by thorns!`, `12134 %1 was pierced by thorns.`) carry no number at all.

This range is read from EQMacEmu and has **not** been confirmed against a live Quarm server. Check
that the `damage_shield` value actually appears before trusting it.

## Groups

`everquest.group.leader` is attached to each damage point at record time, not stamped on at export,
so damage stays attributed to the group it was dealt in even if the player regroups before the next
flush. It is omitted entirely when solo, so ungrouped play drops out of group-scoped queries.

The leader identifies the group in preference to a hash of the roster: a roster identifier changes
the moment anyone joins or leaves, splitting a timeseries mid-fight. Raids are not modelled — each
of a raid's groups reports its own leader.

`everquest.group.member` reports the roster from `GroupInfo`, including members who do not run Zeal
and therefore emit nothing else. That is deliberate — it is what lets a dashboard distinguish "the
group did poorly" from "half the group isn't reporting" — but it does send other players' character
names, which client-facing documentation should disclose.

## Privacy

The exporter can emit every chat line as a log record, which includes tells, guild, and officer
chat. Point it at a local collector that drops the logs pipeline (as the reference client config
does) unless you have a specific reason not to, and be deliberate about where the endpoint points.

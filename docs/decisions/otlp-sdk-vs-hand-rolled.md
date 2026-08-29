# Why Zeal's OTLP exporter is hand-rolled and not opentelemetry-cpp

**Status:** decided (2026-08-01) — hand-rolled, with spec validation in CI.
**Evaluated on:** branch `otlp-sdk` (a complete, working SDK implementation) and the
`spike-otel-cpp` / `build-sdk` workflows.

## The question

The exporter builds OTLP/JSON by hand with `nlohmann::json` and posts it with WinHTTP. Should it use
the official `opentelemetry-cpp` SDK instead? The SDK would give spec-correct serialization, proper
instruments, batching, retry and cardinality limits for free — and a hand-rolled serializer had
already shipped one real bug (a single attribute emitted as an object instead of a one-element
array, which invalidated *every* metrics payload whenever a zone name was attached).

So this was evaluated properly rather than argued from taste.

## What was measured

`opentelemetry-cpp[otlp-http]` **does** build for Zeal's exact target (`x86-windows-static`, i.e.
32-bit with the static CRT). It is not impossible, only expensive:

| | |
|---|---|
| vcpkg build time | **29 min** (protobuf + abseil + curl from source) |
| static libs produced | **122 files, 667 MB** unlinked (`libprotobuf.lib` alone is 194 MB) |
| transitive deps | protobuf, abseil, **libcurl**, zlib |

A full SDK-backed implementation was written (`Zeal/otlp_exporter_sdk.cpp`): same `/otlp` commands,
same metric names and `eq.*` attributes, OTLP HTTP metric + log exporters, a periodic reader, counters
for damage/heal and observable gauges for attack/haste. It compiles.

## What stopped it

Integration failures, in order — the first three were ours to fix, the last one is structural:

1. A project-wide `NOMINMAX` broke 164 existing call sites (Zeal uses the Windows `min`/`max` macros).
2. `ObserverResult` passed to a `void*` trampoline — our glue.
3. `InitSecurityInterfaceW` / `if_nametoindex` unresolved → **libcurl** needs `secur32` + `iphlpapi`.
4. **`zs.lib(inflate.c.obj)`: `_inflate` already defined in `d3dx8.lib(inflate.obj)`.**

That last one is the finding. **libcurl pulls in zlib, and zlib is already statically baked into
`d3dx8.lib`** — the 2003-era DirectX 8 library the game links against. Two copies of the same C
library in one binary. The ways through looked bad: `/FORCE:MULTIPLE` (the linker silently picks one
zlib; curl may then call 2003 code through modern headers — memory corruption risk inside a DLL
injected into the game) or permanently maintaining a custom vcpkg feature set that builds curl
without zlib, surviving every upstream Zeal sync.

> **Correction (2026-08-05).** That last sentence was wrong, and the conclusion it supported was
> reached too quickly. curl is *optional*, and so is the zlib it drags in. `spike/otel-winhttp`
> builds opentelemetry-cpp v1.28.0 for **x86** with `WITH_HTTP_CLIENT_CURL=OFF` and a WinHTTP
> transport supplied instead, then delivers a real OTLP payload end to end:
>
> ```
> WITH_HTTP_CLIENT_CURL:BOOL=OFF
> installed libs: 24 · no curl libraries in the install tree · zlib is absent
> PASS: OTLP payload delivered over WinHTTP, HTTP 200
> ```
>
> Two things made this look impossible at the time. The option is **recent**: on v1.24.0 a plain
> `set(WITH_HTTP_CLIENT_CURL ON)` overwrites anything passed on the command line, so curl is
> mandatory and `-D` is silently ignored. And zlib enters *only* through curl — it is gated behind
> `WITH_HTTP_CLIENT_CURL AND WITH_OTLP_HTTP_COMPRESSION` — so removing curl removes the collision
> outright, with no `/FORCE:MULTIPLE` and no vcpkg patching.
>
> This does not reverse the decision below: protobuf and abseil still roughly double the `.asi`, and
> that remains the deciding cost for a binary guildmates download. What it does change is the
> *reason*. The SDK was rejected here for size and dependency weight, not because its HTTP layer
> cannot be replaced — it can, through an extension point upstream provides deliberately
> ("set OFF to supply a custom transport").

Note that problems 3 and 4 both come from **libcurl**, which exists only because the SDK's OTLP HTTP
exporter uses it. The hand-rolled path uses **WinHTTP** — already in the OS, no dependency tail, and
already proven under Wine, where this client actually runs.

## Decision

Keep the hand-rolled exporter. Additionally:

- **`ci/otlp_validate.py`** validates our payload shapes against the official `opentelemetry-proto`
  schema — the same definitions the collector uses — on every build. The bug that motivated this
  evaluation is rejected offline, instantly, with an exact field path.
- The same check enforces **semantic invariants** the schema can't express (trace/span id widths and
  non-zero-ness, `endTime >= startTime`, cumulative sums carrying `startTimeUnixNano`).
- Planned: generate `eq.*` name constants from the semconv registry with **OTel Weaver**, so a typo'd
  attribute becomes a compile error rather than a silently split timeseries.

Together these recover most of what the SDK offered, with no new dependencies.

## Secondary consideration

This repo is a fork that stays mergeable with upstream `coastalredwood/Zeal`. A PR adding vcpkg,
protobuf, abseil and libcurl to a lean injected `.asi` — roughly doubling the binary users download —
is unlikely to be accepted, and reasonably so. ~600 self-contained lines using the already-vendored
`json.hpp` is a plausible contribution.

## If this is ever revisited

The SDK branch is preserved. Revisit if: histograms or exemplars are needed (hand-rolling those is
genuinely hairy), several more serialization bugs slip past the CI validation, or upstream adopts a
package manager and the dependency tree stops being a fork-local cost.

## Measurement (2026-08-05): the SDK does link, over WinHTTP, for +1.2 MB

The Correction above argued curl was never mandatory. That has now been built rather than reasoned
about, in two steps:

1. `spike/winhttp-client` — a WinHTTP implementation of the SDK's `HttpClient`/`Session`/`Request`
   abstractions, built against opentelemetry-cpp v1.28.0 with `-DWITH_HTTP_CLIENT_CURL=OFF`. CI
   (`otel-winhttp-spike.yml`) delivers a real payload to a local listener on **x64 and x86**.
2. `spike/zeal-sdk` — that SDK linked into `Zeal.asi` itself: 32-bit, `/MT`, alongside `d3dx8.lib`,
   with a `MeterProvider` and one counter reachable from `/otlp sdkprobe`.

Result — a valid PE32 i386 DLL:

| | shipping (hand-rolled) | with the SDK |
|---|---|---|
| `Zeal.asi` | ~11.2 MB | **12.86 MB** |
| imports | — | `WINHTTP.dll`, no libcurl |
| static archives linked | — | 119 (24 OpenTelemetry, 95 abseil/protobuf) |

The first measurement read 12.38 MB, but the probe had no call site then, so the linker discarded
most of what it had just linked. With `/otlp sdkprobe` actually reachable the figure is 12.86 MB -
**+1.66 MB**. Measure a dependency with its entry point live, or the linker flatters it.

So the two strongest arguments in "Problems with the SDK path" are now **measured as wrong**: the
dependency tail does not require libcurl, and it does not double the download. `+1.2 MB` is what the
linker actually keeps. (The `zlib` strings in the binary are Zeal's own vendored `miniz.c`, present
in the shipping build too — not a dependency the SDK dragged in.)

### What this does *not* settle

- **The probe is a single counter, not a port.** It is reachable from `/otlp sdkprobe`. The hand-rolled exporter's real work — the parsing,
  the semconv attributes, the fight/group/raid model — is untouched and still running.
- **The WinHTTP adapter is unexercised** on cancellation, timeouts, TLS failure and concurrent
  sessions. It moves one payload on a happy path. That gap is the blocker for contributing it
  upstream, where those paths are not optional.
- **Build cost is real**: ~25 minutes for the SDK, and 119 archives is a meaningful step up in what
  a contributor must set up to build this fork.

The decision to keep the hand-rolled exporter stands — but it now rests on scope and build cost,
which are true, rather than on binary size and libcurl, which were not.

## Correction (2026-08-06): what the first spike actually proved, and the crash it hid

The section above claimed the SDK works over WinHTTP. It did not prove that. The standalone test
drove `winhttp_client::HttpClient` **directly** and never went near the path the SDK itself uses, so
it validated the transport while leaving the SDK's route to that transport completely untested.

Loaded into the game, `/otlp sdkprobe` killed the client instantly - no crash handler, no minidump.
The cause was not Wine, threading, or the transport:

    No default HTTP client backend is compiled in. Use the HttpClientFactory or
    HttpClient constructor overloads, or enable curl support (WITH_HTTP_CLIENT_CURL=ON).

`OtlpHttpMetricExporterFactory::Create(options)` reaches for the SDK's *default* HTTP backend, which
with `WITH_HTTP_CLIENT_CURL=OFF` is not compiled in - and the SDK's response is to terminate the
process. Standalone that is an error line and exit 9. Inside a game client it is the player losing
their session with nothing written down. Supplying a custom transport requires the injection
constructor, `OtlpHttpMetricExporter(options, http_client)`; switching curl off and implementing
`HttpClientFactory` is not sufficient on its own.

With the transport injected, verified end to end under Wine against a live collector:

    periodic mode: 12 seconds, exporting every 2s ... PASS (~6 exports)
    everquest_sdk_probe_total{service_version="sdk-probe-periodic"} = 12   # in Prometheus

### The testing lesson, which outlived the bug

A passing test that bypasses the integration point is worse than no test: it converts an unknown
into false confidence. The single-shot test passed under Wine on the same machine, minutes after the
game died on the same code - because it exercised the component and not the wiring. Two `terminate`
paths were found and fixed while reasoning from the symptom; both were real, neither was the cause.

Anything claiming the SDK path works must drive `MeterProvider` -> reader -> exporter, the way the
game does, for more than one export cycle.

## The constraint that actually shapes an SDK port: cardinality and temporality

Checked against the spec, not assumed. Two SDK behaviours interact badly with this workload:

**A Resource is immutable and bound at MeterProvider creation.** The hand-rolled exporter rebuilds
resource attributes on every payload - free when writing the JSON yourself. This pipeline carries
the character in `service.instance.id` (Prometheus promotes it to `instance`), so camping to
character select is a *different service instance*: the provider must be rebuilt, not mutated.

**Under cumulative temporality the SDK never reclaims aggregation state.** Every attribute set ever
observed stays resident and is re-exported every cycle:

> With cumulative temporality, the SDK retains state across cycles, so once the limit is reached,
> new combinations keep overflowing until the process restarts.

The C++ SDK caps this at **2000 attribute sets** (`kAggregationCardinalityLimit`, hard-coded in
`sdk/metrics/state/attributes_hashmap.h`; v1.28's View API exposes no override) and folds everything
past it into `otel.metric.overflow=true`. That drops the whole attribute set, including `target` and
`source`. The damage metric is keyed on a per-mob `target`, so a long raid night can plausibly reach
2000 - after which target-filtered panels silently undercount while totals still look correct.

`kSeriesIdleMs` eviction in the hand-rolled exporter is therefore not housekeeping. It is what keeps
a session bounded, and the SDK has no cumulative-mode equivalent.

### Resolution

Export **delta** temporality from the SDK, and convert in the local collector:

```yaml
processors:
  deltatocumulative:
    max_stale: 5m
```

Delta lets the SDK reset state each cycle, so the 2000 limit bounds one cycle rather than the
session; `max_stale` performs the idle eviction, in a component designed for it. `deltatocumulative`
already ships in the collector build the guild runs, so this is configuration, not a redeploy.

This is the clearest example of the spike's general lesson: the SDK is not a drop-in for a
hand-rolled exporter whose behaviour was tuned to this workload. Each such behaviour has to be
re-derived from the SDK's model, and some - like idle eviction - move to a different component.

## Temporality: cumulative, and the spec says so

Briefly changed to delta to bound the SDK's retained state, then reverted after live combat. Both
the failure and the guidance are worth recording, because the reasoning for delta was superficially
good.

**What broke.** Under delta the SDK only exports attribute sets with activity in the cycle, so a mob
killed inside one 10s window produces a single data point. `rate()` needs two points in its window to
return anything, so those fights contributed **zero** DPS - while totals and the dashboard looked
healthy. Observed directly: several series with `samples=1` over ten minutes.

**What the docs say** - all of which predates the experiment:

- OTLP Metrics Exporter (Stable): "MUST set temporality preference to Cumulative for all instrument
  kinds by default."
- `OTEL_EXPORTER_OTLP_METRICS_TEMPORALITY_PREFERENCE` defaults to `cumulative`.
- Prometheus compatibility: "Prometheus metrics are always cumulative ... the Prometheus exporter
  enforces cumulative for all instruments."
- Prometheus Exporter spec (Stable): "MUST set the MetricReader `temporality` ... to be `cumulative`
  for all instrument kinds."

And the data model names the cost that motivated the change in the first place:

> Cumulative data requires the sender to remember all previous measurements, an "up-front" memory
> cost proportional to cardinality.

So the retained state is the documented, accepted price of cumulative - not a defect to engineer
around. Delta plus a collector-side `deltatocumulative` remains a legitimate pattern for genuinely
large cardinality, but it is the deviation, and it needs the sample-density consequence understood
before it is chosen.

**The general lesson.** The hand-rolled exporter already did this correctly. The change was made from
reasoning rather than from the guidance, and the guidance was one search away. Check the spec before
"improving" a behaviour that already works - especially when the improvement targets a cost the spec
explicitly names as expected.

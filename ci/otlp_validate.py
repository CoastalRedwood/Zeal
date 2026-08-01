#!/usr/bin/env python3
"""Validate OTLP/JSON payloads against the OFFICIAL OpenTelemetry protobuf schema.

The hand-rolled exporter builds OTLP JSON by hand, so the spec is the contract. This checks our
payload shapes against opentelemetry-proto itself — the same definitions the collector uses — which
catches the whole class of "valid JSON, invalid OTLP" bugs (e.g. a single attribute serialized as
an object instead of a one-element array) *before* they ship, offline and in milliseconds.

    pip install opentelemetry-proto
    python ci/otlp_validate.py ci/fixtures/*.json        # or any captured payload

File names decide the signal: *metrics*.json, *logs*.json, *traces*.json.
"""
import glob
import sys

from google.protobuf import json_format
from opentelemetry.proto.collector.logs.v1 import logs_service_pb2
from opentelemetry.proto.collector.metrics.v1 import metrics_service_pb2
from opentelemetry.proto.collector.trace.v1 import trace_service_pb2

SIGNALS = {
    "metrics": metrics_service_pb2.ExportMetricsServiceRequest,
    "logs": logs_service_pb2.ExportLogsServiceRequest,
    "traces": trace_service_pb2.ExportTraceServiceRequest,
}


def signal_for(path: str):
    for name, msg in SIGNALS.items():
        if name in path:
            return name, msg
    return None, None


def semantic_checks(name, path, raw):
    """Invariants the protobuf schema accepts but real backends reject."""
    import json
    problems = []
    d = json.loads(raw)
    if name == "traces":
        for rs in d.get("resourceSpans", []):
            for ss in rs.get("scopeSpans", []):
                for sp in ss.get("spans", []):
                    tid, sid = sp.get("traceId", ""), sp.get("spanId", "")
                    if len(tid) != 32 or set(tid) == {"0"}:
                        problems.append(f"span {sp.get('name')!r}: traceId must be 32 non-zero hex chars, got {len(tid)}")
                    if len(sid) != 16 or set(sid) == {"0"}:
                        problems.append(f"span {sp.get('name')!r}: spanId must be 16 non-zero hex chars, got {len(sid)}")
                    psid = sp.get("parentSpanId", "")
                    if psid and len(psid) != 16:
                        problems.append(f"span {sp.get('name')!r}: parentSpanId must be 16 hex chars, got {len(psid)}")
                    if int(sp.get("endTimeUnixNano", 0)) < int(sp.get("startTimeUnixNano", 0)):
                        problems.append(f"span {sp.get('name')!r}: endTime precedes startTime")
    if name == "metrics":
        for rm in d.get("resourceMetrics", []):
            for sm in rm.get("scopeMetrics", []):
                for m in sm.get("metrics", []):
                    if "sum" in m and m["sum"].get("aggregationTemporality") not in (1, 2):
                        problems.append(f"metric {m.get('name')!r}: sum needs aggregationTemporality 1 or 2")
                    for dp in m.get("sum", {}).get("dataPoints", []):
                        if "startTimeUnixNano" not in dp:
                            problems.append(f"metric {m.get('name')!r}: cumulative sum needs startTimeUnixNano")
    return problems


def main(paths):
    files = [f for p in paths for f in (glob.glob(p) or [p])]
    if not files:
        print("no payloads to validate", file=sys.stderr)
        return 1
    failures = 0
    for path in sorted(files):
        name, msg = signal_for(path)
        if not msg:
            print(f"SKIP  {path} (name must contain metrics/logs/traces)")
            continue
        try:
            raw = open(path).read()
            json_format.Parse(raw, msg())
            issues = semantic_checks(name, path, raw)
            if issues:
                failures += 1
                print(f"FAIL  {path} ({name}) — schema ok, but:")
                for i in issues:
                    print(f"      {i}")
            else:
                print(f"OK    {path} ({name})")
        except Exception as exc:  # noqa: BLE001 - report any schema violation verbatim
            failures += 1
            print(f"FAIL  {path} ({name})\n      {str(exc).splitlines()[0]}")
    print(f"\n{len(files) - failures}/{len(files)} payloads valid per official OTLP schema")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:] or ["ci/fixtures/*.json"]))

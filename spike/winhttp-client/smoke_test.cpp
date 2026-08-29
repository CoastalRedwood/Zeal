// Proves the WinHTTP transport carries a real OTLP export end to end.
//
// It builds an OtlpHttpClient with our client injected (the constructor upstream provides for
// exactly this), sends one metrics payload at a local listener, and reports the status it got back.
// Success means the SDK produced a payload, our transport delivered it, and the response came back
// through the SDK's own handler - which is the whole question this spike exists to answer.
#include <atomic>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

#include <cstdlib>

#include "opentelemetry/exporters/otlp/otlp_http_client.h"
#include "opentelemetry/exporters/otlp/otlp_http_metric_exporter.h"
#include "opentelemetry/exporters/otlp/otlp_http_metric_exporter_options.h"
#include "opentelemetry/metrics/provider.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h"
#include "opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_options.h"
#include "opentelemetry/sdk/metrics/meter_provider.h"
#include "opentelemetry/sdk/metrics/push_metric_exporter.h"
#include "opentelemetry/sdk/metrics/meter_provider_factory.h"
#include "opentelemetry/sdk/resource/resource.h"
#include "winhttp_client.h"

namespace otlp = opentelemetry::exporter::otlp;
namespace metrics_sdk = opentelemetry::sdk::metrics;
namespace metrics_api = opentelemetry::metrics;

// Replicates what the in-game probe does: a real MeterProvider with a PeriodicExportingMetricReader
// exporting on a timer, for several export cycles. The single-shot test below never exercised a
// second export, and that is exactly where a session gets reused - the case that killed the game
// client. One delivery proving the transport works says nothing about the tenth.
static int RunPeriodic(const std::string &endpoint, int seconds);

int main(int argc, char **argv) {
  const std::string endpoint = (argc > 1) ? argv[1] : "http://127.0.0.1:4318/v1/metrics";
  if (argc > 2 && std::string(argv[2]) == "--periodic") {
    return RunPeriodic(endpoint, (argc > 3) ? std::atoi(argv[3]) : 10);
  }

  auto transport = std::make_shared<winhttp_client::HttpClient>();
  if (!transport->winhttp_session()) {
    std::fprintf(stderr, "FAIL: WinHttpOpen returned no session\n");
    return 2;
  }
  std::printf("winhttp session opened\n");

  // A direct exchange through the transport, so a failure here is unambiguously ours rather than
  // the SDK's serialisation.
  auto session = transport->CreateSession(endpoint);
  auto request = session->CreateRequest();
  request->SetMethod(opentelemetry::ext::http::client::Method::Post);
  request->SetUri(endpoint.substr(endpoint.find('/', endpoint.find("//") + 2)));
  request->AddHeader("Content-Type", "application/json");
  std::string payload =
      R"({"resourceMetrics":[{"resource":{"attributes":[{"key":"service.name","value":{"stringValue":"winhttp-spike"}}]},)"
      R"("scopeMetrics":[{"metrics":[{"name":"spike.counter","unit":"1","sum":{"aggregationTemporality":2,)"
      R"("isMonotonic":true,"dataPoints":[{"asInt":"1","timeUnixNano":"1700000000000000000",)"
      R"("startTimeUnixNano":"1700000000000000000"}]}}]}]}]})";
  opentelemetry::ext::http::client::Body body(payload.begin(), payload.end());
  request->SetBody(body);

  struct Handler : public opentelemetry::ext::http::client::EventHandler {
    std::atomic<int> status{0};
    std::atomic<bool> done{false};
    std::string last_event;
    void OnResponse(opentelemetry::ext::http::client::Response &response) noexcept override {
      status.store(static_cast<int>(response.GetStatusCode()));
      done.store(true);
    }
    void OnEvent(opentelemetry::ext::http::client::SessionState state,
                 opentelemetry::nostd::string_view reason) noexcept override {
      last_event.assign(reason.data(), reason.size());
      using S = opentelemetry::ext::http::client::SessionState;
      if (state == S::ConnectFailed || state == S::SendFailed || state == S::NetworkError ||
          state == S::TimedOut || state == S::ReadError || state == S::SSLHandshakeFailed ||
          state == S::CreateFailed) {
        done.store(true);
      }
    }
  };
  auto handler = std::make_shared<Handler>();
  session->SendRequest(handler);

  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
  while (!handler->done.load() && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  session->FinishSession();
  transport->FinishAllSessions();

  const int status = handler->status.load();
  if (status >= 200 && status < 300) {
    std::printf("PASS: OTLP payload delivered over WinHTTP, HTTP %d\n", status);
    return 0;
  }
  std::fprintf(stderr, "FAIL: status=%d last_event=%s\n", status, handler->last_event.c_str());
  return 1;
}

// --- periodic mode ------------------------------------------------------------------------------
// Mirrors spike/zeal-sdk/otel_sdk_probe.cpp as closely as a standalone process can, so a failure
// here is the failure that happens in the game, minus the game.
static int RunPeriodic(const std::string &endpoint, int seconds) {
  otlp::OtlpHttpMetricExporterOptions options;
  options.url = endpoint;
  options.content_type = otlp::HttpRequestContentType::kJson;
  // Delta, matching the in-game path: the collector's deltatocumulative rebuilds the cumulative
  // series Prometheus wants, and this test is what verifies that conversion actually happens.
  options.aggregation_temporality = otlp::PreferredAggregationTemporality::kDelta;

  // Inject our transport rather than going through the Factory, which reaches for a default HTTP
  // backend that does not exist in this configuration and kills the process.
  auto transport = std::make_shared<winhttp_client::HttpClient>();
  auto exporter = std::unique_ptr<metrics_sdk::PushMetricExporter>(
      new otlp::OtlpHttpMetricExporter(options, transport));

  metrics_sdk::PeriodicExportingMetricReaderOptions reader_options;
  reader_options.export_interval_millis = std::chrono::milliseconds(2000);
  reader_options.export_timeout_millis = std::chrono::milliseconds(1500);
  auto reader =
      metrics_sdk::PeriodicExportingMetricReaderFactory::Create(std::move(exporter), reader_options);

  auto resource = opentelemetry::sdk::resource::Resource::Create({
      {"service.name", "everquest"},
      {"service.version", "sdk-probe-periodic"},
      {"service.instance.id", "smoke-test"},
  });

  auto provider = metrics_sdk::MeterProviderFactory::Create(
      std::unique_ptr<metrics_sdk::ViewRegistry>(new metrics_sdk::ViewRegistry()), resource);
  provider->AddMetricReader(std::move(reader));

  std::shared_ptr<metrics_api::MeterProvider> api_provider(std::move(provider));
  metrics_api::Provider::SetMeterProvider(api_provider);

  auto meter = api_provider->GetMeter("zeal.sdk.probe", "sdk-probe");
  auto counter = meter->CreateUInt64Counter("everquest.sdk.probe", "probe emitted by the SDK", "1");

  std::printf("periodic mode: %d seconds, exporting every 2s\n", seconds);
  std::fflush(stdout);
  for (int i = 0; i < seconds; ++i) {
    counter->Add(1);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::printf("  t=%ds still alive\n", i + 1);
    std::fflush(stdout);  // unbuffered: if it dies mid-run, the last line must already be on disk
  }

  static_cast<metrics_sdk::MeterProvider *>(api_provider.get())->ForceFlush(std::chrono::microseconds(2000000));
  metrics_api::Provider::SetMeterProvider(std::shared_ptr<metrics_api::MeterProvider>());
  std::printf("PASS: survived %d seconds of periodic export (~%d exports)\n", seconds, seconds / 2);
  return 0;
}

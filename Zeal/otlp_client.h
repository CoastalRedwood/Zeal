#pragma once
#include <Windows.h>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// Transport half of the OTLP/HTTP JSON exporter: a worker thread, a flush timer, WinHTTP delivery,
// and the send statistics. It knows nothing about EverQuest, or about what any payload contains -
// callers hand it finished bodies and it delivers them.
//
// This is the split the OpenTelemetry client design principles ask for even when hand-rolling:
// "The SDK must be clearly separated into wire protocol-independent parts that implement common
// logic (e.g. batching, tag enrichment by process information, etc.) and protocol-dependent
// telemetry exporters." The instrumentation side (which hooks the game and decides what a hit
// means) has no business owning a socket.
class OtlpClient {
 public:
  // One payload to deliver: the signal's path ("/v1/metrics") and its JSON body.
  using Payload = std::pair<std::string, std::string>;
  // Called on the worker thread once per flush interval; returns whatever is ready to send.
  using CollectFn = std::function<std::vector<Payload>()>;

  explicit OtlpClient(CollectFn collect);
  ~OtlpClient();

  OtlpClient(const OtlpClient &) = delete;
  OtlpClient &operator=(const OtlpClient &) = delete;

  // Configuration, all safe to call from the game thread while the worker runs.
  void set_endpoint(const std::string &endpoint);
  void set_flush_ms(int ms);
  void set_enabled(bool enabled);
  // Bearer token sent with every payload, or empty for an unauthenticated
  // endpoint. The transport is handed the token already in the clear; how it
  // is stored at rest is the exporter's problem, not the socket's.
  void set_auth_token(const std::string &token);

  // Wakes the worker early (e.g. a queue crossed its batch threshold, or /otlp flush was used).
  void nudge();

  // POSTs one payload synchronously. Public so a caller can deliver something off-cycle; the worker
  // uses the same path.
  bool post(const std::string &path, const std::string &json_body);

  unsigned long long posted() const { return posted_count.load(); }
  unsigned long long failed() const { return failed_count.load(); }
  int last_status() const { return last_http_status.load(); }
  std::string last_error() const;

  static constexpr int kMinFlushMs = 100;  // Floor: metrics are periodic snapshots, 0 would spin.

 private:
  void worker_loop();

  CollectFn collect;
  std::thread worker;
  mutable std::mutex mutex;
  std::condition_variable cv;
  bool end_thread = false;

  std::string endpoint = "http://127.0.0.1:4318";
  std::string auth_token;  // guarded by `mutex`, like `endpoint`
  std::atomic<int> flush_ms{2000};
  std::atomic<bool> enabled{false};

  std::atomic<unsigned long long> posted_count{0};
  std::atomic<unsigned long long> failed_count{0};
  std::atomic<int> last_http_status{0};
  mutable std::mutex error_mutex;
  std::string last_error_text;  // response body of the most recent failure, for `/otlp status`
};

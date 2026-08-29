// A WinHTTP transport for opentelemetry-cpp, supplied in place of the curl one.
//
// Upstream supports this directly - the CMake option says so:
//
//   WITH_HTTP_CLIENT_CURL  "Use the curl HTTP client backend. Defaults to ON when any HTTP
//                           exporter is enabled; set OFF to supply a custom transport."
//
// Why bother: curl is the only transport shipped, and on Windows it arrives with a vendored TLS
// stack and zlib. Static zlib is what made the SDK unusable inside this game client - the 2003-era
// d3dx8.lib the game links already contains zlib, and two copies of `_inflate` will not link. More
// generally, a Windows process that must not grow new DLL dependencies (a plugin, an OT/industrial
// agent, an injected library) cannot take curl at all, while WinHTTP is part of the OS: no vendored
// crypto, system proxy and certificate store honoured for free, patched by Windows Update.
//
// The SDK's HttpClient contract is asynchronous: SendRequest() must return promptly and the handler
// is notified later. Each session therefore owns a thread that performs the blocking WinHTTP
// exchange and then calls OnResponse/OnEvent.
#pragma once

#include <windows.h>
#include <winhttp.h>

#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "opentelemetry/ext/http/client/http_client.h"
#include "opentelemetry/ext/http/client/http_client_factory.h"
#include "opentelemetry/version.h"

namespace winhttp_client {

namespace http_client = opentelemetry::ext::http::client;

// Delivery statistics for the whole transport, aggregated across every session.
//
// The SDK reports export failures only through its internal log handler, so without this there is
// no way to ask, from in game, whether telemetry is actually arriving - which is how an
// unauthenticated exporter posted into the void unnoticed. `/otlp status` reads these.
struct Stats {
  unsigned long long posted = 0;  // responses with a 2xx status
  unsigned long long failed = 0;  // everything else, transport errors included
  int last_status = 0;            // most recent HTTP status, 0 if none completed
  std::string last_error;         // most recent failure, with the server's explanation if it gave one
};

Stats GetStats();
// Called by the session as each request finishes; `status` of 0 means it never got a response.
void RecordResult(int status, const std::string &error);

class Request final : public http_client::Request {
 public:
  void SetMethod(http_client::Method method) noexcept override { method_ = method; }
  void SetBody(http_client::Body &body) noexcept override { body_.swap(body); }
  void SetUri(opentelemetry::nostd::string_view uri) noexcept override { uri_.assign(uri.data(), uri.size()); }
  void SetSslOptions(const http_client::HttpSslOptions &options) noexcept override { ssl_options_ = options; }
  void SetTimeoutMs(std::chrono::milliseconds timeout_ms) noexcept override { timeout_ms_ = timeout_ms; }
  void EnableLogging(bool) noexcept override {}
  void SetCompression(const http_client::Compression &compression) noexcept override { compression_ = compression; }
  void SetRetryPolicy(const http_client::RetryPolicy &retry_policy) noexcept override {
    retry_policy_ = retry_policy;
  }

  void AddHeader(opentelemetry::nostd::string_view name, opentelemetry::nostd::string_view value) noexcept override {
    headers_.insert({std::string(name.data(), name.size()), std::string(value.data(), value.size())});
  }
  void ReplaceHeader(opentelemetry::nostd::string_view name,
                     opentelemetry::nostd::string_view value) noexcept override {
    std::string key(name.data(), name.size());
    headers_.erase(key);
    headers_.insert({key, std::string(value.data(), value.size())});
  }

  http_client::Method method_ = http_client::Method::Post;
  http_client::Body body_;
  std::string uri_;
  http_client::Headers headers_;
  http_client::HttpSslOptions ssl_options_;
  http_client::Compression compression_ = http_client::Compression::kNone;
  http_client::RetryPolicy retry_policy_;
  std::chrono::milliseconds timeout_ms_{30000};
};

class Response final : public http_client::Response {
 public:
  const http_client::Body &GetBody() const noexcept override { return body_; }
  http_client::StatusCode GetStatusCode() const noexcept override { return status_code_; }

  bool ForEachHeader(opentelemetry::nostd::function_ref<bool(opentelemetry::nostd::string_view name,
                                                             opentelemetry::nostd::string_view value)> callable)
      const noexcept override {
    for (const auto &header : headers_) {
      if (!callable(header.first, header.second)) return false;
    }
    return true;
  }

  bool ForEachHeader(const opentelemetry::nostd::string_view &name,
                     opentelemetry::nostd::function_ref<bool(opentelemetry::nostd::string_view name,
                                                             opentelemetry::nostd::string_view value)> callable)
      const noexcept override {
    const std::string key(name.data(), name.size());
    auto range = headers_.equal_range(key);
    for (auto it = range.first; it != range.second; ++it) {
      if (!callable(it->first, it->second)) return false;
    }
    return true;
  }

  http_client::Body body_;
  http_client::Headers headers_;
  http_client::StatusCode status_code_ = 0;
};

class HttpClient;

class Session final : public http_client::Session, public std::enable_shared_from_this<Session> {
 public:
  Session(std::weak_ptr<HttpClient> parent, std::string host, uint16_t port, bool https);
  ~Session() override;

  std::shared_ptr<http_client::Request> CreateRequest() noexcept override;
  void SendRequest(std::shared_ptr<http_client::EventHandler> callback) noexcept override;
  bool IsSessionActive() noexcept override { return active_.load(); }
  bool CancelSession() noexcept override;
  bool FinishSession() noexcept override;

 private:
  // Runs on the worker thread: performs the exchange and reports through the handler exactly once.
  void Exchange(std::shared_ptr<http_client::EventHandler> handler);

  std::weak_ptr<HttpClient> parent_;
  std::string host_;
  uint16_t port_;
  bool https_;
  std::shared_ptr<Request> request_;
  std::thread worker_;
  std::atomic<bool> active_{false};
  std::atomic<bool> cancelled_{false};
};

class HttpClient final : public http_client::HttpClient, public std::enable_shared_from_this<HttpClient> {
 public:
  HttpClient();
  ~HttpClient() override;

  std::shared_ptr<http_client::Session> CreateSession(opentelemetry::nostd::string_view url) noexcept override;
  bool CancelAllSessions() noexcept override;
  bool FinishAllSessions() noexcept override;
  void SetMaxSessionsPerConnection(std::size_t) noexcept override {}

  // Shared across sessions: WinHTTP session handles are expensive and thread-safe to reuse.
  HINTERNET winhttp_session() const noexcept { return winhttp_session_; }

  void Track(const std::shared_ptr<Session> &session);
  void Forget(Session *session);

 private:
  HINTERNET winhttp_session_ = nullptr;
  std::mutex mutex_;
  std::vector<std::weak_ptr<Session>> sessions_;
};

// Factory, so the client can be handed to OtlpHttpClient the same way the curl one is.
class HttpClientFactory final : public http_client::HttpClientFactory {
 public:
  std::shared_ptr<http_client::HttpClientSync> CreateSync() override { return nullptr; }
  std::shared_ptr<http_client::HttpClient> Create() override { return std::make_shared<HttpClient>(); }
  std::shared_ptr<http_client::HttpClient> Create(
      const std::shared_ptr<opentelemetry::sdk::common::ThreadInstrumentation> &) override {
    return std::make_shared<HttpClient>();
  }
};

}  // namespace winhttp_client

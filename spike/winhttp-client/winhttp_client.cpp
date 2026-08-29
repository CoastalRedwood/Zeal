#include "winhttp_client.h"

#include <algorithm>
#include <exception>

#pragma comment(lib, "winhttp.lib")

namespace winhttp_client {

namespace {
std::mutex g_stats_mutex;
Stats g_stats;
}  // namespace

Stats GetStats() {
  std::lock_guard<std::mutex> lock(g_stats_mutex);
  return g_stats;
}

void RecordResult(int status, const std::string &error) {
  std::lock_guard<std::mutex> lock(g_stats_mutex);
  g_stats.last_status = status;
  if (status >= 200 && status < 300) {
    g_stats.posted++;
    g_stats.last_error.clear();
  } else {
    g_stats.failed++;
    // A rejected export explains itself in the body (a 401 says which auth failed); keeping it is
    // the difference between "something is wrong" and knowing what.
    g_stats.last_error = error;
  }
}

namespace {

std::wstring Widen(const std::string &s) { return std::wstring(s.begin(), s.end()); }

const wchar_t *MethodName(http_client::Method method) {
  switch (method) {
    case http_client::Method::Get: return L"GET";
    case http_client::Method::Put: return L"PUT";
    case http_client::Method::Head: return L"HEAD";
    case http_client::Method::Delete: return L"DELETE";
    case http_client::Method::Patch: return L"PATCH";
    case http_client::Method::Options: return L"OPTIONS";
    case http_client::Method::Post:
    default: return L"POST";
  }
}

// WinHTTP hands back all response headers as one CRLF-separated block; the SDK wants them keyed.
void ParseHeaders(const std::wstring &raw, http_client::Headers &out) {
  std::string block(raw.begin(), raw.end());
  size_t pos = 0;
  bool first = true;
  while (pos < block.size()) {
    size_t end = block.find("\r\n", pos);
    if (end == std::string::npos) end = block.size();
    const std::string line = block.substr(pos, end - pos);
    pos = end + 2;
    if (first) {  // status line, not a header
      first = false;
      continue;
    }
    const size_t colon = line.find(':');
    if (colon == std::string::npos) continue;
    std::string name = line.substr(0, colon);
    std::string value = line.substr(colon + 1);
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.erase(value.begin());
    out.insert({name, value});
  }
}

}  // namespace

// --- Session ----------------------------------------------------------------------------------

Session::Session(std::weak_ptr<HttpClient> parent, std::string host, uint16_t port, bool https)
    : parent_(std::move(parent)), host_(std::move(host)), port_(port), https_(https) {}

Session::~Session() { FinishSession(); }

std::shared_ptr<http_client::Request> Session::CreateRequest() noexcept {
  request_ = std::make_shared<Request>();
  return request_;
}

void Session::SendRequest(std::shared_ptr<http_client::EventHandler> callback) noexcept {
  if (!request_) {
    callback->OnEvent(http_client::SessionState::CreateFailed, "no request");
    return;
  }
  // Reusing a session for a second request would assign over a joinable std::thread, which calls
  // std::terminate() - the process dies with no SEH exception, so no crash handler sees it and no
  // minidump is written. Retire the previous worker first.
  if (worker_.joinable()) {
    if (worker_.get_id() == std::this_thread::get_id())
      worker_.detach();
    else
      worker_.join();
  }
  active_.store(true);
  // The contract is asynchronous: return promptly, report through the handler later. WinHTTP's own
  // async mode would avoid the thread, but a thread per in-flight export is simpler to get right and
  // the exporter batches, so there are few of them.
  auto self = shared_from_this();
  worker_ = std::thread([self, callback]() {
    // An exception escaping a thread function is also std::terminate, and this one runs inside a
    // game process where that means the player loses their session. Nothing here is worth a crash.
    try {
      self->Exchange(callback);
    } catch (const std::exception &e) {
      self->active_.store(false);
      callback->OnEvent(http_client::SessionState::CreateFailed, e.what());
    } catch (...) {
      self->active_.store(false);
      callback->OnEvent(http_client::SessionState::CreateFailed, "unknown exception in WinHTTP worker");
    }
  });
}

void Session::Exchange(std::shared_ptr<http_client::EventHandler> handler) {
  auto parent = parent_.lock();
  if (!parent || !parent->winhttp_session()) {
    handler->OnEvent(http_client::SessionState::ConnectFailed, "no winhttp session");
    active_.store(false);
    return;
  }

  handler->OnEvent(http_client::SessionState::Connecting, "");
  HINTERNET connect = WinHttpConnect(parent->winhttp_session(), Widen(host_).c_str(), port_, 0);
  if (!connect) {
    handler->OnEvent(http_client::SessionState::ConnectFailed, "WinHttpConnect failed");
    active_.store(false);
    return;
  }
  handler->OnEvent(http_client::SessionState::Connected, "");

  const DWORD flags = https_ ? WINHTTP_FLAG_SECURE : 0;
  HINTERNET request = WinHttpOpenRequest(connect, MethodName(request_->method_), Widen(request_->uri_).c_str(),
                                         nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
  if (!request) {
    WinHttpCloseHandle(connect);
    handler->OnEvent(http_client::SessionState::CreateFailed, "WinHttpOpenRequest failed");
    active_.store(false);
    return;
  }

  const DWORD timeout = static_cast<DWORD>(request_->timeout_ms_.count());
  WinHttpSetTimeouts(request, timeout, timeout, timeout, timeout);

  std::wstring headers;
  for (const auto &header : request_->headers_) {
    headers += Widen(header.first) + L": " + Widen(header.second) + L"\r\n";
  }

  auto fail = [&](http_client::SessionState state, const char *why) {
    RecordResult(0, why ? why : "transport error");
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    handler->OnEvent(state, why);
    active_.store(false);
  };

  handler->OnEvent(http_client::SessionState::Sending, "");
  if (cancelled_.load()) return fail(http_client::SessionState::Cancelled, "cancelled");

  const void *body = request_->body_.empty() ? nullptr : request_->body_.data();
  const DWORD body_len = static_cast<DWORD>(request_->body_.size());
  if (!WinHttpSendRequest(request, headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
                          headers.empty() ? 0 : static_cast<DWORD>(-1), const_cast<void *>(body), body_len, body_len,
                          0)) {
    const DWORD err = GetLastError();
    return fail(err == ERROR_WINHTTP_TIMEOUT ? http_client::SessionState::TimedOut
                                             : http_client::SessionState::SendFailed,
                "WinHttpSendRequest failed");
  }

  if (!WinHttpReceiveResponse(request, nullptr)) {
    const DWORD err = GetLastError();
    return fail(err == ERROR_WINHTTP_TIMEOUT      ? http_client::SessionState::TimedOut
                : err == ERROR_WINHTTP_SECURE_FAILURE ? http_client::SessionState::SSLHandshakeFailed
                                                      : http_client::SessionState::NetworkError,
                "WinHttpReceiveResponse failed");
  }

  auto response = std::unique_ptr<Response>(new Response());

  DWORD status = 0, status_size = sizeof(status);
  WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                      &status, &status_size, WINHTTP_NO_HEADER_INDEX);
  response->status_code_ = static_cast<http_client::StatusCode>(status);

  DWORD header_size = 0;
  WinHttpQueryHeaders(request, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, nullptr, &header_size,
                      WINHTTP_NO_HEADER_INDEX);
  if (header_size > 0 && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
    std::wstring raw(header_size / sizeof(wchar_t), L'\0');
    if (WinHttpQueryHeaders(request, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, &raw[0],
                            &header_size, WINHTTP_NO_HEADER_INDEX)) {
      ParseHeaders(raw, response->headers_);
    }
  }

  for (;;) {
    DWORD available = 0;
    if (!WinHttpQueryDataAvailable(request, &available)) {
      return fail(http_client::SessionState::ReadError, "WinHttpQueryDataAvailable failed");
    }
    if (available == 0) break;
    const size_t offset = response->body_.size();
    response->body_.resize(offset + available);
    DWORD read = 0;
    if (!WinHttpReadData(request, response->body_.data() + offset, available, &read)) {
      return fail(http_client::SessionState::ReadError, "WinHttpReadData failed");
    }
    response->body_.resize(offset + read);
    if (read == 0) break;
  }

  // The exchange completed: record what the server said. A non-2xx keeps up to 300 characters of
  // the body, which is where a gateway names the actual problem.
  {
    const int code = static_cast<int>(response->status_code_);
    std::string why;
    if (code < 200 || code >= 300) {
      why = std::string(response->body_.begin(), response->body_.end());
      if (why.size() > 300) why.resize(300);
      why = std::to_string(code) + " " + why;
    }
    RecordResult(code, why);
  }

  WinHttpCloseHandle(request);
  WinHttpCloseHandle(connect);

  handler->OnEvent(http_client::SessionState::Response, "");
  handler->OnResponse(*response);
  active_.store(false);
}

bool Session::CancelSession() noexcept {
  cancelled_.store(true);
  return FinishSession();
}

bool Session::FinishSession() noexcept {
  if (worker_.joinable()) {
    if (worker_.get_id() == std::this_thread::get_id()) {
      worker_.detach();  // called from the handler on our own thread: joining would deadlock
    } else {
      worker_.join();
    }
  }
  active_.store(false);
  if (auto parent = parent_.lock()) parent->Forget(this);
  return true;
}

// --- HttpClient -------------------------------------------------------------------------------

HttpClient::HttpClient() {
  winhttp_session_ = WinHttpOpen(L"opentelemetry-cpp-winhttp/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                 WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!winhttp_session_) {  // pre-Win8.1 has no automatic proxy detection
    winhttp_session_ = WinHttpOpen(L"opentelemetry-cpp-winhttp/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                   WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  }
}

HttpClient::~HttpClient() {
  FinishAllSessions();
  if (winhttp_session_) WinHttpCloseHandle(winhttp_session_);
}

std::shared_ptr<http_client::Session> HttpClient::CreateSession(opentelemetry::nostd::string_view url) noexcept {
  const std::string full(url.data(), url.size());
  URL_COMPONENTS parts = {};
  parts.dwStructSize = sizeof(parts);
  wchar_t host[256] = {}, path[2048] = {};
  parts.lpszHostName = host;
  parts.dwHostNameLength = _countof(host);
  parts.lpszUrlPath = path;
  parts.dwUrlPathLength = _countof(path);

  const std::wstring wide = Widen(full);
  if (!WinHttpCrackUrl(wide.c_str(), 0, 0, &parts)) {
    return std::make_shared<Session>(weak_from_this(), "", 0, false);
  }
  const std::wstring whost(host);
  auto session = std::make_shared<Session>(weak_from_this(), std::string(whost.begin(), whost.end()), parts.nPort,
                                           parts.nScheme == INTERNET_SCHEME_HTTPS);
  Track(session);
  return session;
}

void HttpClient::Track(const std::shared_ptr<Session> &session) {
  std::lock_guard<std::mutex> lock(mutex_);
  sessions_.push_back(session);
}

void HttpClient::Forget(Session *session) {
  std::lock_guard<std::mutex> lock(mutex_);
  sessions_.erase(std::remove_if(sessions_.begin(), sessions_.end(),
                                 [session](const std::weak_ptr<Session> &weak) {
                                   auto locked = weak.lock();
                                   return !locked || locked.get() == session;
                                 }),
                  sessions_.end());
}

bool HttpClient::CancelAllSessions() noexcept {
  std::vector<std::shared_ptr<Session>> live;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto &weak : sessions_) {
      if (auto session = weak.lock()) live.push_back(session);
    }
  }
  for (auto &session : live) session->CancelSession();
  return true;
}

bool HttpClient::FinishAllSessions() noexcept {
  std::vector<std::shared_ptr<Session>> live;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto &weak : sessions_) {
      if (auto session = weak.lock()) live.push_back(session);
    }
  }
  for (auto &session : live) session->FinishSession();
  return true;
}

}  // namespace winhttp_client

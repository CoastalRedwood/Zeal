#include "otlp_client.h"

#include <winhttp.h>

#include <chrono>

#pragma comment(lib, "winhttp.lib")

OtlpClient::OtlpClient(CollectFn collect_fn) : collect(std::move(collect_fn)) {
  worker = std::thread([this]() { worker_loop(); });
}

OtlpClient::~OtlpClient() {
  {
    std::lock_guard<std::mutex> lock(mutex);
    end_thread = true;
  }
  cv.notify_all();
  if (worker.joinable()) worker.join();
}

void OtlpClient::set_endpoint(const std::string &value) {
  std::lock_guard<std::mutex> lock(mutex);
  endpoint = value;
}

void OtlpClient::set_flush_ms(int ms) { flush_ms.store(ms < kMinFlushMs ? kMinFlushMs : ms); }

void OtlpClient::set_enabled(bool value) { enabled.store(value); }

void OtlpClient::nudge() { cv.notify_all(); }

std::string OtlpClient::last_error() const {
  std::lock_guard<std::mutex> lock(error_mutex);
  return last_error_text;
}

void OtlpClient::worker_loop() {
  while (true) {
    {
      std::unique_lock<std::mutex> lock(mutex);
      cv.wait_for(lock, std::chrono::milliseconds(flush_ms.load()), [this]() { return end_thread; });
      if (end_thread) return;
    }
    if (!enabled.load()) continue;

    try {
      for (const Payload &payload : collect()) {
        if (payload.second.empty()) continue;
        if (post(payload.first, payload.second))
          posted_count++;
        else
          failed_count++;
      }
    } catch (const std::exception &) {
      // Never let telemetry take down the game: drop this cycle and try again on the next one.
    }
  }
}

void OtlpClient::set_auth_token(const std::string &token) {
  std::lock_guard<std::mutex> lock(mutex);
  auth_token = token;
}

bool OtlpClient::post(const std::string &path, const std::string &json_body) {
  std::string url;
  std::string token;
  {
    std::lock_guard<std::mutex> lock(mutex);
    url = endpoint + path;
    token = auth_token;
  }
  std::wstring wurl(url.begin(), url.end());

  URL_COMPONENTS uc = {0};
  uc.dwStructSize = sizeof(uc);
  wchar_t host[256] = {0};
  wchar_t url_path[1024] = {0};
  uc.lpszHostName = host;
  uc.dwHostNameLength = _countof(host);
  uc.lpszUrlPath = url_path;
  uc.dwUrlPathLength = _countof(url_path);
  if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc)) return false;

  bool ok = false;
  HINTERNET session = WinHttpOpen(L"Zeal-OTLP/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                                  WINHTTP_NO_PROXY_BYPASS, 0);
  if (session) {
    // Bound every phase so a slow/unreachable endpoint can't hang the sender thread (and thus the
    // join() on shutdown). Values in ms: resolve, connect, send, receive.
    WinHttpSetTimeouts(session, 2000, 2000, 2000, 3000);
    HINTERNET connect = WinHttpConnect(session, host, uc.nPort, 0);
    if (connect) {
      DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
      HINTERNET request = WinHttpOpenRequest(connect, L"POST", url_path, nullptr, WINHTTP_NO_REFERER,
                                             WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
      if (request) {
        // The gateway authenticates per member, so an unauthenticated POST is
        // a 401 rather than an anonymous accept. An empty token is still a
        // valid state: a local collector wants no Authorization at all.
        std::wstring headers = L"Content-Type: application/json";
        if (!token.empty()) {
          headers += L"\r\nAuthorization: Bearer ";
          headers.append(token.begin(), token.end());  // tokens are hex, so no widening to do
        }
        if (WinHttpSendRequest(request, headers.c_str(), -1L, (LPVOID)json_body.data(),
                               static_cast<DWORD>(json_body.size()), static_cast<DWORD>(json_body.size()), 0) &&
            WinHttpReceiveResponse(request, nullptr)) {
          DWORD status = 0, size = sizeof(status);
          WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                              WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
          last_http_status.store(static_cast<int>(status));
          ok = (status >= 200 && status < 300);
          if (!ok) {
            // Keep the server's explanation - it names the exact problem (e.g. a malformed field),
            // which is otherwise invisible from in-game.
            std::string body;
            char buf[512];
            DWORD read = 0;
            while (body.size() < 400 && WinHttpReadData(request, buf, sizeof(buf), &read) && read > 0)
              body.append(buf, read);
            std::lock_guard<std::mutex> lock(error_mutex);
            last_error_text = path + " -> " + std::to_string(status) + " " + body.substr(0, 300);
          }
        }
        WinHttpCloseHandle(request);
      }
      WinHttpCloseHandle(connect);
    }
    WinHttpCloseHandle(session);
  }
  return ok;
}

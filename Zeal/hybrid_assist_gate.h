#pragma once
// Experimental single-flight assist scheduler, isolated from the game for tests.
// Assist responses have no request ID. In-flight requests cannot be cancelled;
// serialization reduces (but cannot eliminate) late-reply ambiguity.
#include <cstdint>
#include <string>
#include <utility>

class HybridAssistGate {
 public:
  bool pending() const { return pending_; }
  uint16_t pending_source() const { return source_; }
  bool has_deferred_manual() const { return !manual_command_.empty(); }
  bool initial_due() const { return initial_due_; }

  void reset() {
    pending_ = false;
    source_ = 0;
    selected_ = 0;
    initial_due_ = false;
    quiet_since_ = 0;
    sent_at_ = 0;
    last_poll_ = 0;
    last_manual_at_ = 0;
    last_timeout_at_ = 0;
    have_poll_ = false;
    have_manual_ = false;
    have_timeout_ = false;
    manual_command_.clear();
  }

  // True only when the selected target changes. Never discard a transmitted
  // request: its response still belongs to the old source Spawn ID.
  bool selection(uint16_t source, uint32_t now) {
    if (selected_ == source) return false;
    selected_ = source;
    quiet_since_ = now;
    initial_due_ = source != 0;
    // The last target's polling interval must not delay this target's first query.
    have_poll_ = false;
    return true;
  }

  // Fresh evidence for the CURRENT selected target takes priority over UNSENT
  // assists. Every new event restarts the full inactivity interval.
  void observed_passive(uint16_t source, uint32_t now) {
    if (!source || source != selected_) return;
    initial_due_ = false;
    quiet_since_ = now;
  }

  // One initial query per newly selected target unless a genuine event arrived
  // first. If the previous target's request is still in flight, wait for it.
  bool try_start_initial(uint16_t source, uint32_t now) {
    if (!initial_due_ || !source || source != selected_ || pending_ ||
        has_deferred_manual() || in_timeout_cooldown(now)) return false;
    initial_due_ = false;
    start(source, now);
    return true;
  }

  // Once X ms pass with no fresh passive event, fall back to one assist.
  // Continued silence allows one assist per X ms, but never a backlog.
  bool try_start_idle(uint16_t source, uint32_t now, uint32_t interval_ms) {
    if (!source || source != selected_ || pending_ || initial_due_ ||
        has_deferred_manual() || in_timeout_cooldown(now) ||
        now - quiet_since_ < interval_ms ||
        (have_poll_ && now - last_poll_ < interval_ms) ||
        (have_manual_ && now - last_manual_at_ < interval_ms)) return false;
    start(source, now);
    return true;
  }

  // Manual /assist requests take precedence; queue only the most recent while
  // an automatic request is awaiting its reply.
  bool defer_manual(std::string command, uint32_t now) {
    last_manual_at_ = now;
    have_manual_ = true;
    if (!pending_) return false;
    manual_command_ = std::move(command);
    return true;
  }

  bool receive(uint16_t& source) {
    if (!pending_) return false;
    source = source_;
    pending_ = false;
    source_ = 0;
    have_timeout_ = false;
    return true;
  }

  // Recovery rather than permanently disabling the user's fallback setting.
  // A timed-out request may still have a delayed reply, so impose a further
  // quiet period before another automatic request. No protocol request ID exists
  // to guarantee perfect attribution of exceptionally late replies.
  bool check_timeout(uint32_t now, uint32_t timeout_ms = 3500) {
    if (!pending_ || now - sent_at_ < timeout_ms) return false;
    pending_ = false;
    source_ = 0;
    have_timeout_ = true;
    last_timeout_at_ = now;
    return true;
  }

  std::string take_deferred_manual() {
    std::string cmd = std::move(manual_command_);
    manual_command_.clear();
    return cmd;
  }

 private:
  bool in_timeout_cooldown(uint32_t now) const {
    return have_timeout_ && now - last_timeout_at_ < 2000;
  }

  void start(uint16_t source, uint32_t now) {
    pending_ = true;
    source_ = source;
    sent_at_ = now;
    last_poll_ = now;
    have_poll_ = true;
  }

  bool pending_ = false;
  uint16_t source_ = 0;
  uint16_t selected_ = 0;
  bool initial_due_ = false;
  uint32_t quiet_since_ = 0;
  uint32_t sent_at_ = 0;
  uint32_t last_poll_ = 0;
  uint32_t last_manual_at_ = 0;
  uint32_t last_timeout_at_ = 0;
  bool have_poll_ = false;
  bool have_manual_ = false;
  bool have_timeout_ = false;
  std::string manual_command_;
};

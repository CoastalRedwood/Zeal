#include "alarm.h"

#include "zeal.h"

void Alarm::Set(int minutes, int seconds) {
  if (!enabled) {
    enabled = true;
    duration = ((minutes * 60) + seconds) * 1000;
    std::ostringstream oss;
    oss << "[Alarm] SET! (" << minutes << "m" << seconds << "s)" << std::endl;
    Zeal::Game::print_chat(oss.str());
  } else {
    Zeal::Game::print_chat("[Alarm] Please halt the active alarm before attempting to set a new one.");
  }
}

void Alarm::Halt(void) {
  if (enabled) {
    enabled = false;
    Zeal::Game::print_chat("[Alarm] HALT!");
  } else {
    Zeal::Game::print_chat("[Alarm] No active alarm.");
  }
}

void Alarm::callback_main() {
  if (enabled) {
    if (!start_time) start_time = GetTickCount64();

    if (GetTickCount64() - start_time > duration) {
      Zeal::Game::print_chat("[Alarm] COMPLETED!");
      enabled = false;
    }
  } else {
    start_time = 0;
  }
}

Alarm::Alarm(ZealService *zeal) {
  zeal->callbacks->AddGeneric([this]() { callback_main(); });
}

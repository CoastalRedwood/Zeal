#include "experience.h"

#include <algorithm>
#include <string>
#include <vector>

#include "callbacks.h"
#include "commands.h"
#include "game_addresses.h"
#include "game_functions.h"
#include "zeal.h"

ExperienceCalc::ExperienceCalc() {}

void ExperienceCalc::reset(int level, int exp) {
  exp_per_hour_pct = 0;
  exp_queue.clear();
  if (level >= 0) {
    int total_exp = level * kExpPerLevel + exp;
    exp_queue.push_back(ExpData(GetTickCount64(), total_exp));
  }
}

void ExperienceCalc::dump() const {
  int total_exp = exp_queue.empty() ? 0 : exp_queue.back().total_exp;
  Zeal::Game::print_chat("Total exp: %d, rate: %.2f, Count: %d", total_exp, get_exp_per_hour_pct(), exp_queue.size());
  for (const auto &entry : exp_queue) Zeal::Game::print_chat("Entry: %llu, %d", entry.timestamp, entry.total_exp);
}

void ExperienceCalc::update(int level, int exp) {
  if (level < 0) {
    reset(level, exp);  // Wipes the queue and resets output to 0.
    return;
  }

  // Add any changes in experience to the timestamped queue.
  int total_exp = level * kExpPerLevel + exp;
  if (exp_queue.empty() || (total_exp != exp_queue.back().total_exp)) {
    exp_queue.push_back(ExpData(GetTickCount64(), total_exp));
    if (exp_queue.size() > kMaxQueueSize) exp_queue.pop_front();
  }

  if (exp_queue.empty()) {
    exp_per_hour_pct = 0;
    return;
  }

  // Calculate the change in experience and time from the first queue entry to now.
  float delta_exp = (total_exp - exp_queue.front().total_exp) / static_cast<float>(kExpPerLevel);
  float duration_secs = static_cast<float>((GetTickCount64() - exp_queue.front().timestamp)) / 1000.f;
  duration_secs = std::clamp(duration_secs, 10.f, 2 * 60 * 60.f);  // Clamp between 10 sec to 2 hours.
  float exp_per_hour = delta_exp / duration_secs * 60 * 60;
  exp_per_hour_pct = std::clamp(exp_per_hour * 100.f, 0.f, 600.f);  // Clamp 0 to 600%.
}

Experience::Experience(ZealService *zeal) {
  zeal->callbacks->AddGeneric([this]() { callback_main(); }, callback_type::MainLoop);
  zeal->callbacks->AddGeneric([this]() { reset(); }, callback_type::InitUI);
  zeal->callbacks->AddGeneric([this]() { reset(); }, callback_type::CleanUI);

  zeal->commands_hook->Add("/resetexp", {}, "resets exp per hour calculations", [this](std::vector<std::string> &args) {
    if (args.size() == 2 && args[1] == "debug") {
      auto char_info = Zeal::Game::get_char_info();
      if (char_info)
        Zeal::Game::print_chat("Exp: %d, Points: %d, AA exp: %d", char_info->Experience, get_aa_total_points(),
                               char_info->AlternateAdvancementExperience);
      exp_calc.dump();
      aa_calc.dump();
    } else if (args.size() == 2 && args[1] == "ding") {
      setting_aa_ding.set(true);
      Zeal::Game::print_chat("AA level ding sound enabled");
    } else if (args.size() == 2 && args[1] == "off") {
      setting_aa_ding.set(false);
      Zeal::Game::print_chat("AA level ding sound disabled");
    } else {
      reset();
    }
    return true;
  });
}

void Experience::reset() {
  reset_exp();
  reset_aa();
}

void Experience::reset_exp() {
  auto self = Zeal::Game::get_self();
  auto char_info = self ? self->CharInfo : nullptr;
  int level = self ? self->Level : 0;
  int exp = char_info ? char_info->Experience : 0;
  exp_calc.reset(get_exp_level(), exp);
}

void Experience::reset_aa() {
  auto self = Zeal::Game::get_self();
  auto char_info = self ? self->CharInfo : nullptr;
  int exp = char_info ? char_info->AlternateAdvancementExperience : 0;
  last_aa_points = get_aa_total_points();
  aa_calc.reset(last_aa_points, exp);
}

int Experience::get_exp_level() const {
  auto self = Zeal::Game::get_self();
  auto char_info = self ? self->CharInfo : nullptr;
  int level = self ? self->Level : 0;
  int exp = char_info ? char_info->Experience : 0;
  if (level <= 0 || level > 65 || exp < 0 || exp > ExperienceCalc::kExpPerLevel) return -1;  // Filters init bad values.
  return level;
}

int Experience::get_aa_total_points() const {
  // The server and client do not directly track the total number of AA points. Instead the new UI calculates the total
  // spent by summing up the allocated based on earned abilities and then we can add the unspent.
  const int *aa_wnd = reinterpret_cast<const int *>(Zeal::Game::Windows->AA);
  if (!aa_wnd) return -1;  // Disables AA exp calc. Will be nullptr with old UI or if UI isn't initialized.

  // The fields below are updated in CAAWnd::Update().
  int total_unspent = aa_wnd[0x980 / sizeof(int)];  // Copied from self->char_info->AlternateAdvancementUnspent.
  int total_spent = aa_wnd[0x984 / sizeof(int)];    // Accumulates spent points.
  int total_points = total_unspent + total_spent;

  auto self = Zeal::Game::get_self();
  auto char_info = self ? self->CharInfo : nullptr;
  int exp = char_info ? self->CharInfo->AlternateAdvancementExperience : -1;
  if (total_points < 0 || total_points > 500 || exp < 0 || exp > ExperienceCalc::kExpPerLevel) return -1;
  return total_points;
}

void Experience::callback_main() {
  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  if (!self || !self->CharInfo || !Zeal::Game::is_in_game()) return;

  exp_calc.update(get_exp_level(), self->CharInfo->Experience);

  // Detect change in aa_points and play a level Ding if it increases. Note that just to
  // avoid any initialization timing dependency around when the AA window data gets
  // refreshed, we don't play a sound unless the last_aa_points was > 0 (skips first ding).
  int aa_points = get_aa_total_points();
  if (aa_points > last_aa_points && last_aa_points > 0 && setting_aa_ding.get()) Zeal::Game::WavePlay(139);  // Ding!
  last_aa_points = aa_points;
  aa_calc.update(aa_points, self->CharInfo->AlternateAdvancementExperience);
}

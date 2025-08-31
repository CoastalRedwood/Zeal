#pragma once
#include <Windows.h>

#include <deque>

#include "zeal_settings.h"

class ExperienceCalc {
 public:
  static constexpr int kExpPerLevel = 330;  // 330 exp points = 100%.

  ExperienceCalc();

  void reset(int level = 0, int exp = 0);
  void update(int level, int exp);
  void dump() const;  // Debug use.

  float get_exp_per_hour_pct() const { return exp_per_hour_pct; }

 private:
  static constexpr int kMaxQueueSize = 20;  // Calculate rate based on last 20 exp updates.

  struct ExpData {
    ULONGLONG timestamp;
    int total_exp;

    ExpData(ULONGLONG _timestamp, int _total_exp) : timestamp(_timestamp), total_exp(_total_exp){};
  };

  float exp_per_hour_pct = 0;     // Calculated latest experience rate.
  std::deque<ExpData> exp_queue;  // Timestamped fifo list of most recent exp changes.
};

class Experience {
 public:
  Experience(class ZealService *zeal);

  float get_exp_per_hour_pct() const { return exp_calc.get_exp_per_hour_pct(); }

  float get_aa_exp_per_hour_pct() const { return aa_calc.get_exp_per_hour_pct(); }

  ZealSetting<bool> setting_aa_ding = {true, "Zeal", "AADing", false};

 private:
  void reset();                     // Resets both exp and aa calcs.
  void reset_exp();                 // Resets exp rate to zero and initializes state.
  void reset_aa();                  // Resets aa rate to zero and initializes state.
  int get_exp_level() const;        // Returns the current level (if level and exp valid).
  int get_aa_total_points() const;  // Returns the total number of AA points.
  void callback_main();             // Periodic update and recalculation of exp rates.

  ExperienceCalc exp_calc;  // Normal experience.
  ExperienceCalc aa_calc;   // Alternate advancement experience.
  int last_aa_points = -1;  // Tracks changes in aa_level for playing ding sound.
};

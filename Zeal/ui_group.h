#pragma once

#include "zeal_settings.h"

class ui_group {
 public:
  ui_group(class ZealService *zeal, class UIManager *mgr);
  ~ui_group();
  void sort();
  void swap(int index1, int index2);

  ZealSetting<bool> setting_add_group_colors = {false, "Zeal", "AddGroupColors", false,
                                                [this](bool val) { handle_group_colors(); }};

 private:
  void InitUI();
  void handle_group_colors(bool report_errors = false);

  UIManager *ui;
};

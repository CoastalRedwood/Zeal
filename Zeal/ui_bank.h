#pragma once
#include "game_ui.h"
#include "hook_wrapper.h"
#include "memory.h"

class ui_bank {
 public:
  void change();
  ui_bank(class ZealService *zeal, class UIManager *mgr);
  ~ui_bank();

 private:
  void InitUI();
  UIManager *ui;
};
#pragma once
#include "game_ui.h"
#include "hook_wrapper.h"
#include "memory.h"

class ui_inspect {
 public:
  ui_inspect(class ZealService *zeal, class UIManager *mgr);
  ~ui_inspect();
  void *orig_vtable = nullptr;

 private:
  void InitUI();
  UIManager *ui;
};

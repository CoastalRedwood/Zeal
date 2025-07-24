#pragma once
#include "game_ui.h"
#include "hook_wrapper.h"
#include "memory.h"

class ui_guild {
 public:
  Zeal::GameUI::BasicWnd *guild = nullptr;
  Zeal::GameUI::ListWnd *members = nullptr;
  ui_guild(class ZealService *zeal, class IO_ini *ini, class UIManager *mgr);
  ~ui_guild();

 private:
  void InitUI();
  void CleanUI();
  void LoadSettings(class IO_ini *ini);
  UIManager *ui;
};

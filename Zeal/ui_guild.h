#pragma once

#include "game_ui.h"

class ui_guild {
 public:
  Zeal::GameUI::BasicWnd *guild = nullptr;
  Zeal::GameUI::ListWnd *members = nullptr;
  ui_guild(class ZealService *zeal, class UIManager *mgr);
  ~ui_guild();

 private:
  void InitUI();
  void CleanUI();
  UIManager *ui;
};

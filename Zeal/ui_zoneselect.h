#pragma once

#include <string>
#include <unordered_map>

#include "game_ui.h"

class ui_zoneselect {
 public:
  void Show();
  void Hide();
  void ShowButton();
  void HideButton();
  std::unordered_map<std::string, int> zones;
  ui_zoneselect(class ZealService *zeal, class UIManager *mgr);
  ~ui_zoneselect();

 private:
  Zeal::GameUI::SidlWnd *wnd = nullptr;
  Zeal::GameUI::SidlWnd *btn_wnd = nullptr;  // Optional button if not in xml.
  void InitUI();                             // Called in InitCharSelectUI().
  void Deactivate();
  void CleanUI();
  UIManager *ui;
};

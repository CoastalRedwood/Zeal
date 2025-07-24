#pragma once
#include <map>

#include "game_ui.h"
#include "hook_wrapper.h"
#include "memory.h"

class hotbutton_state {
 public:
  void tick();
  bool active();
  void set(int _duration);
  Zeal::GameUI::BasicWnd *wnd = 0;
  hotbutton_state(Zeal::GameUI::BasicWnd *btn);
  hotbutton_state(){};

 private:
  ULONGLONG start_time = 0;
  int duration = 0;
};

class ui_hotbutton {
 public:
  int last_button = 0;
  int last_page = 0;
  bool is_btn_active(Zeal::GameUI::BasicWnd *btn);
  ui_hotbutton(class ZealService *zeal, class UIManager *mgr);
  ~ui_hotbutton();

 private:
  std::unordered_map<int, Zeal::GameUI::BasicWnd *> buttons;
  std::unordered_map<int, std::unordered_map<int, hotbutton_state>> states;
  void InitUI();
  void CleanUI();
  void Render();

  UIManager *ui;
};

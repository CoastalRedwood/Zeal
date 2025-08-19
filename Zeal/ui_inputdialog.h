#pragma once

#include <functional>
#include <string>
#include <utility>

#include "game_ui.h"

using InputDialogCallback = std::function<void(std::string input)>;

class ui_inputdialog {
 public:
  bool show(const std::string &title, const std::string &message, const std::string &button1, const std::string button2,
            InputDialogCallback button1_callback = nullptr, InputDialogCallback button2_callback = nullptr,
            bool show_input_field = true);
  void hide();
  bool isVisible();
  std::string getTitle() const;
  std::pair<InputDialogCallback, InputDialogCallback> button_callbacks;
  Zeal::GameUI::BasicWnd *button1;
  Zeal::GameUI::BasicWnd *button2;
  Zeal::GameUI::BasicWnd *label;
  Zeal::GameUI::EditWnd *input;

  ui_inputdialog(class ZealService *zeal, class UIManager *mgr);
  ~ui_inputdialog();

 private:
  Zeal::GameUI::SidlWnd *wnd = nullptr;
  void InitUI();
  void Deactivate();
  void CleanUI();
  UIManager *ui;
};

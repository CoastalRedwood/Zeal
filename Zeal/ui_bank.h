#pragma once

class ui_bank {
 public:
  void change();
  ui_bank(class ZealService *zeal, class UIManager *mgr);
  ~ui_bank();

 private:
  void InitUI();
  UIManager *ui;
};
#pragma once

class ui_loot {
 public:
  ui_loot(class ZealService *zeal, class UIManager *mgr);
  ~ui_loot();

 private:
  void InitUI();
  UIManager *ui;
};

#pragma once

class ui_group {
 public:
  ui_group(class ZealService *zeal, class UIManager *mgr);
  ~ui_group();
  void sort();
  void swap(int index1, int index2);

 private:
  void InitUI();
  UIManager *ui;
};

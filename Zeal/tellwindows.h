#pragma once

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "game_ui.h"

class TellWindows {
 public:
  bool HandleKeyPress(int key, bool down, int modifier);
  std::string GetTellWindowName() const;
  bool IsTellWindow(struct Zeal::GameUI::ChatWnd *wnd) const;
  TellWindows(class ZealService *zeal);
  ~TellWindows();
  void SetEnabled(bool val);
  void SetHist(bool val);
  Zeal::GameUI::ChatWnd *FindTellWnd(std::string &name);
  void AddOutputText(Zeal::GameUI::ChatWnd *&wnd, std::string &msg, short channel);
  void CloseAllWindows();

  void AddOptionsCallback(std::function<void()> callback) { update_options_ui_callback = callback; };

  bool enabled = false;
  bool hist_enabled = true;

 private:
  Zeal::GameUI::ChatWnd *FindPreviousTellWnd();
  Zeal::GameUI::ChatWnd *FindNextTellWnd();
  std::unordered_map<std::string, std::vector<std::pair<short, std::string>>> tell_cache;
  std::function<void()> update_options_ui_callback;
  void CleanUI();
  void LoadUI();
};

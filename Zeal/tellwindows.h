#pragma once

#include <list>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "game_ui.h"
#include "zeal_settings.h"

class TellWindows {
 public:
  bool HandleKeyPress(int key, bool down, int modifier);
  std::string GetTellWindowName() const;
  bool IsTellWindow(struct Zeal::GameUI::ChatWnd *wnd) const;
  TellWindows(class ZealService *zeal);
  ~TellWindows();
  Zeal::GameUI::ChatWnd *FindTellWnd(std::string &name);
  void AddOutputText(Zeal::GameUI::ChatWnd *&wnd, std::string &msg, short channel);
  void CloseMostRecentWindow();
  void CloseAllWindows();

  void AddOptionsCallback(std::function<void()> callback) { update_options_ui_callback = callback; };

  ZealSetting<bool> setting_enabled = {false, "TellWindows", "Enabled", true};
  ZealSetting<bool> setting_hist_enabled = {false, "TellWindows", "HistoryEnabled", true};

 private:
  void UpdateMostRecentList(const std::string &name);
  Zeal::GameUI::ChatWnd *FindPreviousTellWnd();
  Zeal::GameUI::ChatWnd *FindNextTellWnd();
  std::unordered_map<std::string, std::vector<std::pair<short, std::string>>> tell_cache;
  std::function<void()> update_options_ui_callback;
  std::list<std::string> most_recent_list;  // Ordered list of tell targets.
  void CleanUI();
};

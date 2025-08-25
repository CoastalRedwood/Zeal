#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "game_ui.h"
#include "ui_bank.h"
#include "ui_buff.h"
#include "ui_group.h"
#include "ui_guild.h"
#include "ui_hotbutton.h"
#include "ui_inputdialog.h"
#include "ui_inspect.h"
#include "ui_loot.h"
#include "ui_options.h"
#include "ui_raid.h"
#include "ui_zoneselect.h"
#include "zeal_settings.h"

class UIManager {
 public:
  Zeal::GameUI::SliderWnd *GetSlider(std::string name);
  Zeal::GameUI::BasicWnd *GetCheckbox(std::string name);
  Zeal::GameUI::BasicWnd *GetButton(std::string name);
  Zeal::GameUI::BasicWnd *GetCombo(std::string name);
  std::function<void(Zeal::GameUI::SliderWnd *, int)> GetSliderCallback(Zeal::GameUI::SliderWnd *wnd);
  std::function<void(Zeal::GameUI::BasicWnd *, int)> GetComboCallback(Zeal::GameUI::BasicWnd *wnd);
  std::function<void(Zeal::GameUI::BasicWnd *)> GetButtonCallback(Zeal::GameUI::BasicWnd *wnd);
  std::function<void(Zeal::GameUI::BasicWnd *)> GetCheckboxCallback(Zeal::GameUI::BasicWnd *wnd);

  Zeal::GameUI::BasicWnd *clicked_button = nullptr;
  std::unordered_map<Zeal::GameUI::BasicWnd *, std::unordered_map<std::string, Zeal::GameUI::BasicWnd *>>
      WindowChildren;
  void SetLabelValue(std::string name, const char *format, ...);
  void SetSliderValue(std::string name, int value);
  void SetSliderValue(std::string name, float value);
  void SetComboValue(std::string name, int value);
  void SetChecked(std::string name, bool checked);
  Zeal::GameUI::BasicWnd *AddButtonCallback(Zeal::GameUI::BasicWnd *wnd, std::string name,
                                            std::function<void(Zeal::GameUI::BasicWnd *)> callback,
                                            bool log_errors = true);
  Zeal::GameUI::BasicWnd *AddCheckboxCallback(Zeal::GameUI::BasicWnd *wnd, std::string name,
                                              std::function<void(Zeal::GameUI::BasicWnd *)> callback,
                                              bool log_errors = true);
  Zeal::GameUI::BasicWnd *AddSliderCallback(Zeal::GameUI::BasicWnd *wnd, std::string name,
                                            std::function<void(Zeal::GameUI::SliderWnd *, int)> callback,
                                            int max_val = 100, bool log_errors = true);
  Zeal::GameUI::BasicWnd *AddComboCallback(Zeal::GameUI::BasicWnd *wnd, std::string name,
                                           std::function<void(Zeal::GameUI::BasicWnd *, int)> callback,
                                           bool log_errors = true);
  void AddLabel(Zeal::GameUI::BasicWnd *wnd, std::string name, bool log_errors = true);
  void AddListItems(Zeal::GameUI::ComboWnd *wnd, const std::vector<std::string> data);
  void AddListItems(Zeal::GameUI::ListWnd *wnd, const std::vector<std::vector<std::string>> data);
  void AddListItems(Zeal::GameUI::ListWnd *wnd, const std::vector<std::string> data);
  Zeal::GameUI::SidlWnd *CreateSidlScreenWnd(const std::string &name);
  void DestroySidlScreenWnd(Zeal::GameUI::SidlWnd *sidl_wnd);
  bool WriteTemporaryUI(const std::filesystem::path &orig_xml_file, const std::filesystem::path &merged_xml_file);
  void RemoveTemporaryUI(const std::filesystem::path &file_path);
  UIManager(class ZealService *zeal);
  std::shared_ptr<ui_options> options = nullptr;
  std::shared_ptr<ui_bank> bank = nullptr;
  std::shared_ptr<ui_loot> loot = nullptr;
  std::shared_ptr<ui_guild> guild = nullptr;
  std::shared_ptr<ui_raid> raid = nullptr;
  std::shared_ptr<ui_hotbutton> hotbutton = nullptr;
  std::shared_ptr<ui_group> group = nullptr;
  std::shared_ptr<ui_inputdialog> inputDialog = nullptr;
  std::shared_ptr<ui_buff> buffs = nullptr;
  std::shared_ptr<ui_zoneselect> zoneselect = nullptr;
  std::shared_ptr<ui_inspect> inspect = nullptr;

  ZealSetting<bool> setting_show_ui_errors = {true, "Zeal", "ShowUIErrors", false};

 private:
  bool handle_autobank(const std::vector<std::string> &args);
  bool handle_uierrors(const std::vector<std::string> &args);
  bool handle_uilock(const std::vector<std::string> &args);

  std::unordered_map<std::string, Zeal::GameUI::BasicWnd *> checkbox_names;
  std::unordered_map<std::string, Zeal::GameUI::BasicWnd *> button_names;
  std::unordered_map<Zeal::GameUI::BasicWnd *, std::function<void(Zeal::GameUI::BasicWnd *)>> checkbox_callbacks;
  std::unordered_map<Zeal::GameUI::BasicWnd *, std::function<void(Zeal::GameUI::BasicWnd *)>> button_callbacks;
  std::unordered_map<Zeal::GameUI::SliderWnd *, std::function<void(Zeal::GameUI::SliderWnd *, int)>> slider_callbacks;
  std::unordered_map<Zeal::GameUI::BasicWnd *, std::function<void(Zeal::GameUI::BasicWnd *, int)>> combo_callbacks;
  std::unordered_map<std::string, Zeal::GameUI::SliderWnd *> slider_names;

  std::unordered_map<std::string, Zeal::GameUI::BasicWnd *> combo_names;
  std::unordered_map<std::string, Zeal::GameUI::BasicWnd *> label_names;

  void CleanUI();
};

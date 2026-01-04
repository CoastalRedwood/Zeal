#pragma once

#include <string>
#include <utility>
#include <vector>

#include "game_ui.h"
#include "zeal_settings.h"

class ui_options {
 public:
  void UpdateOptions();
  void UpdateOptionsMap();
  void UpdateOptionsTargetRing();
  void UpdateOptionsNameplate();
  void UpdateOptionsCamera();
  void UpdateOptionsGeneral();
  void UpdateOptionsFloatingDamage();
  void SaveColors() const;
  void LoadColors();
  DWORD GetColor(int index) const;
  void ShowInviteDialog(const char *raid_invite_name = nullptr) const;
  void HideInviteDialog() const;
  void PlayInviteSound() const;
  void PlayTellSound() const;

  Zeal::GameUI::SidlWnd *GetZealOptionsWindow() { return wnd; }  // Only use for short-term access.

  ui_options(class ZealService *zeal, class UIManager *mgr);
  ~ui_options();

  ZealSetting<bool> setting_enable_container_lock = {false, "Zeal", "EnableContainerLock", false};
  ZealSetting<bool> setting_ctrl_context_menus = {false, "Zeal", "CtrlContextMenus", false};
  ZealSetting<bool> setting_invite_dialog = {false, "Zeal", "InviteDialog", false};
  ZealSetting<std::string> setting_invite_sound = {"", "Zeal", "InviteSound", false,
                                                   [this](const std::string &) { PlayInviteSound(); }};
  ZealSetting<std::string> setting_tell_sound = {"", "Zeal", "TellSound", false,
                                                 [this](const std::string &) { PlayTellSound(); }};
  ZealSetting<bool> setting_slash_not_poke = {false, "Zeal", "SlashNotPoke", false};
  ZealSetting<bool> setting_escape = {false, "Zeal", "Escape", false};
  ZealSetting<bool> setting_escape_raid_lock = {false, "Zeal", "EscapeRaidLock", false};
  ZealSetting<bool> setting_dialog_position = {false, "Zeal", "DialogPosition", false,
                                               [this](const bool &) { SyncDialogPosition(); }};
  ZealSetting<bool> setting_per_char_keybinds = {false, "Zeal", "PerCharKeybinds", false,
                                                 [this](const bool &) { SyncKeybinds(); }};
  ZealSetting<bool> setting_per_char_autojoin = {false, "Zeal", "PerCharAutojoin", false,
                                                 [this](const bool &) { SyncIniAutojoin(); }};

 private:
  void InitUI();
  void InitColors();
  void InitGeneral();
  void InitMap();
  void InitCamera();
  void InitTargetRing();
  void InitNameplate();
  void InitFloatingDamage();
  void UpdateDynamicUI();
  void CleanUI();
  void CleanDynamicUI();
  void RenderUI();
  void Deactivate();
  int FindComboIndex(std::string combobox, std::string text_value);
  void UpdateComboBox(const std::string &name, const std::string &label, const std::string &default_label);
  void SyncDialogPosition();
  void SyncKeybinds();
  void SyncIniAutojoin();

  Zeal::GameUI::SidlWnd *wnd = nullptr;
  std::vector<Zeal::GameUI::BasicWnd *> color_buttons;
  UIManager *const ui;
  std::vector<std::pair<int, std::string>> sound_list;  // WavePlay index table.
  char ini_autojoin_name[16 + 30 + 2];                  // Space for "ChannelAutoJoin_" + self->name + null.
};

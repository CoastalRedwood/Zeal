#include "ui_options.h"

#include <algorithm>

#include "game_addresses.h"
#include "game_functions.h"
#include "game_structures.h"
#include "string_util.h"
#include "zeal.h"
#include "zeal_settings.h"

static constexpr int kMaxComboBoxItems = 50;  // Maximum length of dynamic combobox lists.
static constexpr char kDefaultSoundNone[] = "None";

float GetSensitivityFromSlider(int value) { return value / 100.f; }

int GetSensitivityForSlider(float *value) {
  if (value && *value > 0)
    return static_cast<int>(*value * 100.f);
  else
    return 0;
}

int GetSensitivityForSlider(ZealSetting<float> &value) {
  if (value.get() > 0)
    return static_cast<int>(value.get() * 100.f);
  else
    return 0;
}

void ui_options::PlayTellSound() const {
  // For now at least just copy the same sound list from invites. A future goal is to
  // support custom wave sounds for tells.
  const auto &sound_name = setting_tell_sound.get();
  if (sound_name.empty() || sound_name == kDefaultSoundNone) return;
  const auto it = std::find_if(sound_list.begin(), sound_list.end(),
                               [sound_name](const auto &entry) { return entry.second == sound_name; });
  if (it != sound_list.end() && it->first >= 0) Zeal::Game::WavePlay(it->first);
}

void ui_options::PlayInviteSound() const {
  const auto &sound_name = setting_invite_sound.get();
  if (sound_name.empty() || sound_name == kDefaultSoundNone) return;
  const auto it = std::find_if(sound_list.begin(), sound_list.end(),
                               [sound_name](const auto &entry) { return entry.second == sound_name; });
  if (it != sound_list.end() && it->first >= 0) Zeal::Game::WavePlay(it->first);
}

// Internal helpers for ShowInviteDialog
static constexpr char kInviteDialogTitle[] = "Invite";

static void handle_follow() {
  const auto self = Zeal::Game::get_self();
  if (self && self->ActorInfo && self->ActorInfo->IsInvited) Zeal::Game::get_game()->Follow();
}

static void handle_decline() {
  const auto self = Zeal::Game::get_self();
  if (self && self->ActorInfo && self->ActorInfo->IsInvited) Zeal::Game::get_game()->Disband();
}

void ui_options::ShowInviteDialog(const char *raid_invite_name) const {
  if (!setting_invite_dialog.get() || !ZealService::get_instance()->ui->inputDialog) return;

  std::string message = raid_invite_name ? (std::string(raid_invite_name) + " invites you to join a raid.")
                                         : "You have been invited to a group";

  // Close / abort any open dialog and then open up a new one with the invite.
  ZealService::get_instance()->ui->inputDialog->hide();
  if (raid_invite_name)
    ZealService::get_instance()->ui->inputDialog->show(
        kInviteDialogTitle, message, "Accept", "Decline", [this](std::string unused) { Zeal::Game::do_raidaccept(); },
        [this](std::string unused) { Zeal::Game::do_raiddecline(); }, false);
  else
    ZealService::get_instance()->ui->inputDialog->show(
        kInviteDialogTitle, message, "Follow", "Decline", [this](std::string unused) { handle_follow(); },
        [this](std::string unused) { handle_decline(); }, false);
}

void ui_options::HideInviteDialog() const {
  // The invite dialog uses the shared input dialog window, so filter these calls to only
  // hide when it is an invite dialog (matching title).
  auto dialog = ZealService::get_instance()->ui->inputDialog.get();
  if (dialog && dialog->isVisible() && dialog->getTitle() == kInviteDialogTitle) dialog->hide();
}

static void __fastcall GamePlayerSetInvited(Zeal::GameStructures::Entity *this_entity, int unused_edx, int flag) {
  if (ZealService::get_instance()->ui) {
    if (flag) {
      ZealService::get_instance()->ui->options->PlayInviteSound();
      ZealService::get_instance()->ui->options->ShowInviteDialog();
    } else {
      ZealService::get_instance()->ui->options->HideInviteDialog();
    }
  }

  ZealService::get_instance()->hooks->hook_map["GamePlayerSetInvited"]->original(GamePlayerSetInvited)(
      this_entity, unused_edx, flag);
}

static void __fastcall RaidSendInviteResponse(void *raid, int unused_edx, int flag) {
  if (ZealService::get_instance()->ui) ZealService::get_instance()->ui->options->HideInviteDialog();

  ZealService::get_instance()->hooks->hook_map["RaidSendInviteResponse"]->original(RaidSendInviteResponse)(
      raid, unused_edx, flag);
}

static void __fastcall RaidHandleCreateInviteRaid(void *raid, int unused_edx, char *payload) {
  if (ZealService::get_instance()->ui && payload) {
    ZealService::get_instance()->ui->options->PlayInviteSound();
    ZealService::get_instance()->ui->options->ShowInviteDialog(payload + 0x44);
  }

  ZealService::get_instance()->hooks->hook_map["RaidHandleCreateInviteRaid"]->original(RaidHandleCreateInviteRaid)(
      raid, unused_edx, payload);
}

int __fastcall WndNotification(Zeal::GameUI::BasicWnd *wnd, int unused, Zeal::GameUI::BasicWnd *sender, int message,
                               int data) {
  UIManager *ui = ZealService::get_instance()->ui.get();
  if (ui && Zeal::Game::Windows && sender == (Zeal::GameUI::BasicWnd *)Zeal::Game::Windows->ColorPicker) {
    if (message == 0x1E && ui->clicked_button) {
      ui->clicked_button->TextColor.ARGB = data | 0xff000000;  // Ensure alpha = 0xff.
      ui->options->SaveColors();
    }
  }
  return reinterpret_cast<int(__thiscall *)(Zeal::GameUI::BasicWnd * wnd, Zeal::GameUI::BasicWnd * sender, int message,
                                            int data)>(0x56e920)(wnd, sender, message, data);
}

void ui_options::SaveColors() const {
  IO_ini *ini = ZealService::get_instance()->ini.get();
  for (auto &[index, btn] : color_buttons) {
    ini->setValue("ZealColors", "Color" + std::to_string(index), std::to_string(btn->TextColor.ARGB));
  }
}

DWORD ui_options::GetColor(int index) const {
  auto it = color_buttons.find(index);
  return (it == color_buttons.end()) ? 0xFFFFFFFF : it->second->TextColor.ARGB;
}

void ui_options::LoadColors() {
  IO_ini *ini = ZealService::get_instance()->ini.get();

  if (!ini->exists("ZealColors", "Color0"))  // Adds default Nameplate Color to Button1  for new users
  {
    if (color_buttons.count(0)) color_buttons[0]->TextColor.ARGB = 0xFFFF8000;  // AFK - Orange
  }
  if (!ini->exists("ZealColors", "Color1"))  // Adds default Nameplate Color to Button2 for new users
  {
    if (color_buttons.count(1)) color_buttons[1]->TextColor.ARGB = 0xFFCFFF00;  // LFG - Yellow
  }
  if (!ini->exists("ZealColors", "Color2"))  // Adds default Nameplate Color to Button3 for new users
  {
    if (color_buttons.count(2)) color_buttons[2]->TextColor.ARGB = 0xFFFF0000;  // LinkDead - Red
  }
  if (!ini->exists("ZealColors", "Color3"))  // Adds default Nameplate Color to Button4 for new users
  {
    if (color_buttons.count(3)) color_buttons[3]->TextColor.ARGB = 0xFFFF8080;  // Guild Member - White Red
  }
  if (!ini->exists("ZealColors", "Color4"))  // Adds default Nameplate Color to Button5 for new users
  {
    if (color_buttons.count(4)) color_buttons[4]->TextColor.ARGB = 0xFFFFFFFF;  // Raid Member - White Light Purple
  }
  if (!ini->exists("ZealColors", "Color5"))  // Adds default Nameplate Color to Button6 for new users
  {
    if (color_buttons.count(5)) color_buttons[5]->TextColor.ARGB = 0xFF00FF32;  // Group Member - Light Green
  }
  if (!ini->exists("ZealColors", "Color6"))  // Adds default Nameplate Color to Button7 for new users
  {
    if (color_buttons.count(6)) color_buttons[6]->TextColor.ARGB = 0xFFFF0000;  // PVP - Red
  }
  if (!ini->exists("ZealColors", "Color7"))  // Adds default Nameplate Color to Button8 for new users
  {
    if (color_buttons.count(7)) color_buttons[7]->TextColor.ARGB = 0xFF85489C;  // Roleplay - Purple
  }
  if (!ini->exists("ZealColors", "Color8"))  // Adds default Nameplate Color to Button9 for new users
  {
    if (color_buttons.count(8)) color_buttons[8]->TextColor.ARGB = 0xFFFFFF80;  // OtherGuild Member - White Yellow
  }
  if (!ini->exists("ZealColors", "Color9"))  // Adds default Nameplate Color to Button10 for new users
  {
    if (color_buttons.count(9)) color_buttons[9]->TextColor.ARGB = 0xFF3D6BDC;  // DefaultAdventurer - Default Blue
  }
  if (!ini->exists("ZealColors", "Color10"))  // Adds default Nameplate Color to Button11 for new users
  {
    if (color_buttons.count(10)) color_buttons[10]->TextColor.ARGB = 0xFF202020;  // Npc Corpse - Dark gray
  }
  if (!ini->exists("ZealColors", "Color11"))  // Adds default Nameplate Color to Button12 for new users
  {
    if (color_buttons.count(11)) color_buttons[11]->TextColor.ARGB = 0xFFFFFFFF;  // Players Corpse - White Light Purple
  }
  if (!ini->exists("ZealColors", "Color12"))  // Adds default Nameplate Color to Button13 for new users
  {
    if (color_buttons.count(12)) color_buttons[12]->TextColor.ARGB = D3DCOLOR_ARGB(0xff, 0x0, 0xf0, 0x0);  // CON_GREEN
  }
  if (!ini->exists("ZealColors", "Color13"))  // Adds default Nameplate Color to Button14 for new users
  {
    if (color_buttons.count(13))
      color_buttons[13]->TextColor.ARGB = D3DCOLOR_ARGB(0xff, 0x0, 0xf0, 0xf0);  // CON_LIGHTBLUE
  }
  if (!ini->exists("ZealColors", "Color14"))  // Adds default Nameplate Color to Button15 for new users
  {
    if (color_buttons.count(14)) {
      if (ZealService::get_instance()->ini->getValue<bool>("Zeal", "Bluecon"))
        color_buttons[14]->TextColor.ARGB =
            Zeal::Game::get_user_color(70);  // BlueCon - Keeps original BlueCon if set from old Options, Color #70
      else
        color_buttons[14]->TextColor.ARGB =
            D3DCOLOR_ARGB(0xff, 0x0, 0x40,
                          0xf0);  // Default DarkBlue is lighter than CON_BLUE as Nameplate Color Button15 for new users
    }
  }
  if (!ini->exists("ZealColors", "Color15"))  // Adds default Nameplate Color to Button16 for new users
  {
    if (color_buttons.count(15))
      color_buttons[15]->TextColor.ARGB = D3DCOLOR_ARGB(0xff, 0xf0, 0xf0, 0xf0);  // CON_WHITE
  }
  if (!ini->exists("ZealColors", "Color16"))  // Adds default Nameplate Color to Button17 for new users
  {
    if (color_buttons.count(16))
      color_buttons[16]->TextColor.ARGB = D3DCOLOR_ARGB(0xff, 0xf0, 0xf0, 0x0);  // CON_YELLOW
  }
  if (!ini->exists("ZealColors", "Color17"))  // Adds default Nameplate Color to Button18 for new users
  {
    if (color_buttons.count(17)) color_buttons[17]->TextColor.ARGB = D3DCOLOR_ARGB(0xff, 0xf0, 0x0, 0x0);  // CON_RED
  }
  if (!ini->exists("ZealColors", "Color18"))  // Adds default Nameplate Color to Button18 for new users
  {
    if (color_buttons.count(18))
      color_buttons[18]->TextColor.ARGB = D3DCOLOR_ARGB(0xff, 0xff, 0x80, 0xff);  // Target Pink Default
  }
  if (!ini->exists("ZealColors", "Color19"))  // My Pet Damage
  {
    if (color_buttons.count(19))
      color_buttons[19]->TextColor.ARGB = D3DCOLOR_ARGB(0xff, 0xf0, 0xf0, 0xf0);  // Default White
  }
  if (!ini->exists("ZealColors", "Color20"))  // Other Pet Damage
  {
    if (color_buttons.count(20))
      color_buttons[20]->TextColor.ARGB = D3DCOLOR_ARGB(0xff, 0xf0, 0xf0, 0xf0);  // Default White
  }
  if (!ini->exists("ZealColors", "Color21"))  // My Pet Say
  {
    if (color_buttons.count(21))
      color_buttons[21]->TextColor.ARGB = D3DCOLOR_ARGB(0xff, 0xf0, 0xf0, 0xf0);  // Default White
  }
  if (!ini->exists("ZealColors", "Color22"))  // Other Pet Say
  {
    if (color_buttons.count(22))
      color_buttons[22]->TextColor.ARGB = D3DCOLOR_ARGB(0xff, 0xf0, 0xf0, 0xf0);  // Default White
  }
  if (!ini->exists("ZealColors", "Color23"))  // My Melee Special
  {
    if (color_buttons.count(23))
      color_buttons[23]->TextColor.ARGB = D3DCOLOR_ARGB(0xff, 0xf0, 0xf0, 0xf0);  // Default White
  }
  if (!ini->exists("ZealColors", "Color24"))  // Other Melee Special
  {
    if (color_buttons.count(24))
      color_buttons[24]->TextColor.ARGB = D3DCOLOR_ARGB(0xff, 0xf0, 0xf0, 0xf0);  // Default White
  }
  // Color25: Unused
  // Color26: Unused
  // Color27: Unused
  // Color28: Unused
  // Color29: Unused
  if (!ini->exists("ZealColors", "Color30"))  // Nameplate: Guild LFG
  {
    if (color_buttons.count(30)) color_buttons[30]->TextColor.ARGB = 0xFFCFFF00;  // LFG - Yellow
  }
  if (!ini->exists("ZealColors", "Color31"))  // Nameplate: PvP Ally
  {
    if (color_buttons.count(31)) color_buttons[31]->TextColor.ARGB = 0xFF3D6BDC;  // Default Blue
  }
  if (!ini->exists("ZealColors", "Color32"))  // FCD: My melee damage
  {
    if (color_buttons.count(32)) color_buttons[32]->TextColor.ARGB = D3DCOLOR_XRGB(0x00, 0xf0, 0xf0);  // Light blue
  }
  if (!ini->exists("ZealColors", "Color33"))  // FCD: My spell damage
  {
    if (color_buttons.count(33)) color_buttons[33]->TextColor.ARGB = D3DCOLOR_XRGB(0x00, 0xf0, 0xf0);  // Light blue
  }
  if (!ini->exists("ZealColors", "Color34"))  // FCD: Melee hitting me
  {
    if (color_buttons.count(34)) color_buttons[34]->TextColor.ARGB = D3DCOLOR_XRGB(0xf0, 0x00, 0x00);  // Red
  }
  if (!ini->exists("ZealColors", "Color35"))  // FCD: Spells hitting me
  {
    if (color_buttons.count(35)) color_buttons[35]->TextColor.ARGB = D3DCOLOR_XRGB(0xf0, 0x00, 0x00);  // Red
  }
  if (!ini->exists("ZealColors", "Color36"))  // FCD: Melee hitting others
  {
    if (color_buttons.count(36)) color_buttons[36]->TextColor.ARGB = D3DCOLOR_XRGB(0xff, 0x80, 0x00);  // Orange
  }
  if (!ini->exists("ZealColors", "Color37"))  // FCD: Spells hitting others
  {
    if (color_buttons.count(37)) color_buttons[37]->TextColor.ARGB = D3DCOLOR_XRGB(0xff, 0x80, 0x00);  // Orange
  }
  if (!ini->exists("ZealColors", "Color38"))  // FCD: Melee hitting NPCs
  {
    if (color_buttons.count(38)) color_buttons[38]->TextColor.ARGB = D3DCOLOR_XRGB(0xf0, 0xf0, 0xf0);  // White
  }
  if (!ini->exists("ZealColors", "Color39"))  // FCD: Spells hitting NPCs
  {
    if (color_buttons.count(39)) color_buttons[39]->TextColor.ARGB = D3DCOLOR_XRGB(0xf0, 0xf0, 0xf0);  // White
  }

  for (auto &[index, btn] : color_buttons) {
    if (ini->exists("ZealColors", "Color" + std::to_string(index)))
      btn->TextColor.ARGB = ini->getValue<DWORD>("ZealColors", "Color" + std::to_string(index));
  }
}

void ui_options::InitUI() {
  if (wnd) Zeal::Game::print_chat("Warning: Init out of sync for ui_options");

  static const char *xml_file = "./uifiles/zeal/EQUI_ZealOptions.xml";
  if (!wnd && ui && std::filesystem::exists(xml_file)) wnd = ui->CreateSidlScreenWnd("ZealOptions");

  if (!wnd) {
    Zeal::Game::print_chat("Error: Failed to load %s", xml_file);
    return;
  }

  wnd->IsVisible = false;  // Redundant but ensure there is a transition to trigger sync.
  wnd->vtbl->WndNotification = WndNotification;
  InitGeneral();
  InitCamera();
  InitMap();
  InitTargetRing();
  InitColors();
  InitNameplate();
  InitFloatingDamage();

  // The option states are synchronized when the wnd is made visible.
}

int ScaleFloatToSlider(float value, float fmin, float fmax, Zeal::GameUI::SliderWnd *wnd) {
  if (!wnd) return 0;
  // Clamp the float value between fmin and fmax
  value = std::clamp(value, fmin, fmax);

  // Normalize the float value to the range [0, 1]
  float normalized = (value - fmin) / (fmax - fmin);

  // Map the normalized float to the integer range [imin, imax]
  return static_cast<int>(0 + normalized * (wnd->max_val));
}

float ScaleSliderToFloat(int ivalue, float fmin, float fmax, Zeal::GameUI::SliderWnd *wnd) {
  if (!wnd) return 0;
  // Clamp the integer value between imin and imax
  ivalue = std::clamp(ivalue, 0, wnd->max_val);

  // Normalize the integer value to the range [0, 1]
  float normalized = static_cast<float>(ivalue) / (wnd->max_val);

  // Map the normalized value to the float range [fmin, fmax]
  return fmin + normalized * (fmax - fmin);
}

void ui_options::InitColors() {
  if (!wnd) return;

  for (int i = 0; i < 100; i++) {
    Zeal::GameUI::BasicWnd *btn = ui->AddButtonCallback(
        wnd, "Zeal_Color" + std::to_string(i),
        [](Zeal::GameUI::BasicWnd *wnd) { Zeal::Game::Windows->ColorPicker->Activate(wnd, wnd->TextColor.ARGB); },
        false);
    if (btn) color_buttons[i] = btn;
  }
  LoadColors();
}

void ui_options::InitGeneral() {
  if (!wnd) return;
  /*add callbacks when the buttons are pressed in the options window*/
  ui->AddCheckboxCallback(wnd, "Zeal_HideCorpse", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->looting_hook->set_hide_looted(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_Cam", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->camera_mods->set_smoothing(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_BlueCon", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->chat_hook->UseBlueCon.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_Input", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->chat_hook->UseZealInput.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_Escape", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->ini->setValue<bool>("Zeal", "Escape", wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_RaidEscapeLock", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->ini->setValue<bool>("Zeal", "EscapeRaidLock", wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_ShowHelm", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->helm->ShowHelmEnabled.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_AltContainerTooltips", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->tooltips->set_alt_all_containers(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_SpellbookAutoStand", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->movement->SpellBookAutoStand.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_CastAutoStand", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->movement->CastAutoStand.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_RightClickToEquip", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->equip_item_hook->Enabled.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_ClassicClasses", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->chat_hook->UseClassicClassNames.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_TellWindows", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->tells->SetEnabled(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_TellWindowsHist", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->tells->SetHist(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_BuffTimers", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->ui->buffs->BuffTimers.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_RecastTimers", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->ui->buffs->RecastTimers.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_RecastTimersLeftAlign", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->ui->buffs->RecastTimersLeftAlign.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_BrownSkeletons", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->game_patches->BrownSkeletons.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_ClassicMusic", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->music->ClassicMusic.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_SuppressMissedNotes", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->chatfilter_hook->setting_suppress_missed_notes.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_SuppressOtherFizzles", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->chatfilter_hook->setting_suppress_other_fizzles.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_UseZealAssistOn", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->assist->setting_use_zeal_assist_on.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_DetectAssistFailure", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->assist->setting_detect_assist_failure.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_SingleClickGiveEnable", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->give->setting_enable_give.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_EnhancedSpellInfo", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->item_displays->setting_enhanced_spell_info.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_SlashNotPoke",
                          [this](Zeal::GameUI::BasicWnd *wnd) { setting_slash_not_poke.set(wnd->Checked); });
  ui->AddCheckboxCallback(wnd, "Zeal_InviteDialog",
                          [this](Zeal::GameUI::BasicWnd *wnd) { setting_invite_dialog.set(wnd->Checked); });
  ui->AddCheckboxCallback(wnd, "Zeal_AutoFollowEnable", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->game_patches->AutoFollowEnable.set(wnd->Checked);
  });

  ui->AddCheckboxCallback(wnd, "Zeal_LinkAllAltDelimiter", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->looting_hook->setting_alt_delimiter.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_CtrlRightClickCorpse", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->looting_hook->setting_ctrl_rightclick_loot.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_CtrlContextMenus", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->ui->options->setting_ctrl_context_menus.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_EnableContainerLock", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->ui->options->setting_enable_container_lock.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_ExportOnCamp", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->outputfile->setting_export_on_camp.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_SelfClickThru", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->camera_mods->setting_selfclickthru.set(wnd->Checked);
  });
  ui->AddComboCallback(wnd, "Zeal_Timestamps_Combobox", [](Zeal::GameUI::BasicWnd *wnd, int value) {
    ZealService::get_instance()->chat_hook->TimeStampsStyle.set(value);
  });
  ui->AddComboCallback(wnd, "Zeal_FPS_Combobox", [](Zeal::GameUI::BasicWnd *wnd, int value) {
    switch (value) {
      case 0:
        ZealService::get_instance()->dx->fps_limit.set(0);
        break;
      case 1:
        ZealService::get_instance()->dx->fps_limit.set(30);
        break;
      case 2:
        ZealService::get_instance()->dx->fps_limit.set(60);
        break;
      case 3:
        ZealService::get_instance()->dx->fps_limit.set(120);
        break;
      case 4:
        ZealService::get_instance()->dx->fps_limit.set(144);
        break;
      case 5:
        ZealService::get_instance()->dx->fps_limit.set(165);
        break;
      case 6:
        ZealService::get_instance()->dx->fps_limit.set(240);
        break;
      default:
        ZealService::get_instance()->dx->fps_limit.set(0);
        break;
    }
  });
  ui->AddSliderCallback(wnd, "Zeal_HoverTimeout_Slider", [this](Zeal::GameUI::SliderWnd *wnd, int value) {
    int val = value * 5;
    ZealService::get_instance()->tooltips->set_timer(val);
    ui->SetLabelValue("Zeal_HoverTimeout_Value", "%i ms", val);
  });
  ui->AddLabel(wnd, "Zeal_HoverTimeout_Value");
  ui->AddLabel(wnd, "Zeal_VersionValue");

  ui->AddComboCallback(wnd, "Zeal_TellSound_Combobox", [this](Zeal::GameUI::BasicWnd *wnd, int value) {
    std::string sound_name("");
    if (value >= 0) sound_name = wnd->CmbListWnd->GetItemText(value, 0);
    setting_tell_sound.set(sound_name);
  });

  ui->AddComboCallback(wnd, "Zeal_InviteSound_Combobox", [this](Zeal::GameUI::BasicWnd *wnd, int value) {
    std::string sound_name("");
    if (value >= 0) sound_name = wnd->CmbListWnd->GetItemText(value, 0);
    setting_invite_sound.set(sound_name);
  });
}

void ui_options::InitFloatingDamage() {
  if (!wnd) return;
  ui->AddCheckboxCallback(wnd, "Zeal_FloatingDamage", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->floating_damage->enabled.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_FloatingHideWithGui", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->floating_damage->hide_with_gui.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_FloatingSelf", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->floating_damage->show_self.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_FloatingPets", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->floating_damage->show_pets.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_FloatingOthers", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->floating_damage->show_others.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_FloatingNpcs", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->floating_damage->show_npcs.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_FloatingHpUpdates", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->floating_damage->show_hp_updates.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_FloatingMelee", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->floating_damage->show_melee.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_FloatingSpells", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->floating_damage->show_spells.set(wnd->Checked);
  });
  ui->AddLabel(wnd, "Zeal_FloatingBigHit_Value");
  ui->AddCheckboxCallback(wnd, "Zeal_FloatingSpellIcons", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->floating_damage->spell_icons.set(wnd->Checked);
  });
  ui->AddSliderCallback(wnd, "Zeal_FloatingBigHit_Slider", [this](Zeal::GameUI::SliderWnd *wnd, int value) {
    ZealService::get_instance()->floating_damage->big_hit_threshold.set(value * 10);
    ui->SetLabelValue("Zeal_FloatingBigHit_Value", "%i", value * 10);
  });
  ui->AddComboCallback(wnd, "Zeal_FloatingFont_Combobox", [this](Zeal::GameUI::BasicWnd *wnd, int value) {
    std::string font_name("");
    if (value >= 0) font_name = wnd->CmbListWnd->GetItemText(value, 0);
    ZealService::get_instance()->floating_damage->bitmap_font_filename.set(font_name);
  });
}

void ui_options::InitCamera() {
  if (!wnd) return;
  ui->AddCheckboxCallback(wnd, "Zeal_Cam_TurnLocked", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->camera_mods->cam_lock.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_UseOldSens", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->camera_mods->use_old_sens.set(wnd->Checked);
  });
  ui->AddSliderCallback(wnd, "Zeal_PanDelaySlider", [this](Zeal::GameUI::SliderWnd *wnd, int value) {
    ZealService::get_instance()->camera_mods->pan_delay.set(value * 4);
    ui->SetLabelValue("Zeal_PanDelayValueLabel", "%d ms", ZealService::get_instance()->camera_mods->pan_delay.get());
  });
  ui->AddSliderCallback(wnd, "Zeal_FirstPersonSlider_X", [this](Zeal::GameUI::SliderWnd *wnd, int value) {
    ZealService::get_instance()->camera_mods->user_sensitivity_x.set(GetSensitivityFromSlider(value));
    ui->SetLabelValue("Zeal_FirstPersonLabel_X", "%.2f",
                      ZealService::get_instance()->camera_mods->user_sensitivity_x.get());
  });
  ui->AddSliderCallback(wnd, "Zeal_FirstPersonSlider_Y", [this](Zeal::GameUI::SliderWnd *wnd, int value) {
    ZealService::get_instance()->camera_mods->user_sensitivity_y.set(GetSensitivityFromSlider(value));
    ui->SetLabelValue("Zeal_FirstPersonLabel_Y", "%.2f",
                      ZealService::get_instance()->camera_mods->user_sensitivity_y.get());
  });
  ui->AddSliderCallback(wnd, "Zeal_ThirdPersonSlider_X", [this](Zeal::GameUI::SliderWnd *wnd, int value) {
    ZealService::get_instance()->camera_mods->user_sensitivity_x_3rd.set(GetSensitivityFromSlider(value));
    ui->SetLabelValue("Zeal_ThirdPersonLabel_X", "%.2f",
                      ZealService::get_instance()->camera_mods->user_sensitivity_x_3rd.get());
  });
  ui->AddSliderCallback(wnd, "Zeal_ThirdPersonSlider_Y", [this](Zeal::GameUI::SliderWnd *wnd, int value) {
    ZealService::get_instance()->camera_mods->user_sensitivity_y_3rd.set(GetSensitivityFromSlider(value));
    ui->SetLabelValue("Zeal_ThirdPersonLabel_Y", "%.2f",
                      ZealService::get_instance()->camera_mods->user_sensitivity_y_3rd.get());
  });
  ui->AddSliderCallback(wnd, "Zeal_FoVSlider", [this](Zeal::GameUI::SliderWnd *wnd, int value) {
    float val = 45.0f + (static_cast<float>(value) / 100.0f) * 45.0f;
    ZealService::get_instance()->camera_mods->fov.set(val);
    ui->SetLabelValue("Zeal_FoVValueLabel", "%.0f", val);
  });

  ui->AddLabel(wnd, "Zeal_PanDelayValueLabel");
  ui->AddLabel(wnd, "Zeal_FirstPersonLabel_X");
  ui->AddLabel(wnd, "Zeal_FirstPersonLabel_Y");
  ui->AddLabel(wnd, "Zeal_ThirdPersonLabel_X");
  ui->AddLabel(wnd, "Zeal_ThirdPersonLabel_Y");
  ui->AddLabel(wnd, "Zeal_FoVValueLabel");
}

void ui_options::InitMap() {
  if (!wnd) return;
  ui->AddCheckboxCallback(wnd, "Zeal_Map", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->zone_map->set_enabled(wnd->Checked, true);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_MapAutoFadeEnable", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->zone_map->set_default_to_zlevel_autofade(wnd->Checked, true);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_MapInteractiveEnable", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->zone_map->set_interactive_enable(wnd->Checked, true);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_MapExternalWindow", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->zone_map->set_external_enable(wnd->Checked, true);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_MapShowRaid", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->zone_map->set_show_raid(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_MapShowGrid", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->zone_map->set_show_grid(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_MapAddLocText", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->zone_map->setting_add_loc_text.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_MapShowPlayerHeadings", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->zone_map->setting_show_all_player_headings.set(wnd->Checked);
  });
  ui->AddComboCallback(wnd, "Zeal_MapShowGroup_Combobox", [this](Zeal::GameUI::BasicWnd *wnd, int value) {
    ZealService::get_instance()->zone_map->set_show_group_mode(value);
  });
  ui->AddComboCallback(wnd, "Zeal_MapBackground_Combobox", [this](Zeal::GameUI::BasicWnd *wnd, int value) {
    ZealService::get_instance()->zone_map->set_background(value);
  });
  ui->AddComboCallback(wnd, "Zeal_MapAlignment_Combobox", [this](Zeal::GameUI::BasicWnd *wnd, int value) {
    ZealService::get_instance()->zone_map->set_alignment(value);
  });
  ui->AddComboCallback(wnd, "Zeal_MapLabels_Combobox", [this](Zeal::GameUI::BasicWnd *wnd, int value) {
    ZealService::get_instance()->zone_map->set_labels_mode(value);
  });
  ui->AddComboCallback(wnd, "Zeal_MapDataMode_Combobox", [this](Zeal::GameUI::BasicWnd *wnd, int value) {
    ZealService::get_instance()->zone_map->set_map_data_mode(value);
  });
  ui->AddComboCallback(wnd, "Zeal_MapZoomDefault_Combobox", [this](Zeal::GameUI::BasicWnd *wnd, int value) {
    ZealService::get_instance()->zone_map->set_map_zoom_default_index(value);
  });

  ui->AddComboCallback(wnd, "Zeal_MapFont_Combobox", [this](Zeal::GameUI::BasicWnd *wnd, int value) {
    std::string font_name("");
    if (value > 0)  // Note: Assuming first value is the default, which "" selects anyways.
      font_name = wnd->CmbListWnd->GetItemText(value, 0);
    ZealService::get_instance()->zone_map->set_font(font_name);
  });

  ui->AddSliderCallback(wnd, "Zeal_MapZoom_Slider", [this](Zeal::GameUI::SliderWnd *wnd, int value) {
    ZealService::get_instance()->zone_map->set_zoom(value * 15 + 100);  // Note scale and zoom offset.
  });
  ui->AddSliderCallback(wnd, "Zeal_MapPositionSize_Slider", [this](Zeal::GameUI::SliderWnd *wnd, int value) {
    ZealService::get_instance()->zone_map->set_position_size(value);
  });
  ui->AddSliderCallback(wnd, "Zeal_MapMarkerSize_Slider", [this](Zeal::GameUI::SliderWnd *wnd, int value) {
    ZealService::get_instance()->zone_map->set_marker_size(value);
  });
  ui->AddSliderCallback(wnd, "Zeal_MapBackgroundAlpha_Slider", [this](Zeal::GameUI::SliderWnd *wnd, int value) {
    ZealService::get_instance()->zone_map->set_background_alpha(value);
  });
  ui->AddSliderCallback(wnd, "Zeal_MapFadedZLevelAlpha_Slider", [this](Zeal::GameUI::SliderWnd *wnd, int value) {
    ZealService::get_instance()->zone_map->set_faded_zlevel_alpha(value);
  });
  ui->AddSliderCallback(wnd, "Zeal_MapNamesLength_Slider", [this](Zeal::GameUI::SliderWnd *wnd, int value) {
    ZealService::get_instance()->zone_map->set_name_length(value * ZoneMap::kMaxNameLength / 100);
  });
  ui->AddSliderCallback(wnd, "Zeal_MapGridPitch_Slider", [this](Zeal::GameUI::SliderWnd *wnd, int value) {
    ZealService::get_instance()->zone_map->set_grid_pitch(value * ZoneMap::kMaxGridPitch / 100);
  });
  ui->AddLabel(wnd, "Zeal_MapZoom_Value");
  ui->AddLabel(wnd, "Zeal_MapPositionSize_Value");
  ui->AddLabel(wnd, "Zeal_MapMarkerSize_Value");
  ui->AddLabel(wnd, "Zeal_MapBackgroundAlpha_Value");
  ui->AddLabel(wnd, "Zeal_MapFadedZLevelAlpha_Value");
  ui->AddLabel(wnd, "Zeal_MapNamesLength_Value");
  ui->AddLabel(wnd, "Zeal_MapGridPitch_Value");
}

void ui_options::InitTargetRing() {
  if (!wnd) return;
  ui->AddLabel(wnd, "Zeal_TargetRingFill_Value");
  ui->AddLabel(wnd, "Zeal_TargetRingSize_Value");
  ui->AddLabel(wnd, "Zeal_TargetRingRotation_Value");
  ui->AddLabel(wnd, "Zeal_TargetRingSegments_Value");
  ui->AddLabel(wnd, "Zeal_TargetRingFlash_Value");
  ui->AddLabel(wnd, "Zeal_TargetRingTransparency_Value");

  ui->AddCheckboxCallback(wnd, "Zeal_TargetRing", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->target_ring->enabled.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_TargetRingHideWithGui", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->target_ring->hide_with_gui.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_TargetRingDisableForSelf", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->target_ring->disable_for_self.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_TargetRingAttackIndicator", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->target_ring->attack_indicator.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_TargetRingForward", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->target_ring->rotate_match_heading.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_TargetRingCone", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->target_ring->use_cone.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_TargetRingColor", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->target_ring->target_color.set(wnd->Checked);
  });

  ui->AddComboCallback(wnd, "Zeal_TargetRingTexture_Combobox", [this](Zeal::GameUI::BasicWnd *wnd, int value) {
    std::string texture_name("None");
    if (value >= 0) texture_name = wnd->CmbListWnd->GetItemText(value, 0);

    ZealService::get_instance()->target_ring->texture_name.set(texture_name);
  });

  ui->AddSliderCallback(wnd, "Zeal_TargetRingFlash_Slider", [this](Zeal::GameUI::SliderWnd *wnd, int value) {
    float val = ScaleSliderToFloat(value, 0, 5, wnd);
    ZealService::get_instance()->target_ring->flash_speed.set(val);
    ui->SetLabelValue("Zeal_TargetRingFlash_Value", "%.2f", val);
  });
  ui->AddSliderCallback(wnd, "Zeal_TargetRingFill_Slider", [this](Zeal::GameUI::SliderWnd *wnd, int value) {
    float val = ScaleSliderToFloat(value, 0, 1, wnd);
    ZealService::get_instance()->target_ring->inner_percent.set(val);
    ui->SetLabelValue("Zeal_TargetRingFill_Value", "%.2f", val);
  });
  ui->AddSliderCallback(wnd, "Zeal_TargetRingSize_Slider", [this](Zeal::GameUI::SliderWnd *wnd, int value) {
    // float val = static_cast<float>(value) / 5.0f;
    float val = ScaleSliderToFloat(value, 0, 20, wnd);
    ZealService::get_instance()->target_ring->outer_size.set(val);
    ui->SetLabelValue("Zeal_TargetRingSize_Value", "%.2f", val);
  });
  ui->AddSliderCallback(wnd, "Zeal_TargetRingRotation_Slider", [this](Zeal::GameUI::SliderWnd *wnd, int value) {
    // float val = ((static_cast<float>(value)-50.f)/100.f)*2;
    float val = ScaleSliderToFloat(value, -1, 1, wnd);
    ZealService::get_instance()->target_ring->rotation_speed.set(val);
    ui->SetLabelValue("Zeal_TargetRingRotation_Value", "%.2f", val);
  });
  ui->AddSliderCallback(
      wnd, "Zeal_TargetRingSegments_Slider",
      [this](Zeal::GameUI::SliderWnd *wnd, int value) {
        ZealService::get_instance()->target_ring->num_segments.set(value);
        ui->SetLabelValue("Zeal_TargetRingSegments_Value", "%i", value);
      },
      128);
  ui->AddSliderCallback(
      wnd, "Zeal_TargetRingTransparency_Slider",
      [this](Zeal::GameUI::SliderWnd *wnd, int value) {
        float val = ScaleSliderToFloat(value, 0, 1, wnd);
        ZealService::get_instance()->target_ring->transparency.set(val);
        ui->SetLabelValue("Zeal_TargetRingTransparency_Value", "%.2f", val);
      },
      128);
}

void ui_options::InitNameplate() {
  if (!wnd) return;
  ui->AddCheckboxCallback(wnd, "Zeal_NameplateColors", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->nameplate->setting_colors.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_NameplateConColors", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->nameplate->setting_con_colors.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_NameplateHideSelf", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->nameplate->setting_hide_self.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_NameplateX", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->nameplate->setting_x.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_NameplateHideRaidPets", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->nameplate->setting_hide_raid_pets.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_NameplateShowPetOwnerName", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->nameplate->setting_show_pet_owner_name.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_NameplateCharSelect", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->nameplate->setting_char_select.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_NameplateInlineGuild", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->nameplate->setting_inline_guild.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_NameplateTargetColor", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->nameplate->setting_target_color.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_NameplateTargetMarker", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->nameplate->setting_target_marker.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_NameplateTargetHealth", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->nameplate->setting_target_health.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_NameplateExtended", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->nameplate->setting_extended_nameplate.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_NameplateTargetBlink", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->nameplate->setting_target_blink.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_NameplateAttackOnly", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->nameplate->setting_attack_only.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_NameplateZealFonts", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->nameplate->setting_zeal_fonts.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_NameplateDropShadow", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->nameplate->setting_drop_shadow.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_NameplateHealthBars", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->nameplate->setting_health_bars.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_NameplateManaBars", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->nameplate->setting_mana_bars.set(wnd->Checked);
  });
  ui->AddCheckboxCallback(wnd, "Zeal_NameplateStaminaBars", [](Zeal::GameUI::BasicWnd *wnd) {
    ZealService::get_instance()->nameplate->setting_stamina_bars.set(wnd->Checked);
  });

  ui->AddComboCallback(wnd, "Zeal_NameplateFont_Combobox", [this](Zeal::GameUI::BasicWnd *wnd, int value) {
    std::string font_name("");
    if (value >= 0) font_name = wnd->CmbListWnd->GetItemText(value, 0);
    ZealService::get_instance()->nameplate->setting_fontname.set(font_name);
  });
}

void ui_options::UpdateOptions() {
  if (!wnd) return;

  UpdateOptionsCamera();
  UpdateOptionsGeneral();
  UpdateOptionsTargetRing();
  UpdateOptionsNameplate();
  UpdateOptionsMap();
  UpdateOptionsFloatingDamage();
}

void ui_options::UpdateOptionsGeneral() {
  if (!wnd) return;

  int fps_limit_selection = 0;
  switch (ZealService::get_instance()->dx->fps_limit.get()) {
    case 0:
      fps_limit_selection = 0;
      break;
    case 30:
      fps_limit_selection = 1;
      break;
    case 60:
      fps_limit_selection = 2;
      break;
    case 120:
      fps_limit_selection = 3;
      break;
    case 144:
      fps_limit_selection = 4;
      break;
    case 165:
      fps_limit_selection = 5;
      break;
    case 240:
      fps_limit_selection = 6;
      break;
    default:
      fps_limit_selection = 0;
      break;
  }
  ui->SetComboValue("Zeal_FPS_Combobox", fps_limit_selection);
  ui->SetComboValue("Zeal_Timestamps_Combobox", ZealService::get_instance()->chat_hook->TimeStampsStyle.get());
  ui->SetSliderValue("Zeal_HoverTimeout_Slider", ZealService::get_instance()->tooltips->hover_timeout.get() > 0
                                                     ? ZealService::get_instance()->tooltips->hover_timeout.get() / 5
                                                     : 0);
  ui->SetLabelValue("Zeal_HoverTimeout_Value", "%d ms", ZealService::get_instance()->tooltips->hover_timeout.get());
  ui->SetChecked("Zeal_HideCorpse", ZealService::get_instance()->looting_hook->hide_looted);
  ui->SetChecked("Zeal_TellWindows", ZealService::get_instance()->tells->enabled);
  ui->SetChecked("Zeal_TellWindowsHist", ZealService::get_instance()->tells->hist_enabled);
  ui->SetChecked("Zeal_LinkAllAltDelimiter", ZealService::get_instance()->looting_hook->setting_alt_delimiter.get());
  ui->SetChecked("Zeal_CtrlRightClickCorpse",
                 ZealService::get_instance()->looting_hook->setting_ctrl_rightclick_loot.get());
  ui->SetChecked("Zeal_CtrlContextMenus", ZealService::get_instance()->ui->options->setting_ctrl_context_menus.get());
  ui->SetChecked("Zeal_EnableContainerLock",
                 ZealService::get_instance()->ui->options->setting_enable_container_lock.get());
  ui->SetChecked("Zeal_ExportOnCamp", ZealService::get_instance()->outputfile->setting_export_on_camp.get());
  ui->SetChecked("Zeal_SelfClickThru", ZealService::get_instance()->camera_mods->setting_selfclickthru.get());
  ui->SetChecked("Zeal_ClassicClasses", ZealService::get_instance()->chat_hook->UseClassicClassNames.get());
  ui->SetLabelValue("Zeal_VersionValue", "%s (%s)", ZEAL_VERSION, ZEAL_BUILD_VERSION);
  ui->SetChecked("Zeal_BlueCon", ZealService::get_instance()->chat_hook->UseBlueCon.get());
  ui->SetChecked("Zeal_Timestamp", ZealService::get_instance()->chat_hook->TimeStampsStyle.get());
  ui->SetChecked("Zeal_Input", ZealService::get_instance()->chat_hook->UseZealInput.get());
  ui->SetChecked("Zeal_Escape", ZealService::get_instance()->ini->getValue<bool>("Zeal", "Escape"));
  ui->SetChecked("Zeal_ShowHelm", ZealService::get_instance()->helm->ShowHelmEnabled.get());
  ui->SetChecked("Zeal_AltContainerTooltips", ZealService::get_instance()->tooltips->all_containers.get());
  ui->SetChecked("Zeal_SpellbookAutoStand", ZealService::get_instance()->movement->SpellBookAutoStand.get());
  ui->SetChecked("Zeal_CastAutoStand", ZealService::get_instance()->movement->CastAutoStand.get());
  ui->SetChecked("Zeal_RightClickToEquip", ZealService::get_instance()->equip_item_hook->Enabled.get());
  ui->SetChecked("Zeal_BuffTimers", ZealService::get_instance()->ui->buffs->BuffTimers.get());
  ui->SetChecked("Zeal_RecastTimers", ZealService::get_instance()->ui->buffs->RecastTimers.get());
  ui->SetChecked("Zeal_RecastTimersLeftAlign", ZealService::get_instance()->ui->buffs->RecastTimersLeftAlign.get());
  ui->SetChecked("Zeal_BrownSkeletons", ZealService::get_instance()->game_patches->BrownSkeletons.get());
  ui->SetChecked("Zeal_ClassicMusic", ZealService::get_instance()->music->ClassicMusic.get());
  ui->SetChecked("Zeal_SuppressMissedNotes",
                 ZealService::get_instance()->chatfilter_hook->setting_suppress_missed_notes.get());
  ui->SetChecked("Zeal_SuppressOtherFizzles",
                 ZealService::get_instance()->chatfilter_hook->setting_suppress_other_fizzles.get());
  ui->SetChecked("Zeal_UseZealAssistOn", ZealService::get_instance()->assist->setting_use_zeal_assist_on.get());
  ui->SetChecked("Zeal_DetectAssistFailure", ZealService::get_instance()->assist->setting_detect_assist_failure.get());
  ui->SetChecked("Zeal_SingleClickGiveEnable", ZealService::get_instance()->give->setting_enable_give.get());
  ui->SetChecked("Zeal_EnhancedSpellInfo",
                 ZealService::get_instance()->item_displays->setting_enhanced_spell_info.get());
  ui->SetChecked("Zeal_SlashNotPoke", setting_slash_not_poke.get());
  ui->SetChecked("Zeal_InviteDialog", setting_invite_dialog.get());
  ui->SetChecked("Zeal_AutoFollowEnable", ZealService::get_instance()->game_patches->AutoFollowEnable.get());

  UpdateComboBox("Zeal_TellSound_Combobox", setting_tell_sound.get(), kDefaultSoundNone);
  UpdateComboBox("Zeal_InviteSound_Combobox", setting_invite_sound.get(), kDefaultSoundNone);
}

void ui_options::UpdateOptionsCamera() {
  if (!wnd) return;

  ui->SetChecked("Zeal_Cam_TurnLocked", ZealService::get_instance()->camera_mods->cam_lock.get());
  ui->SetSliderValue("Zeal_PanDelaySlider", ZealService::get_instance()->camera_mods->pan_delay.get() > 0.f
                                                ? ZealService::get_instance()->camera_mods->pan_delay.get() / 4
                                                : 0.f);
  ui->SetSliderValue("Zeal_ThirdPersonSlider_Y",
                     GetSensitivityForSlider(ZealService::get_instance()->camera_mods->user_sensitivity_y_3rd));
  ui->SetSliderValue("Zeal_ThirdPersonSlider_X",
                     GetSensitivityForSlider(ZealService::get_instance()->camera_mods->user_sensitivity_x_3rd));
  ui->SetSliderValue("Zeal_FirstPersonSlider_Y",
                     GetSensitivityForSlider(ZealService::get_instance()->camera_mods->user_sensitivity_y));
  ui->SetSliderValue("Zeal_FirstPersonSlider_X",
                     GetSensitivityForSlider(ZealService::get_instance()->camera_mods->user_sensitivity_x));
  ui->SetSliderValue("Zeal_FoVSlider",
                     static_cast<int>((ZealService::get_instance()->camera_mods->fov.get() - 45.0f) / 45.0f * 100.0f));
  ui->SetLabelValue("Zeal_FoVValueLabel", "%.0f", ZealService::get_instance()->camera_mods->fov.get());
  ui->SetLabelValue("Zeal_FirstPersonLabel_X", "%.2f",
                    ZealService::get_instance()->camera_mods->user_sensitivity_x.get());
  ui->SetLabelValue("Zeal_FirstPersonLabel_Y", "%.2f",
                    ZealService::get_instance()->camera_mods->user_sensitivity_y.get());
  ui->SetLabelValue("Zeal_ThirdPersonLabel_X", "%.2f",
                    ZealService::get_instance()->camera_mods->user_sensitivity_x_3rd.get());
  ui->SetLabelValue("Zeal_ThirdPersonLabel_Y", "%.2f",
                    ZealService::get_instance()->camera_mods->user_sensitivity_y_3rd.get());
  ui->SetLabelValue("Zeal_PanDelayValueLabel", "%d ms", ZealService::get_instance()->camera_mods->pan_delay.get());
  ui->SetChecked("Zeal_Cam", ZealService::get_instance()->camera_mods->enabled.get());
  ui->SetChecked("Zeal_UseOldSens", ZealService::get_instance()->camera_mods->use_old_sens.get());
}

void ui_options::UpdateOptionsTargetRing() {
  if (!wnd) return;

  ui->SetChecked("Zeal_TargetRing", ZealService::get_instance()->target_ring->enabled.get());
  ui->SetChecked("Zeal_TargetRingDisableForSelf", ZealService::get_instance()->target_ring->disable_for_self.get());
  ui->SetChecked("Zeal_TargetRingAttackIndicator", ZealService::get_instance()->target_ring->attack_indicator.get());
  ui->SetChecked("Zeal_TargetRingForward", ZealService::get_instance()->target_ring->rotate_match_heading.get());
  ui->SetChecked("Zeal_TargetRingCone", ZealService::get_instance()->target_ring->use_cone.get());
  ui->SetChecked("Zeal_TargetRingColor", ZealService::get_instance()->target_ring->target_color.get());
  ui->SetSliderValue("Zeal_TargetRingFill_Slider",
                     ScaleFloatToSlider(ZealService::get_instance()->target_ring->inner_percent.get(), 0, 1,
                                        ui->GetSlider("Zeal_TargetRingFill_Slider")));
  ui->SetSliderValue("Zeal_TargetRingSize_Slider",
                     ScaleFloatToSlider(ZealService::get_instance()->target_ring->outer_size.get(), 0, 20,
                                        ui->GetSlider("Zeal_TargetRingSize_Slider")));
  ui->SetSliderValue("Zeal_TargetRingRotation_Slider",
                     ScaleFloatToSlider(ZealService::get_instance()->target_ring->rotation_speed.get(), -1, 1,
                                        ui->GetSlider("Zeal_TargetRingSize_Slider")));
  ui->SetSliderValue("Zeal_TargetRingFlash_Slider",
                     ScaleFloatToSlider(ZealService::get_instance()->target_ring->flash_speed.get(), 1, 5,
                                        ui->GetSlider("Zeal_TargetRingFlash_Slider")));
  ui->SetSliderValue("Zeal_TargetRingSegments_Slider",
                     static_cast<int>(ZealService::get_instance()->target_ring->num_segments.get()));
  ui->SetSliderValue("Zeal_TargetRingTransparency_Slider",
                     ScaleFloatToSlider(ZealService::get_instance()->target_ring->transparency.get(), 0, 1,
                                        ui->GetSlider("Zeal_TargetRingTransparency_Slider")));
  ui->SetLabelValue("Zeal_TargetRingFlash_Value", "%.2f", ZealService::get_instance()->target_ring->flash_speed.get());
  ui->SetLabelValue("Zeal_TargetRingFill_Value", "%.2f", ZealService::get_instance()->target_ring->inner_percent.get());
  ui->SetLabelValue("Zeal_TargetRingSegments_Value", "%i",
                    ZealService::get_instance()->target_ring->num_segments.get());
  ui->SetLabelValue("Zeal_TargetRingRotation_Value", "%.2f",
                    ZealService::get_instance()->target_ring->rotation_speed.get());
  ui->SetLabelValue("Zeal_TargetRingSize_Value", "%.2f", ZealService::get_instance()->target_ring->outer_size.get());
  ui->SetLabelValue("Zeal_TargetRingTransparency_Value", "%.2f",
                    ZealService::get_instance()->target_ring->transparency.get());

  std::string current_texture = ZealService::get_instance()->target_ring->texture_name.get();
  UpdateComboBox("Zeal_TargetRingTexture_Combobox", current_texture, "None");
}

void ui_options::UpdateOptionsNameplate() {
  if (!wnd) return;

  ui->SetChecked("Zeal_NameplateColors", ZealService::get_instance()->nameplate->setting_colors.get());
  ui->SetChecked("Zeal_NameplateConColors", ZealService::get_instance()->nameplate->setting_con_colors.get());
  ui->SetChecked("Zeal_NameplateHideSelf", ZealService::get_instance()->nameplate->setting_hide_self.get());
  ui->SetChecked("Zeal_NameplateX", ZealService::get_instance()->nameplate->setting_x.get());
  ui->SetChecked("Zeal_NameplateHideRaidPets", ZealService::get_instance()->nameplate->setting_hide_raid_pets.get());
  ui->SetChecked("Zeal_NameplateShowPetOwnerName",
                 ZealService::get_instance()->nameplate->setting_show_pet_owner_name.get());
  ui->SetChecked("Zeal_NameplateCharSelect", ZealService::get_instance()->nameplate->setting_char_select.get());
  ui->SetChecked("Zeal_NameplateInlineGuild", ZealService::get_instance()->nameplate->setting_inline_guild.get());
  ui->SetChecked("Zeal_NameplateTargetColor", ZealService::get_instance()->nameplate->setting_target_color.get());
  ui->SetChecked("Zeal_NameplateTargetMarker", ZealService::get_instance()->nameplate->setting_target_marker.get());
  ui->SetChecked("Zeal_NameplateTargetHealth", ZealService::get_instance()->nameplate->setting_target_health.get());
  ui->SetChecked("Zeal_NameplateExtended", ZealService::get_instance()->nameplate->setting_extended_nameplate.get());
  ui->SetChecked("Zeal_NameplateTargetBlink", ZealService::get_instance()->nameplate->setting_target_blink.get());
  ui->SetChecked("Zeal_NameplateAttackOnly", ZealService::get_instance()->nameplate->setting_attack_only.get());
  ui->SetChecked("Zeal_NameplateZealFonts", ZealService::get_instance()->nameplate->setting_zeal_fonts.get());
  ui->SetChecked("Zeal_NameplateDropShadow", ZealService::get_instance()->nameplate->setting_drop_shadow.get());
  ui->SetChecked("Zeal_NameplateHealthBars", ZealService::get_instance()->nameplate->setting_health_bars.get());
  ui->SetChecked("Zeal_NameplateManaBars", ZealService::get_instance()->nameplate->setting_mana_bars.get());
  ui->SetChecked("Zeal_NameplateStaminaBars", ZealService::get_instance()->nameplate->setting_stamina_bars.get());

  std::string current_font = ZealService::get_instance()->nameplate->setting_fontname.get();
  UpdateComboBox("Zeal_NameplateFont_Combobox", current_font, BitmapFont::kDefaultFontName);
}

void ui_options::UpdateOptionsFloatingDamage() {
  ui->SetChecked("Zeal_FloatingDamage", ZealService::get_instance()->floating_damage->enabled.get());
  ui->SetChecked("Zeal_FloatingSelf", ZealService::get_instance()->floating_damage->show_self.get());
  ui->SetChecked("Zeal_FloatingPets", ZealService::get_instance()->floating_damage->show_pets.get());
  ui->SetChecked("Zeal_FloatingOthers", ZealService::get_instance()->floating_damage->show_others.get());
  ui->SetChecked("Zeal_FloatingNpcs", ZealService::get_instance()->floating_damage->show_npcs.get());
  ui->SetChecked("Zeal_FloatingHpUpdates", ZealService::get_instance()->floating_damage->show_hp_updates.get());
  ui->SetChecked("Zeal_FloatingMelee", ZealService::get_instance()->floating_damage->show_melee.get());
  ui->SetChecked("Zeal_FloatingSpells", ZealService::get_instance()->floating_damage->show_spells.get());
  ui->SetChecked("Zeal_FloatingSpellIcons", ZealService::get_instance()->floating_damage->spell_icons.get());
  int big_hit_threshold = ZealService::get_instance()->floating_damage->big_hit_threshold.get();
  ui->SetLabelValue("Zeal_FloatingBigHit_Value", "%i", big_hit_threshold);
  ui->SetSliderValue("Zeal_FloatingBigHit_Slider", big_hit_threshold / 10);

  std::string current_font = ZealService::get_instance()->floating_damage->bitmap_font_filename.get();
  UpdateComboBox("Zeal_FloatingFont_Combobox", current_font, FloatingDamage::kUseClientFontString);
}

void ui_options::UpdateOptionsMap() {
  if (!wnd) return;

  ui->SetChecked("Zeal_Map", ZealService::get_instance()->zone_map->is_enabled());
  ui->SetChecked("Zeal_MapAutoFadeEnable", ZealService::get_instance()->zone_map->is_default_zlevel_autofade());
  ui->SetChecked("Zeal_MapInteractiveEnable", ZealService::get_instance()->zone_map->is_interactive_enabled());
  ui->SetChecked("Zeal_MapExternalWindow", ZealService::get_instance()->zone_map->is_external_enabled());
  ui->SetChecked("Zeal_MapShowRaid", ZealService::get_instance()->zone_map->is_show_raid_enabled());
  ui->SetChecked("Zeal_MapShowGrid", ZealService::get_instance()->zone_map->is_show_grid_enabled());
  ui->SetChecked("Zeal_MapAddLocText", ZealService::get_instance()->zone_map->setting_add_loc_text.get());
  ui->SetChecked("Zeal_MapShowPlayerHeadings",
                 ZealService::get_instance()->zone_map->setting_show_all_player_headings.get());
  ui->SetComboValue("Zeal_MapShowGroup_Combobox", ZealService::get_instance()->zone_map->get_show_group_mode());
  ui->SetComboValue("Zeal_MapBackground_Combobox", ZealService::get_instance()->zone_map->get_background());
  ui->SetComboValue("Zeal_MapAlignment_Combobox", ZealService::get_instance()->zone_map->get_alignment());
  ui->SetComboValue("Zeal_MapLabels_Combobox", ZealService::get_instance()->zone_map->get_labels_mode());
  ui->SetComboValue("Zeal_MapDataMode_Combobox", ZealService::get_instance()->zone_map->get_map_data_mode());
  ui->SetComboValue("Zeal_MapZoomDefault_Combobox",
                    ZealService::get_instance()->zone_map->get_map_zoom_default_index());
  ui->SetSliderValue("Zeal_MapZoom_Slider",
                     (ZealService::get_instance()->zone_map->get_zoom() - 100) / 15);  // 100 to 1600%
  ui->SetSliderValue("Zeal_MapPositionSize_Slider", ZealService::get_instance()->zone_map->get_position_size());
  ui->SetSliderValue("Zeal_MapMarkerSize_Slider", ZealService::get_instance()->zone_map->get_marker_size());
  ui->SetSliderValue("Zeal_MapBackgroundAlpha_Slider", ZealService::get_instance()->zone_map->get_background_alpha());
  ui->SetSliderValue("Zeal_MapFadedZLevelAlpha_Slider",
                     ZealService::get_instance()->zone_map->get_faded_zlevel_alpha());
  ui->SetSliderValue("Zeal_MapNamesLength_Slider",
                     ZealService::get_instance()->zone_map->get_name_length() * 100 / ZoneMap::kMaxNameLength);
  ui->SetSliderValue("Zeal_MapGridPitch_Slider",
                     ZealService::get_instance()->zone_map->get_grid_pitch() * 100 / ZoneMap::kMaxGridPitch);
  ui->SetLabelValue("Zeal_MapZoom_Value", "%i%%", ZealService::get_instance()->zone_map->get_zoom());
  ui->SetLabelValue("Zeal_MapPositionSize_Value", "%i%%", ZealService::get_instance()->zone_map->get_position_size());
  ui->SetLabelValue("Zeal_MapMarkerSize_Value", "%i%%", ZealService::get_instance()->zone_map->get_marker_size());
  ui->SetLabelValue("Zeal_MapBackgroundAlpha_Value", "%i%%",
                    ZealService::get_instance()->zone_map->get_background_alpha());
  ui->SetLabelValue("Zeal_MapFadedZLevelAlpha_Value", "%i%%",
                    ZealService::get_instance()->zone_map->get_faded_zlevel_alpha());
  ui->SetLabelValue("Zeal_MapNamesLength_Value", "%i", ZealService::get_instance()->zone_map->get_name_length());
  ui->SetLabelValue("Zeal_MapGridPitch_Value", "%i", ZealService::get_instance()->zone_map->get_grid_pitch());

  std::string current_font = ZealService::get_instance()->zone_map->get_font();
  UpdateComboBox("Zeal_MapFont_Combobox", current_font, BitmapFont::kDefaultFontName);
}

void ui_options::RenderUI() {
  if (!wnd || !Zeal::Game::Windows || !Zeal::Game::Windows->Options) return;

  if (wnd->IsVisible != Zeal::Game::Windows->Options->IsVisible) {
    if (Zeal::Game::Windows->Options->IsVisible) {
      UpdateDynamicUI();
      UpdateOptions();
    }
    wnd->show(Zeal::Game::Windows->Options->IsVisible, false);
  }
}

int ui_options::FindComboIndex(std::string combobox, std::string text_value) {
  if (!wnd) return -1;

  Zeal::GameUI::ComboWnd *cmb = (Zeal::GameUI::ComboWnd *)wnd->GetChildItem(combobox.c_str());
  if (!cmb) return -1;

  int value = 0;
  for (int value = 0; value < kMaxComboBoxItems; ++value) {
    auto value_label = cmb->CmbListWnd->GetItemText(value, 0);  // Assumption: value > rows is safe.
    if (value_label.empty()) return -1;                         // End of list.
    if (Zeal::String::compare_insensitive(value_label, text_value)) return value;
  }
  return -1;
}

void ui_options::UpdateComboBox(const std::string &name, const std::string &label, const std::string &default_label) {
  int index = FindComboIndex(name, label.empty() ? default_label : label);
  if (index < 0) index = FindComboIndex(name, default_label);
  ui->SetComboValue(name, max(0, index));
}

void ui_options::UpdateDynamicUI() {
  if (!wnd) return;

  Zeal::GameUI::ComboWnd *cmb = (Zeal::GameUI::ComboWnd *)wnd->GetChildItem("Zeal_TargetRingTexture_Combobox");
  if (cmb) {
    std::vector<std::string> textures = ZealService::get_instance()->target_ring->get_available_textures();
    cmb->DeleteAll();
    if (textures.size() > kMaxComboBoxItems) textures.resize(kMaxComboBoxItems);
    ZealService::get_instance()->ui->AddListItems(cmb, textures);
  }

  cmb = (Zeal::GameUI::ComboWnd *)wnd->GetChildItem("Zeal_MapFont_Combobox");
  if (cmb) {
    std::vector<std::string> fonts = ZealService::get_instance()->zone_map->get_available_fonts();
    if (fonts.size() > kMaxComboBoxItems) fonts.resize(kMaxComboBoxItems);
    cmb->DeleteAll();
    ZealService::get_instance()->ui->AddListItems(cmb, fonts);
  }

  cmb = (Zeal::GameUI::ComboWnd *)wnd->GetChildItem("Zeal_FloatingFont_Combobox");
  if (cmb) {
    std::vector<std::string> fonts = ZealService::get_instance()->floating_damage->get_available_fonts();
    if (fonts.size() > kMaxComboBoxItems) fonts.resize(kMaxComboBoxItems);
    cmb->DeleteAll();
    ZealService::get_instance()->ui->AddListItems(cmb, fonts);
  }

  cmb = (Zeal::GameUI::ComboWnd *)wnd->GetChildItem("Zeal_NameplateFont_Combobox");
  if (cmb) {
    std::vector<std::string> fonts = ZealService::get_instance()->nameplate->get_available_fonts();
    if (fonts.size() > kMaxComboBoxItems) fonts.resize(kMaxComboBoxItems);
    cmb->DeleteAll();
    ZealService::get_instance()->ui->AddListItems(cmb, fonts);
  }

  cmb = (Zeal::GameUI::ComboWnd *)wnd->GetChildItem("Zeal_TellSound_Combobox");
  if (cmb) {
    std::vector<std::string> sounds;
    for (const auto &pair : sound_list) sounds.push_back(pair.second);
    if (sounds.size() > kMaxComboBoxItems) sounds.resize(kMaxComboBoxItems);
    cmb->DeleteAll();
    ZealService::get_instance()->ui->AddListItems(cmb, sounds);
  }

  cmb = (Zeal::GameUI::ComboWnd *)wnd->GetChildItem("Zeal_InviteSound_Combobox");
  if (cmb) {
    std::vector<std::string> sounds;
    for (const auto &pair : sound_list) sounds.push_back(pair.second);
    if (sounds.size() > kMaxComboBoxItems) sounds.resize(kMaxComboBoxItems);
    cmb->DeleteAll();
    ZealService::get_instance()->ui->AddListItems(cmb, sounds);
  }
}

void ui_options::CleanDynamicUI() {
  if (!wnd) return;

  std::vector<std::string> box_list = {"Zeal_TargetRingTexture_Combobox", "Zeal_MapFont_Combobox",
                                       "Zeal_FloatingFont_Combobox",      "Zeal_NameplateFont_Combobox",
                                       "Zeal_TellSound_Combobox",         "Zeal_InviteSound_Combobox"};
  for (const auto &box_name : box_list) {
    Zeal::GameUI::ComboWnd *cmb = (Zeal::GameUI::ComboWnd *)wnd->GetChildItem(box_name.c_str());
    if (cmb) {
      cmb->CmbListWnd->SelectedIndex = -1;
      cmb->DeleteAll();
    }
  }
}

void ui_options::CleanUI() {
  if (!wnd) return;

  // We assume that deactivate_ui() was called by the framework already (so not needed here).
  color_buttons.clear();
  CleanDynamicUI();  // The destructor below may handle these children, but just in case clean up.
  ZealService::get_instance()->ui->DestroySidlScreenWnd(wnd);
  wnd = nullptr;
}

void ui_options::Deactivate() {
  if (!wnd) return;

  wnd->show(0, false);
}

// Setting to allow locking bag container windows. This must be done by the SetContainer() call
// which calls EnableIniStorage() that will reset the lock flag in the ini file if !LockEnable.
static void __fastcall ContainerWndSetContainer(Zeal::GameUI::ContainerWnd *wnd, int unused_edx, void *game_container,
                                                int type) {
  wnd->LockEnable = ZealService::get_instance()->ui->options->setting_enable_container_lock.get();
  ZealService::get_instance()->hooks->hook_map["ContainerWndSetContainer"]->original(ContainerWndSetContainer)(
      wnd, unused_edx, game_container, type);
}

static int __fastcall SidlScreenWndHandleRButtonDown(Zeal::GameUI::SidlWnd *wnd, int unused_edx, int mouse_x,
                                                     int mouse_y, unsigned int unknown3) {
  if (ZealService::get_instance()->ui->options->setting_ctrl_context_menus.get()) {
    if (!Zeal::Game::get_wnd_manager()->ControlKeyState && Zeal::Game::Windows != nullptr && wnd &&
        wnd->ContextMenu > -1)
      return 0;  // Bail out to skip popping up the context menu below.
  }

  return ZealService::get_instance()->hooks->hook_map["SidlScreenWndHandleRButtonDown"]->original(
      SidlScreenWndHandleRButtonDown)(wnd, unused_edx, mouse_x, mouse_y, unknown3);
}

// Support modifying / substituting animations.
static bool handle_animation_packet(Zeal::Packets::Animation_Struct *animation) {
  const auto char_info = Zeal::Game::get_char_info();

  // Allow changing the default piercing-like 2HB anim with the 2HS one.
  auto action = static_cast<Zeal::GameEnums::DoAnimation>(animation->action);
  if (char_info && action == Zeal::GameEnums::DoAnimation::Weapon2H &&
      ZealService::get_instance()->ui->options->setting_slash_not_poke.get()) {
    const auto weapon = char_info->InventoryItem[Zeal::GameEnums::EquipSlot::Primary];
    if (Zeal::Game::get_weapon_skill(weapon) == Zeal::GameEnums::SkillType::Skill2HBlunt)
      animation->action = static_cast<BYTE>(Zeal::GameEnums::DoAnimation::Slashing2H);
  }

  return false;  // Continue processing of the possibly modified animation.
}

ui_options::ui_options(ZealService *zeal, UIManager *mgr) : ui(mgr) {
  wnd = nullptr;
  zeal->callbacks->AddGeneric([this]() { CleanUI(); }, callback_type::CleanUI);
  zeal->callbacks->AddGeneric([this]() { InitUI(); }, callback_type::InitUI);
  zeal->callbacks->AddGeneric([this]() { RenderUI(); }, callback_type::RenderUI);
  zeal->callbacks->AddGeneric([this]() { Deactivate(); }, callback_type::DeactivateUI);
  ui->AddXmlInclude("EQUI_ZealOptions.xml");

  zeal->hooks->Add("ContainerWndSetContainer", 0x0041717d, ContainerWndSetContainer, hook_type_detour);
  zeal->hooks->Add("SidlScreenWndHandleRButtonDown", 0x005703f0, SidlScreenWndHandleRButtonDown, hook_type_detour);

  zeal->callbacks->AddOutputText([this](Zeal::GameUI::ChatWnd *&wnd, const std::string &msg, const short &channel) {
    if (channel == USERCOLOR_TELL) this->PlayTellSound();
  });

  zeal->hooks->Add("RaidHandleCreateInviteRaid", 0x0049e54c, RaidHandleCreateInviteRaid, hook_type_detour);
  zeal->hooks->Add("RaidSendInviteResponse", 0x0049debd, RaidSendInviteResponse, hook_type_detour);
  zeal->hooks->Add("GamePlayerSetInvited", 0x0050c216, GamePlayerSetInvited, hook_type_detour);
  sound_list.push_back({-1, kDefaultSoundNone});
  sound_list.push_back({137, "Gate"});
  sound_list.push_back({138, "Ka-ching!"});
  sound_list.push_back({139, "Ding!"});
  sound_list.push_back({140, "Zoom"});
  sound_list.push_back({141, "Trumpet"});
  sound_list.push_back({143, "Lightning1"});
  sound_list.push_back({144, "Lightning2"});
  sound_list.push_back({99, "Fishing"});
  sound_list.push_back({116, "Arrow"});
  sound_list.push_back({64, "Uh"});
  sound_list.push_back({66, "Aa-uh"});
  sound_list.push_back({76, "Oof"});
  sound_list.push_back({41, "Gurgle"});
  sound_list.push_back({134, "OpenBox"});
  sound_list.push_back({145, "OpenBag"});

  // Support swapping animations (like using 2hs for a 2hb weapon).
  zeal->callbacks->AddPacket([this](UINT opcode, char *buffer, UINT len) {
    if (opcode == Zeal::Packets::Animation && len == sizeof(Zeal::Packets::Animation_Struct))
      return handle_animation_packet(reinterpret_cast<Zeal::Packets::Animation_Struct *>(buffer));
    return false;  // continue processing
  });
}

ui_options::~ui_options() {}

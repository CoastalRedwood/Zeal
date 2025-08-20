#include "ui_inputdialog.h"

#include "commands.h"
#include "game_functions.h"
#include "hook_wrapper.h"
#include "string_util.h"
#include "ui_manager.h"
#include "ui_skin.h"
#include "zeal.h"

bool ui_inputdialog::isVisible() {
  if (!wnd) return false;
  return wnd->IsVisible;
}

void ui_inputdialog::hide() {
  if (wnd) wnd->show(0, false);
}

bool ui_inputdialog::show(const std::string &title, const std::string &message, const std::string &button1_name,
                          const std::string button2_name, InputDialogCallback button1_callback,
                          InputDialogCallback button2_callback, bool show_input_field) {
  if (!wnd || wnd->IsVisible) return false;
  button_callbacks = {button1_callback, button2_callback};
  if (button1) {
    button1->IsVisible = button1_name.length() > 0;
    button1->Text.Set(button1_name);
  }
  if (button2) {
    button2->IsVisible = button2_name.length() > 0;
    button2->Text.Set(button2_name);
  }

  if (label) label->Text.Set(message);
  if (wnd) wnd->Text.Set(title);
  if (input) {
    input->IsVisible = show_input_field;
    input->SetText("");
  }

  bool bring_to_top = (button1 && button1->IsVisible) || (button2 && button2->IsVisible) || (input && input->IsVisible);
  wnd->show(1, bring_to_top);
  if (input && input->IsVisible) input->SetFocus();
  return true;
}

std::string ui_inputdialog::getTitle() const { return wnd ? std::string(wnd->Text) : std::string(""); }

int __fastcall IDWndNotification(Zeal::GameUI::BasicWnd *wnd, int unused, Zeal::GameUI::BasicWnd *sender, int message,
                                 int data) {
  UIManager *ui = ZealService::get_instance()->ui.get();
  if (sender == ui->inputDialog->button1 || message == 0x6)  // message 6 = enter key
  {
    if (ui->inputDialog->button_callbacks.first)
      ui->inputDialog->button_callbacks.first(ui->inputDialog->input->Text.CastToCharPtr());
    ui->inputDialog->hide();
    return 0;
  }
  if (sender == ui->inputDialog->button2) {
    if (ui->inputDialog->button_callbacks.second)
      ui->inputDialog->button_callbacks.second(ui->inputDialog->input->Text.CastToCharPtr());
    ui->inputDialog->hide();
    return 0;
  }
  return reinterpret_cast<int(__thiscall *)(Zeal::GameUI::BasicWnd * wnd, Zeal::GameUI::BasicWnd * sender, int message,
                                            int data)>(0x56e920)(wnd, sender, message, data);
}

// Avoids storing xml defaults loaded in char select to character UI settings.
static void __fastcall IDStoreIniInfo(Zeal::GameUI::SidlWnd *wnd, int unused) {
  if (!wnd || (Zeal::Game::get_gamestate() != GAMESTATE_INGAME)) return;

  reinterpret_cast<void(__fastcall *)(Zeal::GameUI::SidlWnd * wnd, int unused)>(
      Zeal::GameUI::SidlWnd::GetDefaultVTable()->StoreIniInfo)(wnd, unused);
}

void ui_inputdialog::CleanUI() {
  if (wnd) {
    // Should already be deactivated by the time the framework calls this.
    ui->DestroySidlScreenWnd(wnd);
    wnd = nullptr;
  }
}

void ui_inputdialog::Deactivate() {
  if (wnd) {
    wnd->show(0, false);
  }
}

void ui_inputdialog::InitUI() {
  if (wnd) Zeal::Game::print_chat("Warning: Init out of sync for ui_inputdialog");

  std::filesystem::path xml_file = UISkin::get_zeal_xml_path() / std::filesystem::path("EQUI_ZealInputDialog.xml");
  if (!wnd && ui && std::filesystem::exists(xml_file)) wnd = ui->CreateSidlScreenWnd("ZealInputDialog");

  if (!wnd) {
    Zeal::Game::print_chat("Error: Failed to load %s", xml_file.string().c_str());
    return;
  }

  wnd->vtbl->WndNotification = IDWndNotification;
  button1 = wnd->GetChildItem("ZealDialogButton1");
  button2 = wnd->GetChildItem("ZealDialogButton2");
  label = wnd->GetChildItem("ZealDialogMessage");
  input = (Zeal::GameUI::EditWnd *)wnd->GetChildItem("ZealDialogInput");

  // The charselect is uing the input dialog for zone select before the character
  // (and thus the UI INI filename) is set, causing the saved location to be ignored
  // on load and then wiped when stored.
  auto vtable = static_cast<Zeal::GameUI::SidlScreenWndVTable *>(wnd->vtbl);
  vtable->StoreIniInfo = IDStoreIniInfo;
}

ui_inputdialog::ui_inputdialog(ZealService *zeal, UIManager *mgr) {
  ui = mgr;
  zeal->commands_hook->Add("/testdialog", {}, "test", [this, mgr](std::vector<std::string> &args) {
    if (wnd) {
      show(
          "Spell Sets", "Enter a name for this spellset", "Save", "Cancel",
          [](std::string input_data) { Zeal::Game::print_chat(input_data); }, nullptr);
    }
    return true;
  });
  zeal->callbacks->AddGeneric([this]() { CleanUI(); }, callback_type::CleanUI);
  zeal->callbacks->AddGeneric([this]() { InitUI(); }, callback_type::InitUI);
  zeal->callbacks->AddGeneric([this]() { InitUI(); }, callback_type::InitCharSelectUI);
  zeal->callbacks->AddGeneric([this]() { Deactivate(); }, callback_type::DeactivateUI);
}

ui_inputdialog::~ui_inputdialog() {}

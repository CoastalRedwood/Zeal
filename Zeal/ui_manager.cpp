#include "ui_manager.h"

#include <algorithm>
#include <fstream>

#include "commands.h"
#include "game_addresses.h"
#include "game_functions.h"
#include "game_structures.h"
#include "hook_wrapper.h"
#include "item_display.h"
#include "string_util.h"
#include "ui_skin.h"
#include "zeal.h"
#include "zone_map.h"

Zeal::GameUI::SidlWnd *UIManager::CreateSidlScreenWnd(const std::string &name) {
  Zeal::GameUI::SidlWnd *wnd = (Zeal::GameUI::SidlWnd *)HeapAlloc(*Zeal::Game::Heap, 0, sizeof(Zeal::GameUI::SidlWnd));
  mem::set((int)wnd, 0, sizeof(Zeal::GameUI::SidlWnd));
  Zeal::GameUI::CXSTR name_cxstr(name);  // Constructor calls FreeRep() internally.
  Zeal::Game::GameInternal::CSidlScreenWndConstructor(wnd, 0, nullptr, name_cxstr);
  wnd->SetupCustomVTable();
  wnd->CreateChildren();
  return wnd;
}

// The caller should nullptr the wnd after calling.
void UIManager::DestroySidlScreenWnd(Zeal::GameUI::SidlWnd *wnd) {
  if (!wnd) return;

  // The SidlScreenWndDestructor call below also appears to release all children resources (at 0x005711e0),
  // but probably doesn't handle any resources directly allocated in the custom SidlWnd class.
  wnd->DeleteCustomVTable();
  Zeal::Game::GameInternal::CSidlScreenWndDestructor(wnd, 0, true);  // Set true to dealloc memory.
}

static int __fastcall ButtonClick_hook(Zeal::GameUI::BasicWnd *pWnd, int unused, Zeal::GameUI::CXPoint pt,
                                       unsigned int flag) {
  UIManager *ui = ZealService::get_instance()->ui.get();
  int rval =
      ZealService::get_instance()->hooks->hook_map["ButtonClick"]->original(ButtonClick_hook)(pWnd, unused, pt, flag);
  auto cb = ui->GetButtonCallback(pWnd);
  if (cb) {
    ui->clicked_button = pWnd;
    cb(pWnd);
  }
  return rval;
}

static int __fastcall CheckboxClick_hook(Zeal::GameUI::BasicWnd *pWnd, int unused, Zeal::GameUI::CXPoint pt,
                                         unsigned int flag) {
  UIManager *ui = ZealService::get_instance()->ui.get();
  int rval = ZealService::get_instance()->hooks->hook_map["CheckboxClick"]->original(CheckboxClick_hook)(pWnd, unused,
                                                                                                         pt, flag);

  auto cb = ui->GetCheckboxCallback(pWnd);
  if (cb) cb(pWnd);

  return rval;
}

static void __fastcall SetSliderValue_hook(Zeal::GameUI::SliderWnd *pWnd, int unused, int value) {
  UIManager *ui = ZealService::get_instance()->ui.get();
  ZealService::get_instance()->hooks->hook_map["SetSliderValue"]->original(SetSliderValue_hook)(pWnd, unused, value);

  if (value < 0) value = 0;
  if (value > pWnd->max_val) value = pWnd->max_val;

  auto cb = ui->GetSliderCallback(pWnd);
  if (cb) cb(pWnd, value);
}

static void __fastcall SetComboValue_hook(Zeal::GameUI::BasicWnd *pWnd, int unused, int value) {
  UIManager *ui = ZealService::get_instance()->ui.get();
  ZealService::get_instance()->hooks->hook_map["SetComboValue"]->original(SetComboValue_hook)(pWnd, unused, value);

  auto cb = ui->GetComboCallback(pWnd);
  auto cb_parent = ui->GetComboCallback(pWnd->ParentWnd);
  if (cb)
    cb(pWnd, value);
  else if (cb_parent)
    cb_parent(pWnd->ParentWnd, value);
}

Zeal::GameUI::BasicWnd *UIManager::AddButtonCallback(Zeal::GameUI::BasicWnd *wnd, std::string name,
                                                     std::function<void(Zeal::GameUI::BasicWnd *)> callback,
                                                     bool log_errors) {
  if (wnd) {
    Zeal::GameUI::BasicWnd *btn = wnd->GetChildItem(name, log_errors);
    if (btn) {
      button_callbacks[btn] = callback;
      button_names[name] = btn;
      return btn;
    }
  }
  return nullptr;
}

Zeal::GameUI::BasicWnd *UIManager::AddCheckboxCallback(Zeal::GameUI::BasicWnd *wnd, std::string name,
                                                       std::function<void(Zeal::GameUI::BasicWnd *)> callback,
                                                       bool log_errors) {
  if (wnd) {
    Zeal::GameUI::BasicWnd *btn = wnd->GetChildItem(name, log_errors);
    if (btn) {
      checkbox_callbacks[btn] = callback;
      checkbox_names[name] = btn;
      return btn;
    }
  }
  return nullptr;
}

Zeal::GameUI::BasicWnd *UIManager::AddSliderCallback(Zeal::GameUI::BasicWnd *wnd, std::string name,
                                                     std::function<void(Zeal::GameUI::SliderWnd *, int)> callback,
                                                     int max_val, bool log_errors) {
  if (wnd) {
    Zeal::GameUI::SliderWnd *btn = (Zeal::GameUI::SliderWnd *)wnd->GetChildItem(name, log_errors);
    if (btn) {
      slider_callbacks[btn] = callback;
      slider_names[name] = btn;
      btn->max_val = max_val;
      return btn;
    }
  }
  return nullptr;
}

Zeal::GameUI::BasicWnd *UIManager::AddComboCallback(Zeal::GameUI::BasicWnd *wnd, std::string name,
                                                    std::function<void(Zeal::GameUI::BasicWnd *, int)> callback,
                                                    bool log_errors) {
  if (wnd) {
    Zeal::GameUI::BasicWnd *btn = (Zeal::GameUI::BasicWnd *)wnd->GetChildItem(name, log_errors);
    if (btn) {
      combo_callbacks[btn] = callback;
      combo_names[name] = btn;
      return btn;
    }
  }
  return nullptr;
}

void UIManager::AddLabel(Zeal::GameUI::BasicWnd *wnd, std::string name, bool log_errors) {
  if (wnd) {
    Zeal::GameUI::BasicWnd *btn = wnd->GetChildItem(name, log_errors);
    if (btn) {
      label_names[name] = btn;
    }
  }
}

void UIManager::SetSliderValue(std::string name, int value) {
  if (slider_names.count(name) > 0) {
    ZealService::get_instance()->hooks->hook_map["SetSliderValue"]->original(SetSliderValue_hook)(slider_names[name], 0,
                                                                                                  value);
  }
}

void UIManager::SetSliderValue(std::string name, float value) {
  if (slider_names.count(name) > 0) {
    ZealService::get_instance()->hooks->hook_map["SetSliderValue"]->original(SetSliderValue_hook)(
        slider_names[name], 0, static_cast<int>(value));
  }
}

void UIManager::AddListItems(Zeal::GameUI::ListWnd *wnd, const std::vector<std::string> data) {
  for (int row = 0; auto &current_row : data) {
    int x = wnd->AddString("");
    wnd->SetItemText(current_row, row, 0);
    wnd->SetItemData(row);
    row++;
  }
}

void UIManager::AddListItems(Zeal::GameUI::ComboWnd *wnd, const std::vector<std::string> data) {
  for (auto &current_row : data) wnd->InsertChoice(current_row);
}

void UIManager::AddListItems(Zeal::GameUI::ListWnd *wnd, const std::vector<std::vector<std::string>> data) {
  for (int row = 0; auto &current_row : data) {
    int x = wnd->AddString("");
    for (int col = 0; auto &current_col : current_row) {
      wnd->SetItemText(current_col, row, col);
      col++;
    }
    wnd->SetItemData(row);
    row++;
  }
}

void UIManager::SetChecked(std::string name, bool checked) {
  if (checkbox_names.count(name) > 0) checkbox_names[name]->Checked = checked;
}

void UIManager::SetLabelValue(std::string name, const char *format, ...) {
  va_list argptr;
  char buffer[512];
  va_start(argptr, format);
  vsnprintf(buffer, 511, format, argptr);
  va_end(argptr);
  if (label_names.count(name) > 0) {
    Zeal::Game::GameInternal::CXStr_PrintString(&label_names[name]->Text, buffer);
  }
}

void UIManager::SetComboValue(std::string name, int value) {
  if (combo_names.count(name) > 0) {
    //	ZealService::get_instance()->hooks->hook_map["SetComboValue"]->original(SetComboValue_hook)(combo_names[name]->FirstChildWnd,
    // 0, value); //this is crashing since firstchildwnd is null, may have to maintain the combo windows ourselves?
    ZealService::get_instance()->hooks->hook_map["SetComboValue"]->original(SetComboValue_hook)(
        combo_names[name]->CmbListWnd, 0, value);
  }
}

Zeal::GameUI::SliderWnd *UIManager::GetSlider(std::string name) {
  if (slider_names.count(name)) return slider_names[name];
  return nullptr;
}

Zeal::GameUI::BasicWnd *UIManager::GetCheckbox(std::string name) {
  if (checkbox_names.count(name)) return checkbox_names[name];
  return nullptr;
}

Zeal::GameUI::BasicWnd *UIManager::GetButton(std::string name) {
  if (button_names.count(name)) return button_names[name];
  return nullptr;
}

Zeal::GameUI::BasicWnd *UIManager::GetCombo(std::string name) {
  if (combo_names.count(name)) return combo_names[name];
  return nullptr;
}

std::function<void(Zeal::GameUI::SliderWnd *, int)> UIManager::GetSliderCallback(Zeal::GameUI::SliderWnd *wnd) {
  if (slider_callbacks.count(wnd)) return slider_callbacks[wnd];
  return nullptr;
}

std::function<void(Zeal::GameUI::BasicWnd *, int)> UIManager::GetComboCallback(Zeal::GameUI::BasicWnd *wnd) {
  if (combo_callbacks.count(wnd)) return combo_callbacks[wnd];
  return nullptr;
}

std::function<void(Zeal::GameUI::BasicWnd *)> UIManager::GetButtonCallback(Zeal::GameUI::BasicWnd *wnd) {
  if (button_callbacks.count(wnd)) return button_callbacks[wnd];
  return nullptr;
}

std::function<void(Zeal::GameUI::BasicWnd *)> UIManager::GetCheckboxCallback(Zeal::GameUI::BasicWnd *wnd) {
  if (checkbox_callbacks.count(wnd)) return checkbox_callbacks[wnd];
  return nullptr;
}

void UIManager::CleanUI() {
  Zeal::Game::print_debug("Clean UI UIMANAGER");
  combo_names.clear();
  combo_callbacks.clear();
  checkbox_names.clear();
  checkbox_callbacks.clear();
  slider_names.clear();
  slider_callbacks.clear();
  label_names.clear();
  button_names.clear();
  button_callbacks.clear();
}

// Creates a temporary ui.xml by merging the extra required zeal xml files into the existing active ui.xml file.
bool UIManager::WriteTemporaryUI(const std::filesystem::path &orig_file, const std::filesystem::path &merged_file) {
  if (orig_file.empty()) return false;
  std::ifstream infile(orig_file);
  if (!infile) return false;

  std::stringstream buffer;
  std::string line;
  bool compositeFound = false;
  std::string modifiedContent;

  const auto zealXmlFiles = UISkin::get_zeal_ui_xml_files();

  // Read file line by line
  while (std::getline(infile, line)) {
    // Make comparisons case insensitive.
    std::string loweredLine = line;
    std::transform(loweredLine.begin(), loweredLine.end(), loweredLine.begin(), ::tolower);

    // Exclude xml files that are provided by Zeal. Specifically this is the OptionsWindow.xml.
    bool duplicate = false;
    const std::string startTag = "<include>";
    size_t startPos = loweredLine.find(startTag);
    size_t endPos = loweredLine.find("</include>");
    if (startPos != std::string::npos && endPos != std::string::npos) {
      startPos += startTag.length();
      std::string xml_file_name = loweredLine.substr(startPos, endPos - startPos);
      for (const auto &file : zealXmlFiles) {
        std::string zeal_file = file;
        std::transform(zeal_file.begin(), zeal_file.end(), zeal_file.begin(), ::tolower);
        if (zeal_file == xml_file_name) duplicate = true;
      }
    }
    if (duplicate) continue;  // Skip adding file.

    // Search for the closing </composite> tag (case insensitive)
    if (!compositeFound && loweredLine.find("</composite>") != std::string::npos) {
      compositeFound = true;

      for (const auto &file : UISkin::get_zeal_ui_xml_files())
        // Add the new lines before the closing tag
        modifiedContent += "        <Include>" + std::string(file) + "</Include>\n";
    }

    // Add the current line to the buffer
    modifiedContent += line + "\n";
  }
  infile.close();

  std::filesystem::create_directories(merged_file.parent_path());
  std::ofstream outfile(merged_file);
  if (!outfile) return false;

  outfile << modifiedContent;
  outfile.close();
  return true;
}

void UIManager::RemoveTemporaryUI(const std::filesystem::path &zeal_equi_file) {
  if (std::filesystem::exists(zeal_equi_file)) {
    std::error_code ec;
    if (!std::filesystem::remove(zeal_equi_file, ec))  // No exceptions
      Zeal::Game::print_chat("Error removing %s: %s", zeal_equi_file.string().c_str(), ec.message().c_str());
  }
}

static void show_big_fonts_error_text(bool is_current_ui_big_fonts_mode) {
  std::string global_skin = UISkin::get_global_default_ui_skin_name();
  bool is_global_big_fonts_mode = UISkin::is_ui_skin_big_fonts_mode(global_skin.c_str());

  std::string message;
  if (is_current_ui_big_fonts_mode == is_global_big_fonts_mode)
    message = std::format(
        "Your new skin '{0}' does not match the active big fonts mode. You must restart the client"
        " and then run '/load <ui_skin> 0' again to reset to the proper appearance.",
        Zeal::Game::get_ui_skin());
  else
    message = std::format(
        "Your character skin '{0}' does not match the Zeal big fonts mode of your global default skin '{1}'. To fix, "
        "use options UI Loadskin or the /load <ui_skin> command with your desired ui, which will update both your "
        "global default and character setting, and then restart the client and run '/load <ui_skin> 0' again.",
        Zeal::Game::get_ui_skin(), UISkin::get_global_default_ui_skin_name());
  ZealService::get_instance()->queue_chat_message(message);  // Queued in order to defer print to after UI loaded.
}

static bool is_message_an_error(const char *error_message) {
  if (!error_message) return false;
  std::string message = error_message;

  // Ignore all except errors (like Warnings).
  if (!message.starts_with("Error:")) return false;

  // Ignore known spam from the song buff window.
  if (message.starts_with("Error: Could not find child Buff")) return false;

  // Ignore some optional missing buttons that don't cause crashes.
  if (message.starts_with("Error: Could not find child Zeal_ZoneSelect in window CharacterSelectWindow")) return false;
  if (message.starts_with("Error: Could not find child ChangeButton in window BankWnd")) return false;
  if (message.starts_with("Error: Could not find child LinkAllButton in window LootWnd")) return false;
  if (message.starts_with("Error: Could not find child LootAllButton in window LootWnd")) return false;

  return true;
}

// Report errors as they are detected. This has an off setting option in case of future incompatibilities.
static void LogUIError(const char *error_message) {
  auto zeal = ZealService::get_instance();
  if (zeal->ui->setting_show_ui_errors.get() && is_message_an_error(error_message))
    MessageBoxA(NULL, error_message, "UI XML parsing error", MB_ICONWARNING);
  zeal->hooks->hook_map["LogUIError"]->original(LogUIError)(error_message);
}

// This handles severe parsing errors that are likely to cause an abort. Show in dialog vs hunting for uierrors.txt.
static Zeal::GameUI::CXSTR *__fastcall SidlManager__GetParsingErrorMsg(Zeal::GameUI::SidlManager *sidl_manager,
                                                                       int unused_edx,
                                                                       Zeal::GameUI::CXSTR *msg_result) {
  if (sidl_manager->ErrorMsg.Data) {
    std::string error = std::string(sidl_manager->ErrorMsg);
    if (!error.empty()) MessageBoxA(NULL, error.c_str(), "Severe UI XML parsing error", MB_ICONWARNING);
  }
  return ZealService::get_instance()->hooks->hook_map["SidlManager__GetParsingErrorMsg"]->original(
      SidlManager__GetParsingErrorMsg)(sidl_manager, unused_edx, msg_result);
}

void __fastcall LoadSidlHk(void *t, int unused, Zeal::GameUI::CXSTR path1, Zeal::GameUI::CXSTR path2,
                           Zeal::GameUI::CXSTR filename) {
  std::string str_filename = filename;
  if (str_filename != "EQUI.xml") {
    ZealService::get_instance()->hooks->hook_map["LoadSidl"]->original(LoadSidlHk)(t, unused, path1, path2, filename);
    return;
  }

  // Check that the current ui skin big font mode matches the currently active mode.
  bool is_current_ui_big_fonts_mode = UISkin::is_ui_skin_big_fonts_mode(Zeal::Game::get_ui_skin());
  if (is_current_ui_big_fonts_mode != UISkin::is_big_fonts_mode())
    show_big_fonts_error_text(is_current_ui_big_fonts_mode);

  UIManager *ui = ZealService::get_instance()->ui.get();
  std::filesystem::path active_ui_path = std::string(path1);
  std::filesystem::path active_equi_file = active_ui_path / std::filesystem::path(str_filename);
  std::filesystem::path default_equi_file =
      std::filesystem::path(std::string(path2)) / std::filesystem::path(str_filename);
  std::filesystem::path equi_file = std::filesystem::exists(active_equi_file) ? active_equi_file : default_equi_file;

  const char *zeal_equi_filename = "EQUI_Zeal.xml";
  std::filesystem::path zeal_equi_file = active_ui_path / std::filesystem::path(zeal_equi_filename);
  if (ui->WriteTemporaryUI(equi_file, zeal_equi_file))
    filename.Set(zeal_equi_filename);
  else {
    std::string message =
        std::format("Zeal failed to generate {0} from {1}", zeal_equi_file.string(), equi_file.string());
    MessageBoxA(NULL, message.c_str(), "Zeal EQUI.xml failure", MB_OK | MB_ICONERROR | MB_TOPMOST);
  }

  ZealService::get_instance()->hooks->hook_map["LoadSidl"]->original(LoadSidlHk)(t, unused, path1, path2, filename);
  ui->RemoveTemporaryUI(zeal_equi_file);
}

int __fastcall XMLRead(void *t, int unused, Zeal::GameUI::CXSTR path1, Zeal::GameUI::CXSTR path2,
                       Zeal::GameUI::CXSTR filename) {
  if (UISkin::is_zeal_xml_file(std::string(filename))) path1.Set(UISkin::get_zeal_xml_path().append("").string());
  return ZealService::get_instance()->hooks->hook_map["XMLRead"]->original(XMLRead)(t, unused, path1, path2, filename);
}

int __fastcall XMLReadNoValidate(void *t, int unused, Zeal::GameUI::CXSTR path1, Zeal::GameUI::CXSTR path2,
                                 Zeal::GameUI::CXSTR filename) {
  if (UISkin::is_zeal_xml_file(std::string(filename))) path1.Set(UISkin::get_zeal_xml_path().append("").string());
  return ZealService::get_instance()->hooks->hook_map["XMLReadNoValidate"]->original(XMLReadNoValidate)(
      t, unused, path1, path2, filename);
}

// Instead of a full ui_SkillsWnd class just patch things here for sorting with left clicks.
static int __fastcall SkillsWnd_WndNotification(Zeal::GameUI::SidlWnd *wnd, int unused_edx,
                                                Zeal::GameUI::BasicWnd *src_wnd, int param_2, int param_3) {
  // CListWnd::OnHeaderClick() generates a WndNotification callback to the parent with a code of 0x0e.
  if (param_2 == 0x0e && (param_3 == 0 || param_3 == 2)) {
    auto list_wnd = reinterpret_cast<Zeal::GameUI::ListWnd *>(src_wnd);
    Zeal::Game::sort_list_wnd(list_wnd, param_3, Zeal::Game::SortType::Toggle);
    return 0;
  }

  // Pass through other notifications to original CSkillsWnd::WndNotification().
  reinterpret_cast<int(__thiscall *)(Zeal::GameUI::SidlWnd * wnd, Zeal::GameUI::BasicWnd * src_wnd, int param_2,
                                     int param_3)>(0x00432943)(wnd, src_wnd, param_2, param_3);
  return 0;
}

bool UIManager::handle_uierrors(const std::vector<std::string> &args) {
  if (args.size() != 2 || (args[1] != "on" && args[1] != "off")) {
    Zeal::Game::print_chat("Usage: /uierrors <on | off>");
  } else {
    bool enable = (args[2] == "on");
    ZealService::get_instance()->ui->setting_show_ui_errors.set(enable);
    Zeal::Game::print_chat("Showing UI errors is %s.", enable ? "enabled" : "disabled");
  }
  return true;
}

bool UIManager::handle_autobank(const std::vector<std::string> &args) {
  ZealService::get_instance()->ui->bank->change();
  return true;  // return true to stop the game from processing any further on this command, false if you want
}

// Supports batch locking or unlocking of primary UI windows.
bool UIManager::handle_uilock(const std::vector<std::string> &args) {
  if (args.size() == 2) {
    bool turn_on = Zeal::String::compare_insensitive(args[1], "on");
    bool turn_off = !turn_on && Zeal::String::compare_insensitive(args[1], "off");
    if (turn_on || turn_off) {
      Zeal::Game::print_chat("Setting Lock of primary windows to: %s", turn_on ? "ON" : "OFF");
      std::vector<Zeal::GameUI::SidlWnd *> windows = {
          Zeal::Game::Windows->ItemWnd,   Zeal::Game::Windows->PetInfo,   Zeal::Game::Windows->Group,
          Zeal::Game::Windows->Raid,      Zeal::Game::Windows->Target,    Zeal::Game::Windows->Options,
          Zeal::Game::Windows->HotButton, Zeal::Game::Windows->Player,    Zeal::Game::Windows->BuffWindowNORMAL,
          Zeal::Game::Windows->Casting,   Zeal::Game::Windows->SpellGems, Zeal::Game::Windows->SpellBook,
          Zeal::Game::Windows->Inventory, Zeal::Game::Windows->Actions,   Zeal::Game::Windows->Compass,
          Zeal::Game::Windows->Selector,  Zeal::Game::Windows->Tracking,
      };
      auto chat_mgr = Zeal::Game::Windows->ChatManager;
      if (chat_mgr) {
        for (int i = 0; i < chat_mgr->MaxChatWindows; i++)
          if (chat_mgr->ChatWindows[i]) windows.push_back(chat_mgr->ChatWindows[i]);
      }
      // Locking / unlocking bags will be hit and miss depending on whether they
      // are cached in the ContainerMgr.
      auto container_mgr = Zeal::Game::Windows->ContainerMgr;
      if (container_mgr) {
        for (int i = 0; i < 0x11; ++i) {
          auto wnd = container_mgr->pPCContainers[i];
          if (!wnd) continue;
          if (wnd->INIStorageName.Data && !_strnicmp(wnd->INIStorageName.CastToCharPtr(), "BagInv", 6))
            windows.push_back(wnd);
        }
      }
      if (options) windows.push_back(options->GetZealOptionsWindow());
      auto zeal = ZealService::get_instance();
      if (zeal && zeal->zone_map) windows.push_back(zeal->zone_map->get_internal_window());
      if (zeal && zeal->item_displays) {
        std::vector<Zeal::GameUI::ItemDisplayWnd *> item_wnds = zeal->item_displays->get_windows();
        for (auto wnd : item_wnds) windows.push_back(wnd);
      }
      for (Zeal::GameUI::SidlWnd *wnd : windows) {
        if (wnd && wnd->LockEnable) wnd->IsLocked = turn_on;
      }

      return true;
    }
  }
  Zeal::Game::print_chat("Usage: /uilock on or /uilock off");
  return true;
}

UIManager::UIManager(ZealService *zeal) {
  if (!Zeal::Game::is_new_ui()) return;  // Old UI not supported.

  zeal->callbacks->AddGeneric([this]() { CleanUI(); }, callback_type::CleanUI);
  // zeal->callbacks->AddGeneric([this]() { init_ui(); }, callback_type::InitUI);

  zeal->commands_hook->Add(
      "/autobank", {"/autoba", "/ab"},
      "Changes your money into its highest denomination in bank and inventory(requires bank to be open).",
      [this](const std::vector<std::string> &args) { return handle_autobank(args); });

  zeal->commands_hook->Add("/uierrors", {}, "Sets (on) or clears (off) the reporting of xml ui errors.",
                           [this](const std::vector<std::string> &args) { return handle_uierrors(args); });

  zeal->commands_hook->Add("/uilock", {}, "Sets (on) or clears (off) the Lock value in primary ui windows.",
                           [this](const std::vector<std::string> &args) { return handle_uilock(args); });

  bank = std::make_shared<ui_bank>(zeal, this);
  options = std::make_shared<ui_options>(zeal, this);
  loot = std::make_shared<ui_loot>(zeal, this);
  // guild = std::make_shared<ui_guild>(zeal, this);
  raid = std::make_shared<ui_raid>(zeal, this);
  hotbutton = std::make_shared<ui_hotbutton>(zeal, this);
  group = std::make_shared<ui_group>(zeal, this);
  inputDialog = std::make_shared<ui_inputdialog>(zeal, this);
  buffs = std::make_shared<ui_buff>(zeal, this);
  zoneselect = std::make_shared<ui_zoneselect>(zeal, this);
  inspect = std::make_shared<ui_inspect>(zeal, this);

  // zeal->hooks->Add("InitCharSelectSettings", 0x53c234, InitCharSelectSettings, hook_type_replace_call);
  zeal->hooks->Add("ButtonClick", 0x5951E0, ButtonClick_hook, hook_type_detour);
  zeal->hooks->Add("CheckboxClick", 0x5c3480, CheckboxClick_hook, hook_type_detour);
  zeal->hooks->Add("SetSliderValue", 0x5a6c70, SetSliderValue_hook, hook_type_detour);
  zeal->hooks->Add("SetComboValue", 0x579af0, SetComboValue_hook, hook_type_detour);
  zeal->hooks->Add("LoadSidl", 0x5992c0, LoadSidlHk, hook_type_detour);
  zeal->hooks->Add("SidlManager__GetParsingErrorMsg", 0x0058e300, SidlManager__GetParsingErrorMsg, hook_type_detour);
  zeal->hooks->Add("LogUIError", 0x00435eae, LogUIError, hook_type_detour);
  zeal->hooks->Add("XMLRead", 0x58D640, XMLRead, hook_type_detour);
  zeal->hooks->Add("XMLReadNoValidate", 0x58DA10, XMLReadNoValidate, hook_type_detour);

  // Patch the CSkillsWnd vtable so that it calls our custom WndNotification handler.
  auto vtable = reinterpret_cast<Zeal::GameUI::SidlScreenWndVTable *>(0x005e6b3c);
  vtable->WndNotification = SkillsWnd_WndNotification;
}

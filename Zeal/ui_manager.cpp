#include "ui_manager.h"

#include <algorithm>

#include "game_addresses.h"
#include "game_functions.h"
#include "game_structures.h"
#include "string_util.h"
#include "zeal.h"

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

void UIManager::AddXmlInclude(const std::string &name) { XMLIncludes.push_back(name); }

bool UIManager::WriteTemporaryUI(const std::string &equi_path, std::string equi_zeal_path) {
  if (!equi_path.empty()) {
    std::ifstream infile(equi_path);
    std::stringstream buffer;
    std::string line;
    bool compositeFound = false;
    std::string modifiedContent;

    if (infile) {
      // Read file line by line
      while (std::getline(infile, line)) {
        // Search for the closing </composite> tag (case insensitive)
        std::string loweredLine = line;
        std::transform(loweredLine.begin(), loweredLine.end(), loweredLine.begin(), ::tolower);

        if (!compositeFound && loweredLine.find("</composite>") != std::string::npos) {
          compositeFound = true;

          for (auto &file : XMLIncludes)
            // Add the new lines before the closing tag
            modifiedContent += "        <Include>" + file + "</Include>\n";
        }

        // Add the current line to the buffer
        modifiedContent += line + "\n";
      }
      infile.close();

      std::filesystem::path new_file_path = equi_zeal_path;
      std::filesystem::create_directories(new_file_path.parent_path());
      std::ofstream outfile(new_file_path);
      if (outfile) {
        outfile << modifiedContent;
        outfile.close();
        return true;
      }
    }
  }
  return false;
}

void UIManager::RemoveTemporaryUI(const std::string &equi_zeal_path) {
  std::filesystem::path new_file_path = equi_zeal_path;
  if (std::filesystem::exists(new_file_path)) {
    std::error_code ec;
    if (!std::filesystem::remove(new_file_path, ec))  // No exceptions
      Zeal::Game::print_chat("Error removing %s: %s", equi_zeal_path.c_str(), ec.message().c_str());
  }
}

void __fastcall LoadSidlHk(void *t, int unused, Zeal::GameUI::CXSTR path1, Zeal::GameUI::CXSTR path2,
                           Zeal::GameUI::CXSTR filename) {
  UIManager *ui = ZealService::get_instance()->ui.get();
  if (ui->included_files.size()) ui->included_files.clear();
  std::string str_filename = filename;

  std::string equi_zeal_path = "";
  if (str_filename == "EQUI.xml") {
    std::string active_ui_path = path1;
    std::string active_equi_file = active_ui_path + str_filename;
    std::string default_equi_file = static_cast<std::string>(path2) + str_filename;
    std::string equi_path = std::filesystem::exists(active_equi_file) ? active_equi_file : default_equi_file;

    equi_zeal_path = active_ui_path + "EQUI_Zeal.xml";
    if (ui->WriteTemporaryUI(equi_path, equi_zeal_path))
      filename.Set("EQUI_Zeal.xml");
    else {
      std::string message = std::format("Zeal failed to generate {0} from {1}", equi_zeal_path, equi_path);
      MessageBoxA(NULL, message.c_str(), "Zeal EQUI.xml failure", MB_OK | MB_ICONERROR | MB_TOPMOST);
    }
  }
  ZealService::get_instance()->hooks->hook_map["LoadSidl"]->original(LoadSidlHk)(t, unused, path1, path2, filename);

  if (!equi_zeal_path.empty()) ui->RemoveTemporaryUI(equi_zeal_path);
}

bool UIManager::AlreadyLoadedXml(std::string name) {
  std::string lName = name;
  std::transform(lName.begin(), lName.end(), lName.begin(), ::tolower);
  bool exists = std::find(included_files.begin(), included_files.end(), lName) != included_files.end();
  if (!exists)
    included_files.push_back(lName);
  else {
    CreateTmpXML();
    Zeal::Game::print_chat("Warning: Duplicate XML Included: %s", lName.c_str());
  }
  return exists;
}

void UIManager::CreateTmpXML() {
  std::filesystem::path new_file_path = UIManager::ui_path + std::string("EQUI_TMP.xml");
  if (std::filesystem::exists(new_file_path)) std::filesystem::remove(new_file_path);
  std::filesystem::create_directories(new_file_path.parent_path());
  std::ofstream outfile(new_file_path);
  if (outfile) {
    outfile << "<?xml version=\"1.0\" encoding=\"us-ascii\"?>" << std::endl;
    outfile << "<XML ID=\"EQInterfaceDefinitionLanguage\">" << std::endl;
    outfile << "<Schema xmlns=\"EverQuestData\" xmlns:dt=\"EverQuestDataTypes\" />" << std::endl;
    outfile << "</XML>" << std::endl;
    outfile.close();
  }
}

int __fastcall XMLRead(void *t, int unused, Zeal::GameUI::CXSTR path1, Zeal::GameUI::CXSTR path2,
                       Zeal::GameUI::CXSTR filename) {
  UIManager *ui = ZealService::get_instance()->ui.get();
  std::string str_filename = filename;
  std::string file = UIManager::ui_path + str_filename;
  if (std::filesystem::exists(file))
    path1.Set(UIManager::ui_path);
  else
    path1.Set((char *)0x63D3C0);

  if (ui->AlreadyLoadedXml(filename)) {
    path1.Set(UIManager::ui_path);
    filename.Set("EQUI_TMP.xml");
  }

  return ZealService::get_instance()->hooks->hook_map["XMLRead"]->original(XMLRead)(t, unused, path1, path2, filename);
}

int __fastcall XMLReadNoValidate(void *t, int unused, Zeal::GameUI::CXSTR path1, Zeal::GameUI::CXSTR path2,
                                 Zeal::GameUI::CXSTR filename) {
  UIManager *ui = ZealService::get_instance()->ui.get();
  std::string str_filename = filename;
  std::string file = UIManager::ui_path + str_filename;
  if (std::filesystem::exists(file))
    path1.Set(UIManager::ui_path);
  else
    path1.Set((char *)0x63D3C0);

  if (ui->AlreadyLoadedXml(filename)) {
    path1.Set(UIManager::ui_path);
    filename.Set("EQUI_TMP.xml");
  }

  return ZealService::get_instance()->hooks->hook_map["XMLReadNoValidate"]->original(XMLReadNoValidate)(
      t, unused, path1, path2, filename);
}

std::string UIManager::GetUIIni() {
  // First try to use client's function (GetUIIniFilename) to retrieve it.
  const char *ui_ini_file = reinterpret_cast<const char *(__cdecl *)(void)>(0x00437481)();
  if (ui_ini_file && ui_ini_file[0]) return ui_ini_file;

  Zeal::GameStructures::GAMECHARINFO *c = Zeal::Game::get_char_info();
  if (c) {
    std::string char_name = c->Name;
    return ".\\UI_" + char_name + "_pq.proj.ini";
  }
  return "";
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
  zeal->callbacks->AddGeneric([this]() { CleanUI(); }, callback_type::CleanUI);
  // zeal->callbacks->AddGeneric([this]() { init_ui(); }, callback_type::InitUI);

  zeal->commands_hook->Add("/uilock", {}, "Sets (on) or clears (off) the Lock value in primary ui windows.",
                           [this](std::vector<std::string> &args) { return handle_uilock(args); });

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
  zeal->hooks->Add("XMLRead", 0x58D640, XMLRead, hook_type_detour);
  zeal->hooks->Add("XMLReadNoValidate", 0x58DA10, XMLReadNoValidate, hook_type_detour);

  // Patch the CSkillsWnd vtable so that it calls our custom WndNotification handler.
  auto vtable = reinterpret_cast<Zeal::GameUI::SidlScreenWndVTable *>(0x005e6b3c);
  vtable->WndNotification = SkillsWnd_WndNotification;
}

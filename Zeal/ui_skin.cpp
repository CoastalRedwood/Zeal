#include "ui_skin.h"

#include <Windows.h>

#include <array>
#include <filesystem>

#include "game_functions.h"
#include "hook_wrapper.h"
#include "io_ini.h"
#include "string_util.h"
#include "zeal.h"

// List of required Zeal XML Files.  Must be manually kept up to date for config check and loading them.
// These files need to be included in the UI.xml by UIManager.
// Note: The EQUI_OptionsWindow.xml is included just to avoid conflicts with legacy modified files in /default/.
static constexpr std::array<const char *, 6> kZealXmlUiFiles = {"EQUI_ZealButtonWnd.xml", "EQUI_ZealInputDialog.xml",
                                                                "EQUI_ZealMap.xml",       "EQUI_ZealOptions.xml",
                                                                "EQUI_ZoneSelect.xml",    "EQUI_OptionsWindow.xml"};
// These tab files are part of the Zeal Options and do not need to be the UI.xml file.
static constexpr std::array<const char *, 7> kZealXmlTabFiles = {
    "EQUI_Tab_Cam.xml", "EQUI_Tab_Colors.xml",    "EQUI_Tab_FloatingDamage.xml", "EQUI_Tab_General.xml",
    "EQUI_Tab_Map.xml", "EQUI_Tab_Nameplate.xml", "EQUI_Tab_TargetRing.xml"};

// Intercepts the create font call after the world is started to upsize the requested fonts.
static HFONT WINAPI CreateFontIndirectA_hook(LOGFONTA *lplf) {
  auto display = Zeal::Game::get_display();
  int world_display_started = display ? display->WorldDisplayStarted : 0;

  if (world_display_started)  // Only modify fonts when started
  {
    strncpy_s(lplf->lfFaceName, "Calibri", sizeof(lplf->lfFaceName));
    lplf->lfQuality = 5;
    if (lplf->lfHeight == 10)  // Font 0 - not used in the UI
    {
      // weight 100
      lplf->lfHeight = 24;
      lplf->lfWeight = 400;
    } else if (lplf->lfHeight == 12)  // Font 1
    {
      // weight 100
      lplf->lfHeight = 25;
      lplf->lfWeight = 400;
    } else if (lplf->lfHeight == 14)  // Font 2
    {
      // weight 100
      lplf->lfHeight = 26;
      lplf->lfWeight = 400;
    } else if (lplf->lfHeight ==
               15)  // Font 3 - used for many UI elements in the default UI, like titles and button labels
    {
      // weight 100
      lplf->lfHeight = 28;
      lplf->lfWeight = 400;
    } else if (lplf->lfHeight == 16)  // Font 4
    {
      // weight 100
      lplf->lfHeight = 28;
      lplf->lfWeight = 700;
    } else if (lplf->lfHeight == 20)  // Font 5 - also character nametag above head
    {
      // weight 700 and 800 but i think only the 700 weight call's result is used to render
      lplf->lfHeight = 30;
      // if (lplf->lfWeight == 800) // this is the name tag font, created through s3dCreateFontTexture, but doesn't seem
      // to actually get used in the final render
      // else if (lplf->lfWeight == 700) // this is what shows up rendered in both the UI and name tags
    } else if (lplf->lfHeight == 24)  // Font 6 - seems to be used for YOU HAVE BEEN DISCONNECTED too
    {
      // weight 700
      lplf->lfHeight = 32;
    } else if (lplf->lfHeight == 69) {
      lplf->lfHeight = 80;
      lplf->lfWeight = 400;
    }
  }

  return CreateFontIndirectA(lplf);  // Finally call the WINAPI method.
}

static int __stdcall CSidlScreenWnd__ConvertToRes_hook(int val, int span, int defaultres, int screenres) {
  int ret = val;

  if (screenres != defaultres) {
    // try to scale linearly - this doesn't have great results and is just a fallback for when no layout is defined for
    // a resolution
    ret = val * screenres / defaultres;
    if (ret + span > screenres) ret = screenres - span;
    if (ret < 0) ret = 0;
  }

  return ret;
}

static int __cdecl CXWnd__DrawColoredRect_hook(int x1, int y1, int x2, int y2, int color, int clip_x1, int clip_y1,
                                               int clip_x2, int clip_y2) {
  x1 = x2 - 26;
  y1 = y2 - 22;
  //  expand clipping area if needed
  clip_x1 = x1 < clip_x1 ? x1 : clip_x1;
  clip_y1 = y1 < clip_y1 ? y1 : clip_y1;

  // the original function
  auto CXWnd__DrawColoredRect =
      reinterpret_cast<int(__cdecl *)(int, int, int, int, int, int, int, int, int)>(0x00574380);
  return CXWnd__DrawColoredRect(x1, y1, x2, y2, color, clip_x1, clip_y1, clip_x2, clip_y2);
}

static int __fastcall CTextureFont__DrawWrappedText_hook(Zeal::GameUI::CTextureFont *font, int unused_edx,
                                                         Zeal::GameUI::CXSTR str, Zeal::GameUI::CXRect rect,
                                                         Zeal::GameUI::CXRect clip_rect, int color, short a11,
                                                         int a12) {
  if (a11 != 17)  // this is used for coin on cursor too, but that value is centered - it's 17 when it's coin
  {
    rect.Left = rect.Right - 23;
    rect.Top = rect.Bottom - 22;
  }

  return font->DrawWrappedText(str, rect, clip_rect, color, a11, a12);
}

static Zeal::GameUI::CXRect *__fastcall CXWnd__GetHitTestRect_hook(void *this_xwnd, int unused_edx,
                                                                   Zeal::GameUI::CXRect *rect, int region) {
  ZealService::get_instance()->hooks->hook_map["CXWnd__GetHitTestRect_hook"]->original(CXWnd__GetHitTestRect_hook)(
      this_xwnd, unused_edx, rect, region);

  if (region == 3)  // Minimize Box
  {
    rect->Left += 8;
    rect->Top += 2;
    rect->Right += 8;
    rect->Bottom += 2;
  }
  /*
  else if (type == 4) // Tile Box
  {
  }
  */
  else if (region == 5)  // Close Box
  {
    rect->Left -= 7;
    rect->Top += 1;
    rect->Right -= 7;
    rect->Bottom += 1;
  }
  return rect;
}

// Apply client game patches to upsize the fonts and add padding where required to fix hard-coded values.
static bool patch_big_fonts_mode(ZealService *zeal) {
  // Replace a pointer to CreateFontIndirectA in the gfx_dx8.dll with our own function wrapper.
  const int gfx_dx8_base = reinterpret_cast<int>(GetModuleHandleA("eqgfx_dx8.dll"));
  if (!gfx_dx8_base) return false;
  if (gfx_dx8_base) {
    const int CreateFontIndirectA_offset = 0x000C7034;
    mem::write(gfx_dx8_base + CreateFontIndirectA_offset, (int)&CreateFontIndirectA_hook);
  }

  // Client dynamic upsizing is poor. Implement a first-order linear upscale model.
  zeal->hooks->Add("CSidlScreenWnd__ConvertToRes_hook", 0x005702A0, CSidlScreenWnd__ConvertToRes_hook,
                   hook_type_detour);
  // CXWnd::GetHitTestRect - this fixes the position of minimize and close buttons on the window titles
  zeal->hooks->Add("CXWnd__GetHitTestRect_hook", 0x00571540, CXWnd__GetHitTestRect_hook, hook_type_detour);

  // Tweak inventory slot bottom right inset label
  zeal->hooks->Add("InvSlotBrLabel", 0x005A79D1, CXWnd__DrawColoredRect_hook, hook_type_replace_call);
  zeal->hooks->Add("InvSlotBrFont", 0x005A7A30, CTextureFont__DrawWrappedText_hook, hook_type_replace_call);

  // Tweak cursor attachment bottom right inset label
  zeal->hooks->Add("CursorAttachBrLabel", 0x0041934B, CXWnd__DrawColoredRect_hook, hook_type_replace_call);
  zeal->hooks->Add("CursorAttachBrFont", 0x0041938C, CTextureFont__DrawWrappedText_hook, hook_type_replace_call);

  // nop out 10 bytes that set the list item height to 16 instead of using the font height
  mem::set(0x00432804, 0x90, 10);  // SkillsWindow
  mem::set(0x004323AA, 0x90, 10);  // SkillsSelectWindow

  // CTabWnd tab height from 8 to 22.
  const char tabHeight = 22;
  mem::write<char>(0x005935F6, tabHeight);

  // Window title text vertical position
  char titleTextOffset = -4;
  mem::write<char>(0x00572BD6, titleTextOffset);

  // CContainerWnd::SetContainer - this function resizes the window based on the number of container slots
  // Inventory container height padding calculation 36 -> 72
  char containerHeightPadding = 72;
  mem::write<char>(0x0041763B, containerHeightPadding);
  // Inventory container width padding calculation 14 -> 28
  char containerWidthPadding = 28;
  mem::write<char>(0x0041763E, containerWidthPadding);

  // Tooltips
  char val = 44;                      // Vertical offset from 22 to 44.
  mem::write<char>(0x00416D80, val);  // CContainerWnd::PostDraw
  mem::write<char>(0x004076CC, val);  // CBazaarWnd::PostDraw
  mem::write<char>(0x004049DC, val);  // CBankWnd::PostDraw
  mem::write<char>(0x0041ED59, val);  // CGiveWnd::PostDraw
  mem::write<char>(0x004266DC, val);  // CLootWnd::PostDraw
  mem::write<char>(0x00427193, val);  // CMerchantWnd::PostDraw
  mem::write<char>(0x0043950E, val);  // CTradeWnd::PostDraw

  val = 30;                           // Only up to 30 in the buff window.
  mem::write<char>(0x004096FA, val);  // CBuffWindow::PostDraw

  // Change the default screen size in CSidlScreenWnd::Init from 640x480 to 4k (3840 x 2160).
  // We are assuming the 2x UI skins are designed for 4k.
  // TODO: Verify the purpose / utility of this.
  const int width = 3840;
  const int height = 2160;
  mem::write<int>(0x0056E441, width);
  mem::write<int>(0x0056E451, height);
  mem::write<int>(0x0056E5D2, width);
  mem::write<int>(0x0056E5DC, height);

  return true;
}

std::string UISkin::get_global_default_ui_skin_name() {
  IO_ini ini(IO_ini::kClientFilename);
  return ini.getValue<std::string>("Defaults", "UISkin");
}

bool UISkin::is_ui_skin_big_fonts_mode(const char *ui_skin) {
  if (!ui_skin || !ui_skin[0]) return false;
  std::filesystem::path ui_skin_path = std::filesystem::path(ui_skin);  // Drop uifiles if present.
  std::filesystem::path ui_skin_name = ui_skin_path.filename();
  if (ui_skin_name.empty()) ui_skin_name = ui_skin_path.parent_path().filename();
  std::filesystem::path file_path = std::filesystem::current_path() / std::filesystem::path("uifiles") / ui_skin_name /
                                    std::filesystem::path(kBigFontsTriggerFilename);
  return std::filesystem::exists(file_path);
}

static void do_loadskin_hook(void *entity, const char *cmd) {
  std::string str_cmd = Zeal::String::trim_and_reduce_spaces(cmd);
  std::vector<std::string> args = Zeal::String::split(str_cmd, " ");

  if (args.size() > 0) {
    // Keep the global default value in sync with latest character setting (simplifies big font support).
    IO_ini ini(IO_ini::kClientFilename);
    ini.setValue<std::string>("Defaults", "UISkin", args[0]);
  }
  ZealService::get_instance()->hooks->hook_map["do_loadskin_hook"]->original(do_loadskin_hook)(entity, cmd);
}

void UISkin::initialize_mode(ZealService *zeal) {
  // Add a patch that keeps the global default UISkin stored in the client.ini in sync with any
  // updates written to character specific UI.ini files.
  zeal->hooks->Add("do_loadskin_hook", 0x004f8655, do_loadskin_hook, hook_type_detour);

  // The existence of the kBigFontsFilename in the active ui skin path signals the scaling mode.
  auto ui_skin = get_global_default_ui_skin_name();
  if (ui_skin.empty()) return;  // Just stick with default font size.
  is_big_fonts = is_ui_skin_big_fonts_mode(ui_skin.c_str()) && patch_big_fonts_mode(zeal);

  zeal_resources_path = std::filesystem::current_path() / std::filesystem::path("uifiles") /
                        std::filesystem::path(kDefaultZealFileSubfolder);
  zeal_xml_path = zeal_resources_path;
  if (is_big_fonts) zeal_xml_path /= kBigFontsXmlSubfolder;
}

bool UISkin::is_zeal_xml_file(const std::string &xml_file) {
  for (auto file : kZealXmlUiFiles) {
    if (xml_file == std::string(file)) return true;
  }
  for (auto file : kZealXmlTabFiles) {
    if (xml_file == std::string(file)) return true;
  }
  return false;
}

std::vector<const char *> UISkin::get_zeal_ui_xml_files() {
  std::vector<const char *> xml_files(kZealXmlUiFiles.begin(), kZealXmlUiFiles.end());
  return xml_files;
}

bool UISkin::configuration_check() {
  std::filesystem::path zeal_ui_path = get_zeal_xml_path();

  bool filepathExists = std::filesystem::is_directory(zeal_ui_path);
  if (not filepathExists) {
    std::wstring missing =
        L"A required uifiles folder that contains the zeal xml files is missing from the following location:\n" +
        zeal_ui_path.wstring() + L"\n" + L"Zeal will not function properly!";
    MessageBox(NULL, missing.c_str(), L"Zeal installation error", MB_OK | MB_ICONEXCLAMATION);
    return false;
  }

  std::wstring missing_files = L"";
  for (auto file : kZealXmlUiFiles) {
    std::filesystem::path this_file = zeal_ui_path / std::filesystem::path(file);

    if (not std::filesystem::exists(this_file)) missing_files += this_file.wstring() + L"\n";
  }
  for (auto file : kZealXmlTabFiles) {
    std::filesystem::path this_file = zeal_ui_path / std::filesystem::path(file);

    if (not std::filesystem::exists(this_file)) missing_files += this_file.wstring() + L"\n";
  }
  if (missing_files.length() > 0) {
    missing_files = L"The following files are missing from your 'zeal\\uifiles' directory:\n" + missing_files +
                    L"\nZeal will not function properly!";
    MessageBox(NULL, missing_files.c_str(), L"Zeal installation error", MB_OK | MB_ICONEXCLAMATION);
    return false;
  }

  return true;
}

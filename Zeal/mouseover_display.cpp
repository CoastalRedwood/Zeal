#include "mouseover_display.h"

#include <cctype>
#include <climits>
#include <cmath>
#include <format>
#include <unordered_map>
#include <vector>

#include "callbacks.h"
#include "commands.h"
#include "game_addresses.h"
#include "game_functions.h"
#include "game_ui.h"
#include "hook_wrapper.h"
#include "item_display.h"
#include "memory.h"
#include "string_util.h"
#include "tooltip.h"
#include "ui_skin.h"
#include "zeal.h"

// Provides mouseover tooltips by reusing the existing hold-right-click ItemDisplayWnd.
// Moves/Attaches it to the cursor when hovering over an item or spell.
// Hides it off screen when not in use to avoid a performance hit when creating/destroying windows.
// While enabled, it disables updates to "[ItemDisplayWindow]" for the character's UI file.

// InvSlotWnd's original handlers
static LPVOID s_original_inv_slot_mouse_move = nullptr;
static LPVOID s_original_inv_slot_wheel_move = nullptr;

// Pointers / Vtables for patching and restoration
static LPVOID s_original_spell_gem_wnd_mouse_move = nullptr;
static Zeal::GameUI::BaseVTable *s_hooked_spell_gem_vtbl = nullptr;
static LPVOID s_original_buff_button_wnd_mouse_move = nullptr;
static Zeal::GameUI::BaseVTable *s_hooked_buff_btn_vtbl = nullptr;
static LPVOID s_original_buff_wnd_notification = nullptr;
static Zeal::GameUI::SidlScreenWndVTable *s_hooked_buff_wnd_vtbl = nullptr;

// The ItemDisplayWnd re-used to render mouseover tooltips.
// Kept off-screen whenever no tooltip is currently shown.
static Zeal::GameUI::ItemDisplayWnd *s_mouseover_wnd = nullptr;

// The Short Duration Buff (song) window. Couldn't find a pointer so will attempt to discover it live
static Zeal::GameUI::BuffWindow *s_song_wnd = nullptr;

// Items/Spells/Buffs
static int s_mouseover_slot_index = -1;

// To determine when the mouseover tooltip is being populated vs a normal one
static bool s_in_mouseover_set_item = false;

// Maps each spell book icon's window pointer to its 0-7 slot index on it's spell page
static std::unordered_map<Zeal::GameUI::BasicWnd *, int> s_spell_book_icon_slot_map;

// Off-screen position to move the window to when hiding
static constexpr int kOffscreenX = -10000;
static constexpr int kOffscreenY = -10000;

// CXWnd::HandleWheelMove stub
static LPVOID const kDefaultHandleWheelMove = reinterpret_cast<LPVOID>(0x574ec0);

// Placeholder for the native/ini size of the tooltip window
static int s_native_max_width = 0;
static int s_native_max_height = 0;

// Minimum size for dynamically-sized tooltip
static constexpr int kMinTooltipWidth = 80;
static constexpr int kMinTooltipHeight = 40;

// Whether the current SetItem call is populating the mouseover tooltip window (item_display.cpp).
bool mouseover_in_set_item() { return s_in_mouseover_set_item; }

// The window used to render mouseover tooltips, or nullptr if mouseover tooltips are
// disabled/not yet initialized.
Zeal::GameUI::ItemDisplayWnd *mouseover_get_wnd() { return s_mouseover_wnd; }

// Suppresses/restores the native name-only hover tooltip. Suppression is global, so every
// path that sets it must have a matching restore (see mouseover_process_frame)
static void suppress_native_tooltip(bool suppressed) {
  auto &tooltips = ZealService::get_instance()->tooltips;
  if (tooltips) tooltips->set_native_tooltip_suppressed(suppressed);
}

// Moves s_mouseover_wnd off-screen without deactivating it, so it can be repositioned 
// back under the cursor without the window creation performance hit
static void hide_mouseover_window() {
  if (!s_mouseover_wnd) return;
  int current_width = s_mouseover_wnd->Location.Right - s_mouseover_wnd->Location.Left;
  int current_height = s_mouseover_wnd->Location.Bottom - s_mouseover_wnd->Location.Top;
  s_mouseover_wnd->Location.Left = kOffscreenX;
  s_mouseover_wnd->Location.Top = kOffscreenY;
  s_mouseover_wnd->Location.Right = kOffscreenX + current_width;
  s_mouseover_wnd->Location.Bottom = kOffscreenY + current_height;
  s_mouseover_slot_index = -1;
}

// Move window back on screen when required.
// Anchors the tooltip to the bottom-right of the cursor by a fixed offset, flipping to
// the left and/or above the cursor if it would otherwise extend past the screen edge.
static void reposition_mouseover_window(int mouse_x, int mouse_y) {

  if (!s_mouseover_wnd) return;
  auto *wnd_mgr = Zeal::Game::get_wnd_manager();
  if (!wnd_mgr) return;

  int screen_w = wnd_mgr->ScreenWidth;
  int screen_h = wnd_mgr->ScreenHeight;
  int current_width = s_mouseover_wnd->Location.Right - s_mouseover_wnd->Location.Left;
  int current_height = s_mouseover_wnd->Location.Bottom - s_mouseover_wnd->Location.Top;
  static constexpr int kTooltipCursorOffset = 16;

  // Regular tooltip position
  int x = mouse_x + kTooltipCursorOffset;
  int y = mouse_y + kTooltipCursorOffset;

  // Alternate tooltip possitons to avoid extension off-screen
  if (x + current_width > screen_w) x = mouse_x - current_width;
  if (y + current_height > screen_h) y = mouse_y - current_height;
  if (y + current_height > screen_h) y = screen_h - current_height;

  // Move tooltip
  s_mouseover_wnd->Location.Left = x;
  s_mouseover_wnd->Location.Top = y;
  s_mouseover_wnd->Location.Right = x + current_width;
  s_mouseover_wnd->Location.Bottom = y + current_height;
  s_mouseover_wnd->BringToFront();
}

// Remove the Close/Minimize buttons from the tooltip title bar via bitmasks
static constexpr DWORD kWindowStyleCloseBoxBit = 0x08;
static constexpr DWORD kWindowStyleMinimizeBoxBit = 0x20;
static void hide_mouseover_titlebar_buttons() {
  if (!s_mouseover_wnd) return;
  s_mouseover_wnd->WindowStyleFlags &= ~(kWindowStyleCloseBoxBit | kWindowStyleMinimizeBoxBit);
}

// Commandeers an ItemDisplayWnd for mouseover tooltips: captures its native/INI size as the
// default max, disables position persistence, and hides it off-screen ready for use
static void acquire_mouseover_wnd(Zeal::GameUI::ItemDisplayWnd *wnd) {
  s_mouseover_wnd = wnd;
  // Capture the native/INI size before anything (e.g. dynamic resizing) touches it
  s_native_max_width = wnd->Location.Right - wnd->Location.Left;
  s_native_max_height = wnd->Location.Bottom - wnd->Location.Top;
  wnd->EnableINIStorage &= ~0x1;
  wnd->Activate();
  hide_mouseover_titlebar_buttons();
  hide_mouseover_window();
}

// Use mouse wheel to scroll tooltip
static void forward_wheel_to_tooltip(int mouse_x, int mouse_y, int wheel_delta, int unknown) {

  if (s_mouseover_wnd && s_mouseover_slot_index != -1 && s_mouseover_wnd->ItemDescription) {
    reinterpret_cast<int(__thiscall *)(Zeal::GameUI::BasicWnd *, int, int, int, int)>(
        s_mouseover_wnd->ItemDescription->vtbl->HandleWheelMove)(s_mouseover_wnd->ItemDescription, mouse_x, mouse_y,
                                                                 wheel_delta, unknown);
  }
}

// CItemDisplayWnd's internal text/layout refresh, also used after SetItem/SetSpell
static void trigger_item_display_redraw(Zeal::GameUI::ItemDisplayWnd *wnd) {
  reinterpret_cast<void(__thiscall *)(Zeal::GameUI::ItemDisplayWnd *)>(0x0042359a)(wnd);
}

static constexpr int kHeuristicFontIndex = 3;  // A representative default UI text font size, fallback only
static constexpr int kFallbackLineHeight = 14;

static const char *get_measurement_font_face() { return UISkin::is_big_fonts_mode() ? "Calibri" : "Arial"; }

// Safety margins, mostly as ratios of line_height. Width margins cover measurement error
// (face/weight guesses, kerning, rounding) plus right-side padding/scrollbar. Erring wide should be
// safe since the longest line can never wrap against its own measured width
static constexpr float kHorizontalPaddingRatio = 0.4f;
static constexpr int kFixedChromeEstimate = 20;  // Title bar, doesn't scale with font size
static constexpr float kHeightSafetyMarginRatio = 0.15f;
static constexpr float kWidthSafetyMarginRatio = 1.0f;
static constexpr int kMinWidthSafetyMarginPx = 18;  // Floor for small fonts, not additive

// Lazily-created GDI measurement fonts keyed by (pixel height / weight / big-fonts), never deleted
static std::unordered_map<int64_t, HFONT> s_measurement_font_cache;

static constexpr int kMeasurementFontWeight = FW_NORMAL;

static constexpr int kMaxFontMatchAttempts = 8;

static HFONT create_font_matching_height(int line_height, int weight, int request_height, const char *face) {
  // CreateFontA's lfHeight doesn't map directly to the resulting tmHeight, so hill-climb by
  // 1px toward the target line_height, keeping the closest match seen
  HFONT best_font = nullptr;
  int best_diff = INT_MAX;
  int height_to_try = request_height;
  int tried_heights[kMaxFontMatchAttempts];
  int tried_count = 0;

  for (int attempt = 0; attempt < kMaxFontMatchAttempts; attempt++) {
    if (height_to_try <= 0) break;
    // Revisiting a height means the walk is oscillating around the target, close enough
    bool already_tried = false;
    for (int i = 0; i < tried_count; i++) {
      if (tried_heights[i] == height_to_try) already_tried = true;
    }
    if (already_tried) break;
    tried_heights[tried_count++] = height_to_try;

    // Negative lfHeight requests character height, tmHeight adds internal leading on top
    HFONT candidate = CreateFontA(-height_to_try, 0, 0, 0, weight, FALSE, FALSE, FALSE, ANSI_CHARSET,
                                  OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                  DEFAULT_PITCH | FF_DONTCARE, face);
    if (!candidate) break;
    HDC hdc = GetDC(nullptr);
    TEXTMETRICA tm = {0};
    if (hdc) {
      HFONT old_font = static_cast<HFONT>(SelectObject(hdc, candidate));
      GetTextMetricsA(hdc, &tm);
      SelectObject(hdc, old_font);
      ReleaseDC(nullptr, hdc);
    }

    // Keep the closest match seen so a near-miss is returned instead of nullptr
    int diff = std::abs(tm.tmHeight - line_height);
    if (diff < best_diff) {
      if (best_font) DeleteObject(best_font);
      best_font = candidate;
      best_diff = diff;
    } else {
      DeleteObject(candidate);
    }
    // tmHeight <= 0 means metrics were never measured (GetDC failed), use current best_font
    if (diff == 0 || tm.tmHeight <= 0) break;
    height_to_try += (tm.tmHeight > line_height ? -1 : 1);
  }
  return best_font;
}

// Returns a (cached) GDI font matching the game font's line height, for measuring text widths
static HFONT get_measurement_font(int line_height, int weight) {
  bool big_fonts = UISkin::is_big_fonts_mode();
  // Pack (line_height, weight, big-fonts flag) into one cache key
  int64_t key =
      (static_cast<int64_t>(line_height) << 33) | (static_cast<int64_t>(weight) << 1) | (big_fonts ? 1 : 0);
  auto it = s_measurement_font_cache.find(key);
  if (it != s_measurement_font_cache.end()) return it->second;
  HFONT font = create_font_matching_height(line_height, weight, line_height, get_measurement_font_face());
  s_measurement_font_cache[key] = font;  // Cached even on failure (nullptr) to avoid retrying every frame
  return font;
}

// The real font weight isn't known from line_height alone, so lines are measured at both
// weights and the wider result wins - extra space beats an unexpected wrap
static constexpr int kAltMeasurementFontWeight = FW_BOLD;

// Measures the on-screen pixel width of a plain (markup-free) string using GDI
static float measure_text_width_px(const std::string &text, int line_height, int weight = kMeasurementFontWeight) {
  if (text.empty()) return 0.0f;
  HFONT font = get_measurement_font(line_height, weight);
  if (!font) return 0.0f;
  // GetDC(nullptr) borrows the shared screen DC; text extents don't need a window of our own
  HDC hdc = GetDC(nullptr);
  if (!hdc) return 0.0f;
  // Select our font, measure, then restore the DC's previous font before releasing it
  HFONT old_font = static_cast<HFONT>(SelectObject(hdc, font));
  SIZE size = {0, 0};
  GetTextExtentPoint32A(hdc, text.c_str(), static_cast<int>(text.length()), &size);
  SelectObject(hdc, old_font);
  ReleaseDC(nullptr, hdc);
  // 0.0f returns above mean "unmeasurable" and are indistinguishable from genuinely empty text
  return static_cast<float>(size.cx);
}

// Strips STML markup tags, treats &nbsp; as a (non-collapsing) space, combines multiple spaces into one
static std::string strip_and_collapse_line(const std::string &line) {
  std::string plain;
  plain.reserve(line.size());
  bool in_space_run = false;
  for (size_t i = 0; i < line.size();) {
    char c = line[i];
    // Only treat '<' as markup if it looks like a tag
    if (c == '<' && i + 1 < line.size() && (isalpha(static_cast<unsigned char>(line[i + 1])) || line[i + 1] == '/')) {
      size_t close = line.find('>', i);
      if (close != std::string::npos) {
        i = close + 1;
        continue;
      }
    }
    if (line.compare(i, 6, "&nbsp;") == 0) {
      plain += ' ';
      in_space_run = false;
      i += 6;
      continue;
    }
    i++;
    if (c == ' ') {
      if (in_space_run) continue;
      in_space_run = true;
      plain += ' ';
      continue;
    }
    in_space_run = false;
    plain += c;
  }
  return plain;
}

// Estimates the on-screen pixel width of one line of STML content (max of normal/bold weights).
static float estimate_line_width_px(const std::string &line, int line_height) {
  std::string plain = strip_and_collapse_line(line);
  float normal_px = measure_text_width_px(plain, line_height, kMeasurementFontWeight);
  float bold_px = measure_text_width_px(plain, line_height, kAltMeasurementFontWeight);
  return max(normal_px, bold_px);
}

// Shrinks/grows s_mouseover_wnd to fit the content in DisplayText, word-wrap aware, capped by
// the effective max width/height (/mouseover width|height overrides, else the native size).
// The engine never re-lays-out ItemDescription after resizing the parent, so content size is
// estimated from the raw STML text using GDI font metrics (the client's fonts are real GDI
// fonts). Height is exact via the live font's GetHeight(); width depends on the guessed face
// (see get_measurement_font_face).
static void fit_mouseover_window_to_content() {
  auto &item_displays = ZealService::get_instance()->item_displays;
  if (!s_mouseover_wnd || !s_mouseover_wnd->DisplayText.Data || !item_displays) return;

  int max_w = item_displays->setting_mouseover_max_width.get();
  max_w = max_w > 0 ? max_w : s_native_max_width;
  int max_h = item_displays->setting_mouseover_max_height.get();
  max_h = max_h > 0 ? max_h : s_native_max_height;
  if (max_w <= 0 || max_h <= 0) return;  // Native size not yet captured.

  max_w = max(max_w, kMinTooltipWidth);
  max_h = max(max_h, kMinTooltipHeight);

  // Prefer ItemDescription's own font so this adapts to custom skins/big-fonts mode. GAMEFONT*
  // and CTextureFont* are the same underlying object (see CXWndManager::TextureFont in game_ui.h)
  Zeal::GameUI::GAMEFONT *desc_font_ptr =
      s_mouseover_wnd->ItemDescription ? s_mouseover_wnd->ItemDescription->FontPointer : s_mouseover_wnd->FontPointer;
  auto *desc_font = reinterpret_cast<Zeal::GameUI::CTextureFont *>(desc_font_ptr);
  int line_height = desc_font ? desc_font->GetHeight() : 0;
  if (line_height <= 0) {
    // Fall back to a representative default UI font if ItemDescription's own font isn't available.
    auto *wnd_mgr = Zeal::Game::get_wnd_manager();
    auto *fallback_font = wnd_mgr ? wnd_mgr->GetFont(kHeuristicFontIndex) : nullptr;
    line_height = fallback_font ? fallback_font->GetHeight() : 0;
  }
  if (line_height <= 0) line_height = kFallbackLineHeight;

  // Measure once per line up front; the wrap pass below reuses the results.
  auto lines = Zeal::String::split_text(std::string(s_mouseover_wnd->DisplayText), "<BR>");
  std::vector<float> line_widths_px;
  line_widths_px.reserve(lines.size());
  float longest_line_px = 0.0f;
  for (auto &line : lines) {
    float px = estimate_line_width_px(line, line_height);
    line_widths_px.push_back(px);
    longest_line_px = max(longest_line_px, px);
  }

  int horizontal_padding = static_cast<int>(line_height * kHorizontalPaddingRatio);
  int width_safety_margin = max(static_cast<int>(line_height * kWidthSafetyMarginRatio), kMinWidthSafetyMarginPx);
  int height_safety_margin = static_cast<int>(line_height * kHeightSafetyMarginRatio);

  int natural_text_w = static_cast<int>(longest_line_px) + horizontal_padding + width_safety_margin;
  int final_width = max(kMinTooltipWidth, min(max_w, natural_text_w));

  float avail_width_px = static_cast<float>(max(1, final_width - horizontal_padding));

  int total_lines = 0;
  for (float line_px : line_widths_px) {
    total_lines += max(1, static_cast<int>(std::ceil(line_px / avail_width_px)));
  }
  if (lines.empty()) total_lines = 1;

  int content_h = total_lines * line_height;
  int final_height = max(kMinTooltipHeight, min(max_h, content_h + kFixedChromeEstimate + height_safety_margin));

  Zeal::Game::GameInternal::CXWndMoveAndInvalidate(s_mouseover_wnd, 0, s_mouseover_wnd->Location.Left,
                                                   s_mouseover_wnd->Location.Top,
                                                   s_mouseover_wnd->Location.Left + final_width,
                                                   s_mouseover_wnd->Location.Top + final_height);
  trigger_item_display_redraw(s_mouseover_wnd);
}

// Updates the mouseover window for the current spell/item slot
template <typename PopulateFn>
static void run_mouseover_update(int spell_or_slot, int mouse_x, int mouse_y, PopulateFn populate) {
  // Activate mouseover window if required
  if (!s_mouseover_wnd->IsActivated) {
    s_mouseover_wnd->EnableINIStorage &= ~0x1;
    s_mouseover_wnd->Activate();
  }

  // Just reposition if already showing same spell/slot
  if (spell_or_slot == s_mouseover_slot_index) {
    reposition_mouseover_window(mouse_x, mouse_y);
    return;
  }

  auto *default_wnd = Zeal::Game::Windows->ItemWnd;
  Zeal::Game::Windows->ItemWnd = s_mouseover_wnd;

  populate();
  // Trigger redraw after updated DisplayText.
  trigger_item_display_redraw(s_mouseover_wnd);
  // SetItem/SetSpell always re-show the icon; force it back off for the mouseover window
  // so the tooltip stays compact (the icon is already under the mouse anyway)
  if (s_mouseover_wnd->IconBtn) s_mouseover_wnd->IconBtn->IsVisible = false;
  // Likewise re-hide the close/minimize title bar buttons in case Activate() re-drew them
  hide_mouseover_titlebar_buttons();

  Zeal::Game::Windows->ItemWnd = default_wnd;

  fit_mouseover_window_to_content();
  reposition_mouseover_window(mouse_x, mouse_y);
  s_mouseover_slot_index = spell_or_slot;
}

// Starts the process of displaying/updating an item when it's hovered over
static void show_mouseover_item(Zeal::GameStructures::_GAMEITEMINFO *item, int slot_index, int mouse_x, int mouse_y) {
  if (!s_mouseover_wnd) return;
  auto &item_displays = ZealService::get_instance()->item_displays;
  if (!item_displays || !item_displays->setting_mouseover_tooltips.get()) return;

  run_mouseover_update(slot_index, mouse_x, mouse_y, [&]() {
    s_in_mouseover_set_item = true;
    // Call original SetItem with show=true to build basic DisplayText.
    ZealService::get_instance()->hooks->hook_map["SetItem"]->original(SetItem)(s_mouseover_wnd, 0, item, true);
    s_in_mouseover_set_item = false;

    // Append our enhanced text on top of the basic text.
    item_displays->add_to_cache(item);
    UpdateSetItemText(s_mouseover_wnd, item);
  });
}

// Starts the process of displaying/updating a spell/buff when it's hovered over
static void show_mouseover_spell(int spell_id, int mouse_x, int mouse_y, bool is_buff_spell = false) {
  if (!s_mouseover_wnd) return;
  auto &item_displays = ZealService::get_instance()->item_displays;
  if (!item_displays || !item_displays->setting_mouseover_tooltips.get()) return;

  run_mouseover_update(spell_id, mouse_x, mouse_y, [&]() {
    ZealService::get_instance()->hooks->hook_map["SetSpell"]->original(SetSpell)(s_mouseover_wnd, 0, spell_id, true,
                                                                                  0);
    UpdateSetSpellText(s_mouseover_wnd, spell_id, is_buff_spell);
  });
}

// Hooked HandleMouseMove for inventory slots: shows a tooltip for the item in `wnd`, or
// closes the tooltip if the slot is empty.
static int __fastcall InvSlotWnd_HandleMouseMove(Zeal::GameUI::InvSlotWnd *wnd, int unused_edx, int mouse_x,
                                                 int mouse_y, unsigned int flags) {
  bool mouseover_enabled = ZealService::get_instance()->item_displays->setting_mouseover_tooltips.get();
  // Suppress before calling through: the original handler can trigger the native tooltip itself.
  if (mouseover_enabled && s_mouseover_wnd) suppress_native_tooltip(true);

  int result = reinterpret_cast<int(__thiscall *)(Zeal::GameUI::InvSlotWnd *, int, int, unsigned int)>(
      s_original_inv_slot_mouse_move)(wnd, mouse_x, mouse_y, flags);

  if (!mouseover_enabled) return result;

  if (wnd->invSlot && wnd->invSlot->Item) {
    auto *item = reinterpret_cast<Zeal::GameStructures::_GAMEITEMINFO *>(wnd->invSlot->Item);
    show_mouseover_item(item, (int)wnd->invSlot->Index, mouse_x, mouse_y);
  } else {
    hide_mouseover_window();
  }
  return result;
}

// Hooked HandleWheelMove for items that scrolls the tooltip
static int __fastcall InvSlotWnd_HandleWheelMove(Zeal::GameUI::InvSlotWnd *wnd, int unused_edx, int mouse_x,
                                                 int mouse_y, int wheel_delta, int unknown) {
  // Mouseover, forward the scroll to the tooltip
  if (s_mouseover_wnd && s_mouseover_slot_index != -1 && s_mouseover_wnd->ItemDescription) {
    forward_wheel_to_tooltip(mouse_x, mouse_y, wheel_delta, unknown);
    return 1;  // Consumed - prevents zoom.
  }
  // No mouseover, allow default behavior
  return reinterpret_cast<int(__thiscall *)(Zeal::GameUI::InvSlotWnd *, int, int, int, int)>(
      s_original_inv_slot_wheel_move)(wnd, mouse_x, mouse_y, wheel_delta, unknown);
}

// Hooked HandleMouseMove for memorized-spell gems
static int __fastcall SpellGemWnd_HandleMouseMove(Zeal::GameUI::SpellGemWnd *wnd, int unused_edx, int mouse_x,
                                                  int mouse_y, unsigned int flags) {
  bool mouseover_enabled = ZealService::get_instance()->item_displays->setting_mouseover_tooltips.get();
  // Suppress before calling through: the original handler can trigger the native tooltip itself.
  if (mouseover_enabled && s_mouseover_wnd) suppress_native_tooltip(true);

  int result = reinterpret_cast<int(__thiscall *)(Zeal::GameUI::SpellGemWnd *, int, int, unsigned int)>(
      s_original_spell_gem_wnd_mouse_move)(wnd, mouse_x, mouse_y, flags);

  if (!mouseover_enabled) return result;

  auto *char_info = Zeal::Game::get_char_info();
  auto *cast_wnd = Zeal::Game::Windows->SpellGems;
  if (!char_info || !cast_wnd) {
    hide_mouseover_window();
    return result;
  }

  for (int i = 0; i < 8; i++) {
    if (cast_wnd->SpellSlots[i] == wnd) {
      int spell_id = char_info->MemorizedSpell[i];
      if (spell_id > 0 && spell_id < GAME_NUM_SPELLS)
        show_mouseover_spell(spell_id, mouse_x, mouse_y);
      else
        hide_mouseover_window();
      return result;
    }
  }

  hide_mouseover_window();
  return result;
}

// Hooked HandleWheelMove for spells that scrolls the tooltip
static int __fastcall SpellGemWnd_HandleWheelMove(Zeal::GameUI::SpellGemWnd *wnd, int unused_edx, int mouse_x,
                                                  int mouse_y, int wheel_delta, int unknown) {
  if (s_mouseover_wnd && s_mouseover_slot_index != -1 && s_mouseover_wnd->ItemDescription) {
    forward_wheel_to_tooltip(mouse_x, mouse_y, wheel_delta, unknown);
    return 1;
  }
  return 0;
}

// Handler for BuffWindowButton (buff bar, song window, & spell book icons) which share a vtable.
// Actions based on parent window
static int __fastcall ButtonWnd_HandleMouseMove(Zeal::GameUI::SidlWnd *wnd, int unused_edx, int mouse_x, int mouse_y,
                                                unsigned int flags) {
  bool mouseover_enabled = ZealService::get_instance()->item_displays->setting_mouseover_tooltips.get();
  // Suppress before calling through: the original handler can trigger the native tooltip itself.
  if (mouseover_enabled && s_mouseover_wnd) suppress_native_tooltip(true);

  // Temporarily swap ItemWnd away before calling the original so the client's
  // hover handling doesn't activate/deactivate our mouseover window.
  auto *default_item_wnd = Zeal::Game::Windows->ItemWnd;
  if (s_mouseover_wnd && Zeal::Game::Windows->ItemWnd == s_mouseover_wnd) {
    auto *safe_wnd = ZealService::get_instance()->item_displays->get_available_window();
    Zeal::Game::Windows->ItemWnd = safe_wnd ? safe_wnd : default_item_wnd;
  }

  int result = reinterpret_cast<int(__thiscall *)(Zeal::GameUI::SidlWnd *, int, int, unsigned int)>(
      s_original_buff_button_wnd_mouse_move)(wnd, mouse_x, mouse_y, flags);

  Zeal::Game::Windows->ItemWnd = default_item_wnd;

  if (!mouseover_enabled) return result;

  auto *parent = reinterpret_cast<Zeal::GameUI::BasicWnd *>(wnd->ParentWnd);
  auto *buff_wnd = reinterpret_cast<Zeal::GameUI::BuffWindow *>(Zeal::Game::Windows->BuffWindowNORMAL);
  auto *spell_book = reinterpret_cast<Zeal::GameUI::BasicWnd *>(Zeal::Game::Windows->SpellBook);

  // Buff bar path
  if (buff_wnd && parent == reinterpret_cast<Zeal::GameUI::BasicWnd *>(buff_wnd)) {
    auto *char_info = Zeal::Game::get_char_info();
    if (!char_info) {
      hide_mouseover_window();
      return result;
    }
    int max_buffs = char_info->GetMaxBuffs();
    for (int i = 0; i < max_buffs && i < GAME_NUM_BUFFS; i++) {
      if (buff_wnd->BuffButtonWnd[i] == reinterpret_cast<Zeal::GameUI::BuffWindowButton *>(wnd)) {
        auto *buff = char_info->GetBuff(i);
        if (buff && buff->SpellId > 0 && buff->SpellId < GAME_NUM_SPELLS)
          show_mouseover_spell(buff->SpellId, mouse_x, mouse_y, true);
        else
          hide_mouseover_window();
        return result;
      }
    }
    hide_mouseover_window();
    return result;
  }

  // Attempt to discover song window if not already known
  if (!s_song_wnd && parent && buff_wnd && parent != reinterpret_cast<Zeal::GameUI::BasicWnd *>(buff_wnd) &&
      parent != spell_book) {
    auto *candidate = reinterpret_cast<Zeal::GameUI::BuffWindow *>(parent);
    if (candidate->vtbl == buff_wnd->vtbl) s_song_wnd = candidate;
  }

  // Song bar path
  if (s_song_wnd && parent == reinterpret_cast<Zeal::GameUI::BasicWnd *>(s_song_wnd)) {
    auto *char_info = Zeal::Game::get_char_info();
    if (!char_info) {
      hide_mouseover_window();
      return result;
    }

    int button_index = -1;
    for (int i = 0; i < 6 && i < GAME_NUM_BUFFS; i++) {
      if (s_song_wnd->BuffButtonWnd[i] == reinterpret_cast<Zeal::GameUI::BuffWindowButton *>(wnd)) {
        button_index = i;
        break;
      }
    }
    if (button_index == -1) {
      hide_mouseover_window();
      return result;
    }

    // Song window buttons map to buff slots starting at offset 15
    static constexpr int kSongBuffOffset = 15;
    auto *buff = char_info->GetBuff(kSongBuffOffset + button_index);
    if (buff && buff->SpellId > 0 && buff->SpellId < GAME_NUM_SPELLS)
      show_mouseover_spell(buff->SpellId, mouse_x, mouse_y, true);
    else
      hide_mouseover_window();
    return result;
  }

  // Spell book icon path
  if (spell_book && parent == spell_book) {
    auto *spell_book_wnd = Zeal::Game::Windows->SpellBook;
    auto *char_info = Zeal::Game::get_char_info();
    if (!spell_book_wnd || !char_info) {
      hide_mouseover_window();
      return result;
    }

    auto it = s_spell_book_icon_slot_map.find(reinterpret_cast<Zeal::GameUI::BasicWnd *>(wnd));
    if (it == s_spell_book_icon_slot_map.end()) {
      hide_mouseover_window();
      return result;
    }

    DWORD book_index = spell_book_wnd->SpellBookIndex;
    if (book_index >= 32) {
      hide_mouseover_window();
      return result;
    }

    DWORD actual_index = (book_index * 8) + it->second;
    if (actual_index >= 256) {
      hide_mouseover_window();
      return result;
    }

    int spell_id = char_info->SpellBook[actual_index];
    if (spell_id > 0 && spell_id < GAME_NUM_SPELLS)
      show_mouseover_spell(spell_id, mouse_x, mouse_y);
    else
      hide_mouseover_window();
    return result;
  }

  // Any other button type, ignore.
  return result;
}

// Hooked HandleWheelMove for other spells (buffs/songs/spellbook) that scrolls the tooltip
static int __fastcall ButtonWnd_HandleWheelMove(Zeal::GameUI::SidlWnd *wnd, int unused_edx, int mouse_x, int mouse_y,
                                                int wheel_delta, int unknown) {
  if (s_mouseover_wnd && s_mouseover_slot_index != -1 && s_mouseover_wnd->ItemDescription) {
    forward_wheel_to_tooltip(mouse_x, mouse_y, wheel_delta, unknown);
    return 1;
  }
  return 0;
}

// Hooked WndNotification on the BuffWindow vtable (also covers the song window, which shares the same vtable).
static int __fastcall BuffWnd_WndNotification(Zeal::GameUI::BuffWindow *wnd, int unused_edx,
                                              Zeal::GameUI::BasicWnd *src_wnd, int param_2, void *param_3) {

  auto *default_item_display_wnd = Zeal::Game::Windows->ItemWnd;
  if (s_mouseover_wnd && default_item_display_wnd == s_mouseover_wnd) {
    auto *display_wnd = ZealService::get_instance()->item_displays->get_available_window();
    if (display_wnd) {
      Zeal::Game::Windows->ItemWnd = display_wnd;
      // Re-use existing windows if the max windows are already open
      if (display_wnd->IsVisible) display_wnd->Deactivate();
    }
  }

  int result = reinterpret_cast<int(__thiscall *)(Zeal::GameUI::BuffWindow *, Zeal::GameUI::BasicWnd *, int, void *)>(
      s_original_buff_wnd_notification)(wnd, src_wnd, param_2, param_3);

  Zeal::Game::Windows->ItemWnd = default_item_display_wnd;
  return result;
}

// Per-frame check to close the tooltip when not needed. This catches the case where the mouse leaves a hooked button
// without a final HandleMouseMove call landing on another hooked window.
static void mouseover_process_frame() {
  auto &item_displays = ZealService::get_instance()->item_displays;
  bool active = s_mouseover_wnd && s_mouseover_wnd->IsActivated && item_displays &&
                item_displays->setting_mouseover_tooltips.get();

  // Every exit below restores the native tooltip, so suppression set by the mouse move hooks
  // can never be left stuck on (e.g. hovering a slot while the window is released for zoning)
  auto *wnd_mgr = active ? Zeal::Game::get_wnd_manager() : nullptr;
  auto *hovered = wnd_mgr ? reinterpret_cast<Zeal::GameUI::BasicWnd *>(wnd_mgr->Hovered) : nullptr;
  if (!hovered) {
    suppress_native_tooltip(false);
    hide_mouseover_window();
    return;
  }

  bool is_inv_slot = (hovered->vtbl == Zeal::GameUI::InvSlotWnd::default_vtable);

  bool is_spell_gem = s_original_spell_gem_wnd_mouse_move &&
                      hovered->vtbl->HandleMouseMove == reinterpret_cast<LPVOID>(SpellGemWnd_HandleMouseMove);

  auto *parent = reinterpret_cast<Zeal::GameUI::BasicWnd *>(hovered->ParentWnd);
  auto *buff_wnd = reinterpret_cast<Zeal::GameUI::BuffWindow *>(Zeal::Game::Windows->BuffWindowNORMAL);
  auto *spell_book = reinterpret_cast<Zeal::GameUI::BasicWnd *>(Zeal::Game::Windows->SpellBook);

  bool is_buff_button = (buff_wnd && parent == reinterpret_cast<Zeal::GameUI::BasicWnd *>(buff_wnd)) ||
                        (s_song_wnd && parent == reinterpret_cast<Zeal::GameUI::BasicWnd *>(s_song_wnd));

  bool is_spell_book_icon = (spell_book && parent == spell_book) &&
                            s_spell_book_icon_slot_map.find(hovered) != s_spell_book_icon_slot_map.end();

  bool over_valid = is_inv_slot || is_spell_gem || is_spell_book_icon || is_buff_button;

  // Suppress the native name-only hover tooltip on windows the mouseover tooltip already covers;
  // its global hover timer would otherwise still pop it on top of/after ours.
  suppress_native_tooltip(over_valid);
  if (!over_valid) hide_mouseover_window();
}

// Returns the mouseover window behavior to it's disabled state. Re-enables storage/position persistence,
// reloads its saved position, and deactivates if currently shown.
static void release_mouseover_wnd() {
  suppress_native_tooltip(false);
  if (!s_mouseover_wnd) return;
  if (s_mouseover_wnd->IconBtn) s_mouseover_wnd->IconBtn->IsVisible = true;
  s_mouseover_wnd->EnableINIStorage |= 0x1;
  auto vtable = static_cast<Zeal::GameUI::ItemDisplayVTable *>(s_mouseover_wnd->vtbl);
  auto load_ini = reinterpret_cast<void(__fastcall *)(Zeal::GameUI::ItemDisplayWnd *, int)>(vtable->LoadIniInfo);
  load_ini(s_mouseover_wnd, 0);
  if (s_mouseover_wnd->IsVisible) s_mouseover_wnd->Deactivate();
  s_mouseover_wnd = nullptr;
  s_mouseover_slot_index = -1;
}

// Settings callback for the mouseover tooltips toggle. Enabling commandeers ItemWnd the same
// way mouseover_init_ui() does (covers toggling on after UI init), Disabling releases it back
void ItemDisplay::set_mouseover_tooltips(bool enabled) {
  setting_mouseover_tooltips.set(enabled);
  if (enabled) {
    if (!s_mouseover_wnd && Zeal::Game::Windows && Zeal::Game::Windows->ItemWnd)
      acquire_mouseover_wnd(Zeal::Game::Windows->ItemWnd);
  } else {
    release_mouseover_wnd();
  }
}

// Sets up everything mouseover tooltips need: commandeers an ItemDisplayWnd, patches the
// shared vtables for buff/song buttons, spell book icons, and spell gems, hooks
// BuffWindow's WndNotification, and builds the spell book icon slot map. Resets all
// mouseover state first, so this is safe to call again (e.g. on zoning) after
// mouseover_clean_ui().
void mouseover_init_ui() {
  Zeal::Game::print_debug("[Mouseover] mouseover_init_ui called");
  s_hooked_buff_btn_vtbl = nullptr;
  s_hooked_spell_gem_vtbl = nullptr;
  s_hooked_buff_wnd_vtbl = nullptr;
  s_mouseover_wnd = nullptr;
  s_mouseover_slot_index = -1;
  s_in_mouseover_set_item = false;
  s_song_wnd = nullptr;
  s_spell_book_icon_slot_map.clear();
  s_original_spell_gem_wnd_mouse_move = nullptr;
  s_original_buff_button_wnd_mouse_move = nullptr;
  s_original_buff_wnd_notification = nullptr;

  auto &item_displays = ZealService::get_instance()->item_displays;

  // Take over the default ItemWnd if mouseover tooltips are enabled
  if (item_displays && item_displays->setting_mouseover_tooltips.get() && Zeal::Game::Windows &&
      Zeal::Game::Windows->ItemWnd)
    acquire_mouseover_wnd(Zeal::Game::Windows->ItemWnd);

  // Patch the shared ButtonWnd vtable once from the first buff button.
  // Covers both buff buttons and spell book icons since they share a vtable.
  auto *buff_wnd_live = reinterpret_cast<Zeal::GameUI::BuffWindow *>(Zeal::Game::Windows->BuffWindowNORMAL);
  if (buff_wnd_live) {
    for (int i = 0; i < GAME_NUM_BUFFS; i++) {
      if (buff_wnd_live->BuffButtonWnd[i]) {
        auto *btn_vtbl = buff_wnd_live->BuffButtonWnd[i]->vtbl;
        if (btn_vtbl->HandleMouseMove != reinterpret_cast<LPVOID>(ButtonWnd_HandleMouseMove)) {
          s_original_buff_button_wnd_mouse_move = btn_vtbl->HandleMouseMove;
          s_hooked_buff_btn_vtbl = btn_vtbl;
          mem::unprotect_memory(btn_vtbl, sizeof(*btn_vtbl));
          btn_vtbl->HandleMouseMove = ButtonWnd_HandleMouseMove;
          btn_vtbl->HandleWheelMove = ButtonWnd_HandleWheelMove;
          mem::reset_memory_protection(btn_vtbl);
        }
        break;
      }
    }

    // Hook WndNotification on BuffWindow for alt+click spell info.
    if (!s_original_buff_wnd_notification) {
      auto *buff_vtbl = buff_wnd_live->vtbl;
      s_original_buff_wnd_notification = buff_vtbl->WndNotification;
      s_hooked_buff_wnd_vtbl = reinterpret_cast<Zeal::GameUI::SidlScreenWndVTable *>(buff_vtbl);
      mem::unprotect_memory(buff_vtbl, sizeof(*buff_vtbl));
      buff_vtbl->WndNotification = BuffWnd_WndNotification;
      mem::reset_memory_protection(buff_vtbl);
    }
  }

  // Hook SpellGemWnd via CastSpellWnd's SpellSlots
  auto *cast_wnd = Zeal::Game::Windows->SpellGems;
  if (cast_wnd) {
    for (int i = 0; i < 8; i++) {
      if (cast_wnd->SpellSlots[i]) {
        auto *gem_vtbl = cast_wnd->SpellSlots[i]->vtbl;
        if (gem_vtbl->HandleMouseMove != reinterpret_cast<LPVOID>(SpellGemWnd_HandleMouseMove)) {
          s_hooked_spell_gem_vtbl = gem_vtbl;
          s_original_spell_gem_wnd_mouse_move = gem_vtbl->HandleMouseMove;
          mem::unprotect_memory(gem_vtbl, sizeof(*gem_vtbl));
          gem_vtbl->HandleMouseMove = SpellGemWnd_HandleMouseMove;
          gem_vtbl->HandleWheelMove = SpellGemWnd_HandleWheelMove;
          mem::reset_memory_protection(gem_vtbl);
        }
        break;
      }
    }
  }

  // Build spell book icon slot map.
  s_spell_book_icon_slot_map.clear();
  auto *spell_book = Zeal::Game::Windows->SpellBook;
  if (spell_book) {
    for (int i = 0; i < 16; i++) {
      std::string name = std::format("SBW_Spell{}", i);
      auto *spell_btn = spell_book->GetChildItem(name, false);
      if (spell_btn) s_spell_book_icon_slot_map[spell_btn] = i;
    }
  }
}

// Reverses mouseover_init_ui(): restores patched vtables, releases s_mouseover_wnd, and
// clears cached state. Called on UI cleanup and before re-running mouseover_init_ui() on zoning.
void mouseover_clean_ui() {
  Zeal::Game::print_debug("[Mouseover] mouseover_clean_ui called");
  s_in_mouseover_set_item = false;

  if (s_hooked_buff_btn_vtbl && s_original_buff_button_wnd_mouse_move) {
    mem::unprotect_memory(s_hooked_buff_btn_vtbl, sizeof(*s_hooked_buff_btn_vtbl));
    s_hooked_buff_btn_vtbl->HandleMouseMove = s_original_buff_button_wnd_mouse_move;
    // Restore HandleWheelMove to the known default stub (assumed, not saved separately)
    s_hooked_buff_btn_vtbl->HandleWheelMove = kDefaultHandleWheelMove;
    mem::reset_memory_protection(s_hooked_buff_btn_vtbl);
    s_hooked_buff_btn_vtbl = nullptr;
  }

  if (s_hooked_spell_gem_vtbl && s_original_spell_gem_wnd_mouse_move) {
    mem::unprotect_memory(s_hooked_spell_gem_vtbl, sizeof(*s_hooked_spell_gem_vtbl));
    s_hooked_spell_gem_vtbl->HandleMouseMove = s_original_spell_gem_wnd_mouse_move;
    // See the equivalent restore above - same shared default wheel handler.
    s_hooked_spell_gem_vtbl->HandleWheelMove = kDefaultHandleWheelMove;
    mem::reset_memory_protection(s_hooked_spell_gem_vtbl);
    s_hooked_spell_gem_vtbl = nullptr;
  }

  if (s_hooked_buff_wnd_vtbl && s_original_buff_wnd_notification) {
    mem::unprotect_memory(s_hooked_buff_wnd_vtbl, sizeof(*s_hooked_buff_wnd_vtbl));
    s_hooked_buff_wnd_vtbl->WndNotification = s_original_buff_wnd_notification;
    mem::reset_memory_protection(s_hooked_buff_wnd_vtbl);
    s_hooked_buff_wnd_vtbl = nullptr;
  }

  release_mouseover_wnd();
  s_spell_book_icon_slot_map.clear();
  s_song_wnd = nullptr;
  s_original_spell_gem_wnd_mouse_move = nullptr;
  s_original_buff_button_wnd_mouse_move = nullptr;
  s_original_buff_wnd_notification = nullptr;
}

// Release when loading/zoning etc
void mouseover_deactivate_ui() { release_mouseover_wnd(); }

// Prints a brief overview of /mouseover, used for no-args and unrecognized subcommands
static void print_mouseover_usage() {
  Zeal::Game::print_chat("Mouseover tooltips dynamically resize to fit their content, up to a max width/height.");
  Zeal::Game::print_chat("  /mouseover on|off - enables or disables mouseover tooltips.");
  Zeal::Game::print_chat("  /mouseover width|height - shows the current maximum width or height");
  Zeal::Game::print_chat("  /mouseover width <pixels> - sets the maximum width (0 to reset)");
  Zeal::Game::print_chat("  /mouseover height <pixels> - sets the maximum height (0 to reset)");
}

// Shared handler for the /mouseover width and /mouseover height subcommands.
static void handle_mouseover_size_command(std::vector<std::string> &args, const char *label,
                                          ZealSetting<int> &setting, int native_default) {
  if (args.size() >= 3) {
    int value = 0;
    if (Zeal::String::tryParse(args[2], &value, true) && value >= 0) {
      setting.set(value);
      if (value > 0)
        Zeal::Game::print_chat("Mouseover max %s set to %d.", label, value);
      else
        Zeal::Game::print_chat("Mouseover max %s reset to the default (%d).", label, native_default);
      return;
    }
    Zeal::Game::print_chat("Invalid value, usage: /mouseover %s <pixels>", label);
    return;
  }

  int value = setting.get();
  if (value > 0)
    Zeal::Game::print_chat("Mouseover max %s: %d", label, value);
  else
    Zeal::Game::print_chat("Mouseover max %s: default (%d)", label, native_default);
}

static bool handle_mouseover_command(std::vector<std::string> &args) {
  auto &item_displays = ZealService::get_instance()->item_displays;
  if (!item_displays) return true;

  if (args.size() >= 2 && Zeal::String::compare_insensitive(args[1], "on")) {
    item_displays->set_mouseover_tooltips(true);
    Zeal::Game::print_chat("Mouseover tooltips: ON");
    return true;
  }
  if (args.size() >= 2 && Zeal::String::compare_insensitive(args[1], "off")) {
    item_displays->set_mouseover_tooltips(false);
    Zeal::Game::print_chat("Mouseover tooltips: OFF");
    return true;
  }
  if (args.size() >= 2 && Zeal::String::compare_insensitive(args[1], "width")) {
    handle_mouseover_size_command(args, "width", item_displays->setting_mouseover_max_width, s_native_max_width);
    return true;
  }
  if (args.size() >= 2 && Zeal::String::compare_insensitive(args[1], "height")) {
    handle_mouseover_size_command(args, "height", item_displays->setting_mouseover_max_height, s_native_max_height);
    return true;
  }

  print_mouseover_usage();
  return true;
}

// One-time setup from ItemDisplay's constructor. Hooks InvSlotWnd's vtable once (shared by
// all inventory slots for the client's lifetime, unlike the buff/gem vtables) and registers
// the per-frame validity check.
void mouseover_init_hooks(ZealService *zeal) {
  zeal->commands_hook->Add("/mouseover", {}, "Toggles mouseover tooltips and configures their max width/height.",
                           [](std::vector<std::string> &args) { return handle_mouseover_command(args); });

  // Hook HandleMouseMove and HandleWheelMove on InvSlotWnd.
  auto *inv_slot_wnd_vtable = Zeal::GameUI::InvSlotWnd::default_vtable;
  s_original_inv_slot_mouse_move = inv_slot_wnd_vtable->HandleMouseMove;
  s_original_inv_slot_wheel_move = inv_slot_wnd_vtable->HandleWheelMove;
  mem::unprotect_memory(inv_slot_wnd_vtable, sizeof(*inv_slot_wnd_vtable));
  inv_slot_wnd_vtable->HandleMouseMove = InvSlotWnd_HandleMouseMove;
  inv_slot_wnd_vtable->HandleWheelMove = InvSlotWnd_HandleWheelMove;
  mem::reset_memory_protection(inv_slot_wnd_vtable);

  zeal->callbacks->AddGeneric([]() { mouseover_process_frame(); }, callback_type::MainLoop);
}

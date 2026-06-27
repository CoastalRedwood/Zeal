#include "mouseover_display.h"

#include <format>
#include <unordered_map>

#include "callbacks.h"
#include "game_addresses.h"
#include "game_functions.h"
#include "game_ui.h"
#include "hook_wrapper.h"
#include "item_display.h"
#include "memory.h"
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

// Whether the current SetItem call is populating the mouseover tooltip window (item_display.cpp).
bool mouseover_in_set_item() { return s_in_mouseover_set_item; }

// The window used to render mouseover tooltips, or nullptr if mouseover tooltips are
// disabled/not yet initialized.
Zeal::GameUI::ItemDisplayWnd *mouseover_get_wnd() { return s_mouseover_wnd; }

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
  if (x + current_width > screen_w) x = mouse_x - current_width - kTooltipCursorOffset;
  if (y + current_height > screen_h) y = mouse_y - current_height - kTooltipCursorOffset;
  if (y + current_height > screen_h) y = screen_h - current_height;

  // Move tooltip
  s_mouseover_wnd->Location.Left = x;
  s_mouseover_wnd->Location.Top = y;
  s_mouseover_wnd->Location.Right = x + current_width;
  s_mouseover_wnd->Location.Bottom = y + current_height;
  s_mouseover_wnd->BringToFront();
}

// Use mouse wheel to scroll tooltip
static void forward_wheel_to_tooltip(int mouse_x, int mouse_y, int wheel_delta, int unknown) {

  if (s_mouseover_wnd && s_mouseover_slot_index != -1 && s_mouseover_wnd->ItemDescription) {
    reinterpret_cast<int(__thiscall *)(Zeal::GameUI::BasicWnd *, int, int, int, int)>(
        s_mouseover_wnd->ItemDescription->vtbl->HandleWheelMove)(s_mouseover_wnd->ItemDescription, mouse_x, mouse_y,
                                                                 wheel_delta, unknown);
  }
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
  reinterpret_cast<void(__thiscall *)(Zeal::GameUI::ItemDisplayWnd *)>(0x0042359a)(s_mouseover_wnd);
  // SetItem/SetSpell always re-show the icon; force it back off for the mouseover window
  // so the tooltip stays compact (the icon is already under the mouse anyway)
  if (s_mouseover_wnd->IconBtn) s_mouseover_wnd->IconBtn->IsVisible = false;

  Zeal::Game::Windows->ItemWnd = default_wnd;

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
  int result = reinterpret_cast<int(__thiscall *)(Zeal::GameUI::InvSlotWnd *, int, int, unsigned int)>(
      s_original_inv_slot_mouse_move)(wnd, mouse_x, mouse_y, flags);

  if (!ZealService::get_instance()->item_displays->setting_mouseover_tooltips.get()) return result;

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
  int result = reinterpret_cast<int(__thiscall *)(Zeal::GameUI::SpellGemWnd *, int, int, unsigned int)>(
      s_original_spell_gem_wnd_mouse_move)(wnd, mouse_x, mouse_y, flags);

  if (!ZealService::get_instance()->item_displays->setting_mouseover_tooltips.get()) return result;

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

  if (!ZealService::get_instance()->item_displays->setting_mouseover_tooltips.get()) return result;

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
  if (!s_mouseover_wnd || !s_mouseover_wnd->IsActivated) return;

  auto &item_displays = ZealService::get_instance()->item_displays;
  if (!item_displays || !item_displays->setting_mouseover_tooltips.get()) {
    hide_mouseover_window();
    return;
  }

  auto *wnd_mgr = Zeal::Game::get_wnd_manager();
  if (!wnd_mgr) return;

  auto *hovered = reinterpret_cast<Zeal::GameUI::BasicWnd *>(wnd_mgr->Hovered);
  if (!hovered) {
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

  if (!over_valid) hide_mouseover_window();
}

// Returns the mouseover window behavior to it's disabled state. Re-enables storage/position persistence,
// reloads its saved position, and deactivates if currently shown.
static void release_mouseover_wnd() {
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
    if (!s_mouseover_wnd && Zeal::Game::Windows && Zeal::Game::Windows->ItemWnd) {
      s_mouseover_wnd = Zeal::Game::Windows->ItemWnd;
      s_mouseover_wnd->EnableINIStorage &= ~0x1;
      s_mouseover_wnd->Activate();
      hide_mouseover_window();
    }
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
      Zeal::Game::Windows->ItemWnd) {
    s_mouseover_wnd = Zeal::Game::Windows->ItemWnd;
    s_mouseover_wnd->EnableINIStorage &= ~0x1;
    s_mouseover_wnd->Activate();
    hide_mouseover_window();
  }

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

// One-time setup from ItemDisplay's constructor. Hooks InvSlotWnd's vtable once (shared by
// all inventory slots for the client's lifetime, unlike the buff/gem vtables) and registers
// the per-frame validity check.
void mouseover_init_hooks(ZealService *zeal) {
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

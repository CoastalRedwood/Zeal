#include "ui_hide_fake_slots.h"

#include "callbacks.h"
#include "game_addresses.h"
#include "game_functions.h"
#include "game_ui.h"
#include "zeal.h"

typedef int(__thiscall *HandleLButtonUp_fn)(Zeal::GameUI::InvSlotWnd *, int, int, unsigned int);
static HandleLButtonUp_fn next_HandleLButtonUp = nullptr;

static int __fastcall InvSlotWnd_HandleLButtonUp(Zeal::GameUI::InvSlotWnd *wnd,
                                                  void *unused_edx, int mouse_x,
                                                  int mouse_y, unsigned int flags) {
  // Trigger a visibility update when a pack slot is clicked,
  //   for if a bag was added/removed

  int result = next_HandleLButtonUp(wnd, mouse_x, mouse_y, flags);

  auto *zeal = ZealService::get_instance();
  if (!zeal->ui_hide_fake_slots || !zeal->ui_hide_fake_slots->Enabled.get()) return result;

  int slot_id = wnd->SlotID;

  // Only run if a valid bag slot was clicked
  if (slot_id >= GAME_PACKS_SLOTS_START && slot_id <= GAME_PACKS_SLOTS_END) {
    // Call with delay so that game has time to update
    ZealService::get_instance()->callbacks->AddDelayed([]() {
      auto zeal = ZealService::get_instance();
      if (zeal->ui_hide_fake_slots)
        zeal->ui_hide_fake_slots->update_slot_visibility();
    }, 50);
  }

  return result;
}

void UI_HideFakeSlots::update_slot_visibility() {
  // Hides or shows inventory slots based on the parent bag's capacity

  auto *inv_slot_mgr = Zeal::Game::Windows->InvSlotMgr;
  if (!inv_slot_mgr) return;
  auto *char_info = Zeal::Game::get_char_info();
  if (!char_info) return;

  // Walk the slot array and update any container sub-slots found
  for (int i = 0; i < 450; ++i) {  // 450 as per struct CInvSlotMgr
    auto *inv_slot = inv_slot_mgr->InvSlots[i];

    // Skip non-relevant slots
    if ((DWORD)inv_slot < 0x10000) continue;
    if (IsBadReadPtr(inv_slot, sizeof(Zeal::GameUI::InvSlot))) continue;
    if (IsBadReadPtr(inv_slot->invSlotWnd, sizeof(Zeal::GameUI::InvSlotWnd))) continue;

    int slot_id = inv_slot->invSlotWnd->SlotID;
    if (slot_id < GAME_CONTAINER_SLOTS_START || slot_id > GAME_CONTAINER_SLOTS_END) continue;

    int offset    = slot_id - GAME_CONTAINER_SLOTS_START;
    int bag_slot  = offset / GAME_NUM_CONTAINER_SLOTS;
    int bag_index = offset % GAME_NUM_CONTAINER_SLOTS;

    auto *bag_item = char_info->InventoryPackItem[bag_slot];
    int capacity = (bag_item && bag_item->Type == 1) ? bag_item->Container.Capacity : 0;

    bool show = !Enabled.get() || (bag_index < capacity);
    inv_slot->invSlotWnd->show(show ? 1 : 0, false);
  }
}

UI_HideFakeSlots::UI_HideFakeSlots(ZealService *zeal)
    : Enabled(false, "Zeal", "HideFakeSlots", false, [this](const bool &value) {
        update_slot_visibility(); }) {  // Unhide slots if changed from Enabled to Disabled

  // Save current vtable
  auto *vtable = Zeal::GameUI::InvSlotWnd::default_vtable;
  next_HandleLButtonUp = (HandleLButtonUp_fn)(vtable->HandleLButtonUp);

  mem::unprotect_memory(vtable, sizeof(*vtable));
  vtable->HandleLButtonUp = InvSlotWnd_HandleLButtonUp;
  mem::reset_memory_protection(vtable);

  // Update slot visibility on a timer as a fallback
  zeal->callbacks->AddGeneric([this]() {
    if (!Enabled.get()) return;

    static DWORD last_check = 0;
    DWORD now = GetTickCount();
    if (now - last_check > 2000) {
      last_check = now;
      update_slot_visibility();
    }
  }, callback_type::MainLoop);
}


UI_HideFakeSlots::~UI_HideFakeSlots() {
  auto *vtable = Zeal::GameUI::InvSlotWnd::default_vtable;
  mem::unprotect_memory(vtable, sizeof(*vtable));
  vtable->HandleLButtonUp = (LPVOID)next_HandleLButtonUp;
  mem::reset_memory_protection(vtable);

  Enabled.set(false, false);  // Disable without saving to ini
  update_slot_visibility();
}
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
  if (!zeal->ui_hide_fake_slots->Enabled.get()) return result;

  int slot_id = wnd->SlotID;

  // Only run if a valid bag slot was clicked
  if (slot_id >= GAME_PACKS_SLOTS_START && slot_id <= GAME_PACKS_SLOTS_END) {
    zeal->callbacks->AddDelayed([]() {
      auto *z = ZealService::get_instance();
      if (z->ui_hide_fake_slots)
        z->ui_hide_fake_slots->check_and_update();
    }, 50); // Call with delay so that game has time to update
  }

  return result;
}

void UI_HideFakeSlots::check_and_update() {
  // Call an update if the bag slots have changed
  
  if (!Enabled.get()) return;

  auto *char_info = Zeal::Game::get_char_info();
  if (!char_info) return;

  bool changed = false;
  for (int i = 0; i < GAME_NUM_INVENTORY_PACK_SLOTS; ++i) {
    auto *bag = char_info->InventoryPackItem[i];
    int id = (bag && bag->Type == 1) ? bag->ID : 0;
    if (id != cached_pack_slot_ids[i]) {
      cached_pack_slot_ids[i] = id;
      changed = true;
    }
  }

  if (changed)
    update_slot_visibility();
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

    // Determine the index of the bag that contains this item
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

  // Initial update when entring world
  zeal->callbacks->AddGeneric([this]() { check_and_update(); }, callback_type::InitUI);

  // Trigger update when an item is received from the server (purchase/quest/loot/etc)
  zeal->callbacks->AddPacket([this](UINT opcode, char *buffer, UINT len) -> bool {
    if (opcode == 0x4031 && this->Enabled.get())
      check_and_update();
    return false;
  }, callback_type::WorldMessagePost);
}

UI_HideFakeSlots::~UI_HideFakeSlots() {
  auto *vtable = Zeal::GameUI::InvSlotWnd::default_vtable;
  mem::unprotect_memory(vtable, sizeof(*vtable));
  vtable->HandleLButtonUp = (LPVOID)next_HandleLButtonUp;
  mem::reset_memory_protection(vtable);

  Enabled.set(false, false);  // Disable without saving to ini
  update_slot_visibility();
}
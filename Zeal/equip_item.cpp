#include "equip_item.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "game_addresses.h"
#include "game_functions.h"
#include "hook_wrapper.h"
#include "melody.h"
#include "zeal.h"

static void __fastcall CInvSlot_HandleRButtonUp(Zeal::GameUI::InvSlot *inv_slot, int unused_edx, int x, int y) {
  if (ZealService::get_instance()->equip_item_hook->HandleRButtonUp(inv_slot)) {
    return;
  }
  ZealService::get_instance()->hooks->hook_map["CInvSlot_HandleRButtonUp"]->original(CInvSlot_HandleRButtonUp)(
      inv_slot, unused_edx, x, y);
}

bool EquipItem::HandleRightClickActivation(Zeal::GameUI::InvSlot *inv_slot) {
  if (!inv_slot) return false;
  auto item = reinterpret_cast<const Zeal::GameStructures::GAMEITEMINFO *>(inv_slot->Item);
  auto wnd = inv_slot->invSlotWnd;
  if (!item || !wnd) return false;
  int slot_id = wnd->SlotID;

  // We want to let the client handle all of the non-click effects like opening bags, consuming food,
  // requesting loot, etc, so we filter aggressively here.

  // Let client handle any slots that are not part of player inventory.
  bool is_equipped = (slot_id >= GAME_EQUIPMENT_SLOTS_START && slot_id <= GAME_EQUIPMENT_SLOTS_END);
  bool is_pack = (slot_id >= GAME_PACKS_SLOTS_START && slot_id <= GAME_PACKS_SLOTS_END);
  bool is_bagged = (slot_id >= GAME_CONTAINER_SLOTS_START && slot_id <= GAME_CONTAINER_SLOTS_END);
  if (!is_equipped && !is_pack && !is_bagged) return false;

  // Use the same check that use_item() (and also eqgame.dll) uses to verify it is a valid clicky.
  if (!Zeal::Game::is_valid_item_to_use(item, is_equipped, false)) return false;

  // It is a valid clicky so it should get the right click attempt and we will absorb the click.
  // First try queuing it into a Bard's Melody otherwise execute it now with use_item().
  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();
  if (char_info && char_info->Class == Zeal::GameEnums::ClassTypes::Bard &&
      ZealService::get_instance()->melody->use_item(slot_id))
    return true;  // Melody has queued the click so snuff it from further processing.

  Zeal::Game::use_item(slot_id);
  return true;  // Ignore the result of use_item() and return true here to snuff further click processing.
}

bool EquipItem::HandleRButtonUp(Zeal::GameUI::InvSlot *src_inv_slot) {
  Zeal::GameUI::CXWndManager *wnd_mgr = Zeal::Game::get_wnd_manager();
  if (!wnd_mgr) {
    return false;
  }

  // Get the currently held keyboard modifiers
  BYTE alt = wnd_mgr->AltKeyState;
  BYTE shift = wnd_mgr->ShiftKeyState;
  BYTE ctrl = wnd_mgr->ControlKeyState;

  // Right click activation of clickies is allowed when shift and control are not
  // held and the user selected state of the alt key matches. This call will return
  // false if there was no attempt to activate allowing the client handler to deal with it.
  // Note that items in the equipped slots will be activated with or without the modifier
  // since the client does it in one case and the Zeal handler in the other, but only the
  // modifier path will queue it into melody.
  bool clicky_modifier = !shift && !ctrl && ((alt != 0) == setting_use_alt_for_clicky.get());
  if (clicky_modifier && setting_click_from_inventory.get()) {
    return HandleRightClickActivation(src_inv_slot);
  }

  // Nothing downstream is required if RightClickToEquip is disabled
  if (!Enabled.get()) return false;

  // Slot ID for bagged items is 250 + (bag_i*10) + (contents_i) = [250...329]
  auto src_wnd = src_inv_slot ? src_inv_slot->invSlotWnd : nullptr;
  if (!src_wnd) {
    return false;
  }
  int src_slot_id = src_wnd->SlotID;
  if (src_slot_id < GAME_CONTAINER_SLOTS_START || src_slot_id > GAME_CONTAINER_SLOTS_END) {
    return false;  // Item is not in an inventory bag.
  }

  int src_container_i = (src_slot_id - GAME_CONTAINER_SLOTS_START) / GAME_NUM_CONTAINER_SLOTS;
  if (src_container_i < 0 || src_container_i >= GAME_NUM_INVENTORY_PACK_SLOTS) {
    return false;  // Shouldn't happen. Ensure bag is 0..7
  }

  Zeal::GameStructures::GAMECHARINFO *c = Zeal::Game::get_char_info();
  if (!c || c->CursorItem || c->CursorCopper || c->CursorGold || c->CursorPlatinum || c->CursorSilver) {
    return false;  // Fast-fail. We are holding something.
  }

  Zeal::GameStructures::GAMEITEMINFO *src_container = c->InventoryPackItem[src_container_i];
  if (!src_container || src_container->Type != 1) {
    return false;  // Can't locate source bag info.
  }

  Zeal::GameStructures::GAMEITEMINFO *src_item = (Zeal::GameStructures::GAMEITEMINFO *)src_inv_slot->Item;
  if (!src_item || src_item->Type != 0) {
    return false;  // Item is not a common item.
  }

  bool can_class_race_equip = Zeal::Game::can_use_item(c, src_item);
  if (!can_class_race_equip) {
    return false;  // Item is not usable by our race/class/deity
  }

  // ------------------------------------------
  // -- Find what slots will accept the item --
  // ------------------------------------------

  WORD src_item_id = src_item->ID;
  std::vector<std::pair<int, WORD>> dst_slots;  // Pairs of {invSlot,itemID} for the valid destination slots

  // First look for empty slots, prioritize those.
  for (int i : EQUIP_PRIORITY_ORDER) {
    if (!c->InventoryItem[i] && Zeal::Game::can_item_equip_in_slot(c, src_item, i + 1)) {
      dst_slots.push_back(std::make_pair<int, WORD>(i + 1, 0));
    }
  }

  // Looks for any valid slot if no empty ones were available.
  if (dst_slots.empty()) {
    for (int i : EQUIP_PRIORITY_ORDER) {
      if (c->InventoryItem[i] && Zeal::Game::can_item_equip_in_slot(c, src_item, i + 1)) {
        if (c->InventoryItem[i]->Type != 0) {
          continue;  // Equipped item isn't a normal item? Skip it.
        }
        if (c->InventoryItem[i]->ID == src_item_id) {
          if (src_item->Common.IsStackable) {
            if (c->InventoryItem[i]->Common.StackCount >= 20) {
              continue;  // Equipepd item is already a full stack of the same item. Skip it.
            }
          } else if (src_item->Common.Charges == c->InventoryItem[i]->Common.Charges) {
            continue;  // Equiped item is the same item. Skip it.
          }
        } else if (src_container->Container.SizeCapacity < c->InventoryItem[i]->Size) {
          continue;  // Equipped item won't fit into this bag when we swap. Skip it.
        }
        dst_slots.push_back(std::make_pair(i + 1, c->InventoryItem[i]->ID));
      }
    }
    if (dst_slots.empty()) {
      return false;  // Couldn't find any slot to equip the item in.
    }
  }

  // -----------------------------------------
  // -- Preparing to Swap, Picking the Slot --
  // -----------------------------------------

  // User controlled behavior:
  // Based on what keys are held (nothing, Shift, Ctrl, Shift+Ctrl), use the 1st, 2nd, 3rd, or 4th equippable slot,
  // respectively
  int i = (shift ? 1 : 0) + (ctrl ? 2 : 0);
  if (i >= dst_slots.size()) {
    i = dst_slots.size() - 1;
  }

  int dst_inv_slot_id = dst_slots[i].first;
  WORD dst_item_id = dst_slots[i].second;
  Zeal::GameUI::InvSlot *dst_inv_slot = GetInventorySlot(dst_inv_slot_id);
  if (!dst_inv_slot) {
    return false;  // Destination slot not found.
  }

  // --------------------
  // -- Begin Swapping --
  // --------------------

  // Pre-swap:
  // - Force the keyboard modifiers 'SHIFT'.
  // - This ensure we pick up stacked items when sending LeftClick events.
  wnd_mgr->ShiftKeyState = 1;
  wnd_mgr->ControlKeyState = 0;
  wnd_mgr->AltKeyState = 0;

  // (1) Pickup the bagged item.
  src_inv_slot->HandleLButtonUp();
  // Now we should be holding the item to swap on our Cursor
  if (c->CursorItem == src_item) {
    // (2) Equip it in the destination slot.
    dst_inv_slot->HandleLButtonUp();
    // Now we should be holding the swapped-out item instead (if there was one equipped).
    if (dst_item_id && c->CursorItem && c->CursorItem->ID == dst_item_id) {
      // (3) Put the swapped-out item in the bag slot we just emptied.
      src_inv_slot->HandleLButtonUp();
    }
  }

  // Done. Restore the keyboard flags.
  wnd_mgr->ShiftKeyState = shift;
  wnd_mgr->ControlKeyState = ctrl;
  wnd_mgr->AltKeyState = alt;
  return true;
}

Zeal::GameUI::InvSlot *EquipItem::GetInventorySlot(int inv_slot_id) {
  if (inv_slot_id < 1 || inv_slot_id > 22) {
    return nullptr;  // Not an equippable inventory slot
  }
  Zeal::GameUI::CInvSlotMgr *inv_slot_mgr = Zeal::Game::Windows->InvSlotMgr;
  if (!inv_slot_mgr) {
    return nullptr;
  }
  Zeal::GameUI::InvSlot *inv_slot = inv_slot_mgr->FindInvSlot(inv_slot_id);
  if (!inv_slot) {
    return nullptr;  // Slot Unavailable
  }
  Zeal::GameUI::InvSlotWnd *wnd = inv_slot->invSlotWnd;
  if (!wnd || wnd->SlotID != inv_slot_id) {
    return nullptr;  // Invalid window
  }
  return inv_slot;
}

EquipItem::EquipItem(ZealService *zeal) {
  if (!Zeal::Game::is_new_ui()) {
    return;
  }
  ZealService::get_instance()->hooks->Add("CInvSlot_HandleRButtonUp", 0x422804, CInvSlot_HandleRButtonUp,
                                          hook_type_detour);
}

EquipItem::~EquipItem() {}

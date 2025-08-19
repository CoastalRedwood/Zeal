#include "ui_inspect.h"

#include <format>

#include "callbacks.h"
#include "game_addresses.h"
#include "game_functions.h"
#include "game_structures.h"
#include "game_ui.h"
#include "items.h"
#include "memory.h"
#include "ui_manager.h"
#include "zeal.h"

static int __fastcall InspectItemClickDown(Zeal::GameUI::InvSlotWnd *pWnd, int unused, Zeal::GameUI::CXPoint pt,
                                           unsigned int flag) {
  if (pWnd->invSlot && pWnd->invSlot->Item && Zeal::Game::get_wnd_manager()->AltKeyState) {
    short id = Zeal::Items::lookup(pWnd->invSlot->Item->Name);
    if (id > 0) {
      Zeal::Packets::ItemViewRequest_Struct tmp;
      memset(&tmp, 0, sizeof(tmp));
      tmp.item_id = id;
      Zeal::Game::send_message(Zeal::Packets::opcodes::ItemLinkResponse, (int *)&tmp, sizeof(tmp), 0);
    } else
      Zeal::Game::print_chat("Unable to find id for item: %s", pWnd->invSlot->Item->Name);
  }
  return 0;
}

static void __fastcall InvSlotDestructor(Zeal::GameUI::BasicWnd *pWnd, int unusedEDX, BYTE delete_me) {
  pWnd->DeleteCustomVTable((Zeal::GameUI::BaseVTable *)ZealService::get_instance()->ui->inspect->orig_vtable);
  pWnd->Deconstruct(delete_me);  // Calls the vtable's destructor.
}

void ui_inspect::InitUI() {
  for (int i = 1; i < 22; i++) {
    std::string slot_name = std::format("InvSlot{}", i);
    Zeal::GameUI::InvSlotWnd *wnd = (Zeal::GameUI::InvSlotWnd *)Zeal::Game::Windows->Inspect->GetChildItem(slot_name);
    if (wnd) {
      orig_vtable = wnd->vtbl;
      wnd->SetupCustomVTable(sizeof(Zeal::GameUI::ButtonWndVTable));
      wnd->vtbl->HandleLButtonDown = InspectItemClickDown;
      wnd->vtbl->Deconstructor = InvSlotDestructor;
    }
  }
}

ui_inspect::ui_inspect(ZealService *zeal, UIManager *mgr) {
  ui = mgr;
  zeal->callbacks->AddGeneric([this]() { InitUI(); }, callback_type::InitUI);
}

ui_inspect::~ui_inspect() {}

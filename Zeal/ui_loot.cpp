#include "ui_loot.h"

#include "game_addresses.h"
#include "game_functions.h"
#include "game_structures.h"
#include "looting.h"
#include "string_util.h"
#include "zeal.h"

static int __fastcall LinkAllButtonDown(Zeal::GameUI::LootWnd *pWnd, int unused, Zeal::GameUI::CXPoint pt,
                                        unsigned int flag) {
  int rval = reinterpret_cast<int(__fastcall *)(Zeal::GameUI::LootWnd * pWnd, int unused, Zeal::GameUI::CXPoint pt,
                                                unsigned int flag)>(0x0595330)(pWnd, unused, pt, flag);
  ZealService::get_instance()->looting_hook->link_all();  // Uses default active chat window.
  return rval;
}

static int __fastcall LootAllButtonDown(Zeal::GameUI::LootWnd *pWnd, int unused, Zeal::GameUI::CXPoint pt,
                                        unsigned int flag) {
  int rval = reinterpret_cast<int(__fastcall *)(Zeal::GameUI::LootWnd * pWnd, int unused, Zeal::GameUI::CXPoint pt,
                                                unsigned int flag)>(0x0595330)(pWnd, unused, pt, flag);
  ZealService *zeal = ZealService::get_instance();
  zeal->looting_hook->loot_all = true;
  zeal->looting_hook->looted_item();
  return rval;
}

static void __fastcall ButtonDestructor(Zeal::GameUI::BasicWnd *pWnd, int unusedEDX, byte delete_me) {
  // Deletes the custom vtable and swaps back to the CButtonWnd default vtable before calling the Deconstructor.
  // The CButtonWnd default deconstructor basically does the same table swap to the default a few lines in.
  pWnd->DeleteCustomVTable(reinterpret_cast<Zeal::GameUI::BaseVTable *>(0x005eaf00));  // CButtonWnd::vtable
  pWnd->Deconstruct(delete_me);                                                        // Calls the vtable's destructor.
}

void ui_loot::InitUI() {
  // The buttons are optional in the XML files so just skip when not found.
  Zeal::GameUI::BasicWnd *btn = Zeal::Game::Windows->Loot->GetChildItem("LinkAllButton");
  if (btn) {
    btn->SetupCustomVTable(sizeof(Zeal::GameUI::ButtonWndVTable));
    btn->vtbl->HandleLButtonDown = LinkAllButtonDown;
    btn->vtbl->Deconstructor = ButtonDestructor;
  }

  btn = Zeal::Game::Windows->Loot->GetChildItem("LootAllButton");
  if (btn) {
    btn->SetupCustomVTable(sizeof(Zeal::GameUI::ButtonWndVTable));
    btn->vtbl->HandleLButtonDown = LootAllButtonDown;
    btn->vtbl->Deconstructor = ButtonDestructor;
  }
}

ui_loot::ui_loot(ZealService *zeal, UIManager *mgr) {
  ui = mgr;
  zeal->callbacks->AddGeneric([this]() { InitUI(); }, callback_type::InitUI);
  /*zeal->callbacks->add_packet([this](UINT opcode, char* buffer, UINT size) {
          Zeal::Game::print_chat("Opcode: 0x%x Size: %i Buffer: %s", opcode, size,
  StringUtil::byteArrayToHexString(buffer, size).c_str()); return false;
  }, callback_type::SendMessage_);*/
}

ui_loot::~ui_loot() {}

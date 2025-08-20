#include "ui_bank.h"

#include "callbacks.h"
#include "game_addresses.h"
#include "game_functions.h"
#include "game_ui.h"
#include "ui_manager.h"
#include "zeal.h"

void ui_bank::change() {
  if (Zeal::Game::Windows->Bank && Zeal::Game::Windows->Bank->IsVisible) {
    if (Zeal::Game::get_char_info()->BankGold > 0) {
      reinterpret_cast<void(__thiscall *)(Zeal::GameUI::BasicWnd *, int, int)>(0x404aec)(
          Zeal::Game::Windows->Bank, 1, Zeal::Game::get_char_info()->BankGold);
      reinterpret_cast<void(__thiscall *)(Zeal::GameUI::BasicWnd *, int, int)>(0x404aec)(Zeal::Game::Windows->Bank, 2,
                                                                                         -1);
    }
    if (Zeal::Game::get_char_info()->BankSilver > 0) {
      reinterpret_cast<void(__thiscall *)(Zeal::GameUI::BasicWnd *, int, int)>(0x404aec)(
          Zeal::Game::Windows->Bank, 2, Zeal::Game::get_char_info()->BankSilver);
      reinterpret_cast<void(__thiscall *)(Zeal::GameUI::BasicWnd *, int, int)>(0x404aec)(Zeal::Game::Windows->Bank, 3,
                                                                                         -1);
    }
    if (Zeal::Game::get_char_info()->BankCopper > 0) {
      reinterpret_cast<void(__thiscall *)(Zeal::GameUI::BasicWnd *, int, int)>(0x404aec)(
          Zeal::Game::Windows->Bank, 3, Zeal::Game::get_char_info()->BankCopper);
      reinterpret_cast<void(__thiscall *)(Zeal::GameUI::BasicWnd *, int, int)>(0x404aec)(Zeal::Game::Windows->Bank, 0,
                                                                                         -1);
      reinterpret_cast<void(__thiscall *)(Zeal::GameUI::BasicWnd *, int, int)>(0x404aec)(Zeal::Game::Windows->Bank, 1,
                                                                                         -1);
      reinterpret_cast<void(__thiscall *)(Zeal::GameUI::BasicWnd *, int, int)>(0x404aec)(Zeal::Game::Windows->Bank, 2,
                                                                                         -1);
      reinterpret_cast<void(__thiscall *)(Zeal::GameUI::BasicWnd *, int, int)>(0x404aec)(Zeal::Game::Windows->Bank, 3,
                                                                                         -1);
    }

    if (Zeal::Game::get_char_info()->Gold > 0) {
      reinterpret_cast<void(__thiscall *)(Zeal::GameUI::BasicWnd *, int, int)>(0x421876)(
          Zeal::Game::Windows->Inventory, 1, Zeal::Game::get_char_info()->Gold);
      reinterpret_cast<void(__thiscall *)(Zeal::GameUI::BasicWnd *, int, int)>(0x421876)(Zeal::Game::Windows->Inventory,
                                                                                         2, -1);
    }
    if (Zeal::Game::get_char_info()->Silver > 0) {
      reinterpret_cast<void(__thiscall *)(Zeal::GameUI::BasicWnd *, int, int)>(0x421876)(
          Zeal::Game::Windows->Inventory, 2, Zeal::Game::get_char_info()->Silver);
      reinterpret_cast<void(__thiscall *)(Zeal::GameUI::BasicWnd *, int, int)>(0x421876)(Zeal::Game::Windows->Inventory,
                                                                                         3, -1);
    }
    if (Zeal::Game::get_char_info()->Copper > 0) {
      reinterpret_cast<void(__thiscall *)(Zeal::GameUI::BasicWnd *, int, int)>(0x421876)(
          Zeal::Game::Windows->Inventory, 3, Zeal::Game::get_char_info()->Copper);
      reinterpret_cast<void(__thiscall *)(Zeal::GameUI::BasicWnd *, int, int)>(0x421876)(Zeal::Game::Windows->Inventory,
                                                                                         0, -1);
      reinterpret_cast<void(__thiscall *)(Zeal::GameUI::BasicWnd *, int, int)>(0x421876)(Zeal::Game::Windows->Inventory,
                                                                                         1, -1);
      reinterpret_cast<void(__thiscall *)(Zeal::GameUI::BasicWnd *, int, int)>(0x421876)(Zeal::Game::Windows->Inventory,
                                                                                         2, -1);
      reinterpret_cast<void(__thiscall *)(Zeal::GameUI::BasicWnd *, int, int)>(0x421876)(Zeal::Game::Windows->Inventory,
                                                                                         3, -1);
    }
  }
}

static int __fastcall ChangeButtonDown(Zeal::GameUI::BasicWnd *pWnd, int unused, Zeal::GameUI::CXPoint pt,
                                       unsigned int flag) {
  int rval = reinterpret_cast<int(__fastcall *)(Zeal::GameUI::BasicWnd * pWnd, int unused, Zeal::GameUI::CXPoint pt,
                                                unsigned int flag)>(0x0595330)(pWnd, unused, pt, flag);

  ZealService::get_instance()->ui->bank->change();

  return rval;
}

static void __fastcall ButtonDestructor(Zeal::GameUI::BasicWnd *pWnd, int unusedEDX, BYTE delete_me) {
  // Deletes the custom vtable and swaps back to the CButtonWnd default vtable before calling the Deconstructor.
  // The CButtonWnd default deconstructor basically does the same table swap to the default a few lines in.
  pWnd->DeleteCustomVTable(reinterpret_cast<Zeal::GameUI::BaseVTable *>(0x005eaf00));  // CButtonWnd::vtable
  pWnd->Deconstruct(delete_me);                                                        // Calls the vtable's destructor.
}

// The InitUI is called immediately after all new UI objects are allocated and initialized. The
// ButtonDestructor should get called by the normal framework cleanup in CleanUI.
void ui_bank::InitUI() {
  // The ChangeButton is optional in the XML files so support the case where it is not found.
  Zeal::GameUI::BasicWnd *btn = Zeal::Game::Windows->Bank->GetChildItem("ChangeButton");
  if (btn) {
    btn->SetupCustomVTable(sizeof(Zeal::GameUI::ButtonWndVTable));
    btn->vtbl->HandleLButtonDown = ChangeButtonDown;
    btn->vtbl->Deconstructor = ButtonDestructor;
  }
}

ui_bank::ui_bank(ZealService *zeal, UIManager *mgr) {
  ui = mgr;
  zeal->callbacks->AddGeneric([this]() { InitUI(); }, callback_type::InitUI);
}

ui_bank::~ui_bank() {}

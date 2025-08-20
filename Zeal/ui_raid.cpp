#include "ui_raid.h"

#include "ui_manager.h"
#include "zeal.h"

// void ui_raid::CleanUI()
//{
//	Zeal::Game::print_debug("Clean UI RAID");
// }

// void ui_raid::InitUI()
//{
//
// }

ui_raid::ui_raid(ZealService *zeal, UIManager *mgr) {
  ui = mgr;
  // zeal->callbacks->AddGeneric([this]() { CleanUI(); }, callback_type::CleanUI);
  // zeal->callbacks->AddGeneric([this]() { InitUI(); }, callback_type::InitUI);
  // if (Zeal::Game::is_in_game()) InitUI();
}

ui_raid::~ui_raid() {}

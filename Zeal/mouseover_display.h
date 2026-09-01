#pragma once
#include "game_ui.h"
#include "game_structures.h"

// Forward declarations.
void __fastcall SetItem(Zeal::GameUI::ItemDisplayWnd *wnd, int unused, Zeal::GameStructures::_GAMEITEMINFO *item,
                        bool show);
void __fastcall SetSpell(Zeal::GameUI::ItemDisplayWnd *wnd, int unused, int spell_id, bool show, int unknown);
void UpdateSetSpellText(Zeal::GameUI::ItemDisplayWnd *wnd, int spell_id, bool buff);

// Called from ItemDisplay::InitUI, CleanUI, DeactivateUI, and constructor.
void mouseover_init_ui();
void mouseover_clean_ui();
void mouseover_deactivate_ui();
void mouseover_init_hooks(class ZealService *zeal);
void UpdateSetItemText(Zeal::GameUI::ItemDisplayWnd *wnd, Zeal::GameStructures::_GAMEITEMINFO *item);
bool mouseover_in_set_item();
Zeal::GameUI::ItemDisplayWnd *mouseover_get_wnd();
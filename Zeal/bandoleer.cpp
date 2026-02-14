#include "bandoleer.h"

#include <filesystem>

#include "callbacks.h"
#include "commands.h"
#include "game_addresses.h"
#include "game_functions.h"
#include "string_util.h"
#include "zeal.h"

void Bandoleer::initialize_ini_filename() {
  const char *name = Zeal::Game::get_char_info() ? Zeal::Game::get_char_info()->Name : "unknown";
  std::string filename = std::string(name) + "_bandoleer.ini";
  std::filesystem::path file_path = Zeal::Game::get_game_path() / std::filesystem::path(filename);
  ini.set(file_path.string());
}

const char *Bandoleer::get_slot_label(int slot_index) {
  switch (slot_index) {
    case kPrimarySlot:
      return "Primary";
    case kSecondarySlot:
      return "Secondary";
    case kRangeSlot:
      return "Range";
    default:
      return "Unknown";
  }
}

// Saves the current Primary, Secondary, and Range items for a gem (1-based gem_number).
void Bandoleer::save(int gem_number) {
  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();
  if (!char_info) return;

  initialize_ini_filename();
  std::string section = "Gem" + std::to_string(gem_number);

  for (int slot : kManagedSlots) {
    Zeal::GameStructures::GAMEITEMINFO *item = char_info->InventoryItem[slot];
    std::string key_name = std::string(get_slot_label(slot)) + "_Name";
    std::string key_id = std::string(get_slot_label(slot)) + "_ID";
    if (item) {
      ini.setValue(section, key_name, std::string(item->Name));
      ini.setValue(section, key_id, static_cast<int>(item->ID));
    } else {
      ini.setValue(section, key_name, std::string(""));
      ini.setValue(section, key_id, 0);
    }
  }

  Zeal::Game::print_chat("Bandoleer: Saved instruments for gem %i.", gem_number);
  for (int slot : kManagedSlots) {
    Zeal::GameStructures::GAMEITEMINFO *item = char_info->InventoryItem[slot];
    if (item)
      Zeal::Game::print_chat("  %s: %s", get_slot_label(slot), item->Name);
    else
      Zeal::Game::print_chat("  %s: (empty)", get_slot_label(slot));
  }
}

// Clears the saved items for a gem (1-based gem_number).
void Bandoleer::clear(int gem_number) {
  initialize_ini_filename();
  std::string section = "Gem" + std::to_string(gem_number);
  if (!ini.deleteSection(section))
    Zeal::Game::print_chat("Bandoleer: Error clearing gem %i.", gem_number);
  else
    Zeal::Game::print_chat("Bandoleer: Cleared gem %i.", gem_number);
}

// Lists all gem item assignments.
void Bandoleer::list() {
  initialize_ini_filename();
  Zeal::Game::print_chat("--- bandoleer assignments ---");
  bool found_any = false;
  for (int gem = 1; gem <= GAME_NUM_SPELL_GEMS; gem++) {
    std::string section = "Gem" + std::to_string(gem);
    if (!has_config_for_gem(gem - 1)) continue;

    found_any = true;
    Zeal::Game::print_chat("  Gem %i:", gem);
    for (int slot : kManagedSlots) {
      int item_id = ini.getValue<int>(section, std::string(get_slot_label(slot)) + "_ID");
      std::string item_name = ini.getValue<std::string>(section, std::string(get_slot_label(slot)) + "_Name");
      if (item_id > 0)
        Zeal::Game::print_chat("    %s: %s", get_slot_label(slot), item_name.c_str());
    }
  }
  if (!found_any) Zeal::Game::print_chat("  (none)");
  Zeal::Game::print_chat("--- end of bandoleer ---");
}

// Returns true if the given gem (0-based) has any non-empty bandoleer items configured.
bool Bandoleer::has_config_for_gem(int gem_index) {
  initialize_ini_filename();
  std::string section = "Gem" + std::to_string(gem_index + 1);
  for (int slot : kManagedSlots) {
    std::string key_id = std::string(get_slot_label(slot)) + "_ID";
    if (ini.exists(section, key_id) && ini.getValue<int>(section, key_id) > 0) return true;
  }
  return false;
}

// Searches inventory bag slots for an item matching the given id and name.
// Returns the bag slot id (250+) or -1 if not found.
int Bandoleer::find_item_in_bags(int item_id, const std::string &item_name) {
  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();
  if (!char_info || item_id <= 0) return -1;

  for (int bag_i = 0; bag_i < GAME_NUM_INVENTORY_PACK_SLOTS; bag_i++) {
    Zeal::GameStructures::GAMEITEMINFO *container = char_info->InventoryPackItem[bag_i];
    if (!container || container->Type != 1) continue;
    for (int slot_i = 0; slot_i < container->Container.Capacity; slot_i++) {
      Zeal::GameStructures::GAMEITEMINFO *item = container->Container.Item[slot_i];
      if (item && item->ID == item_id && item_name == item->Name) {
        return 250 + (bag_i * 10) + slot_i;
      }
    }
  }
  return -1;
}

// Swaps an item from a bag slot into an equipment slot using the InvSlot click mechanism.
// equip_slot_index is 0-based (e.g. 12=Primary). The slot ID for FindInvSlot is index+1.
bool Bandoleer::swap_item(int bag_slot_id, int equip_slot_index) {
  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();
  if (!char_info) return false;

  if (char_info->CursorItem || char_info->CursorCopper || char_info->CursorGold || char_info->CursorPlatinum ||
      char_info->CursorSilver)
    return false;

  Zeal::GameUI::CInvSlotMgr *inv_slot_mgr = Zeal::Game::Windows->InvSlotMgr;
  if (!inv_slot_mgr) return false;

  Zeal::GameUI::InvSlot *src_slot = inv_slot_mgr->FindInvSlot(bag_slot_id);
  if (!src_slot || !src_slot->invSlotWnd) return false;

  int equip_slot_id = equip_slot_index + 1;
  Zeal::GameUI::InvSlot *dst_slot = inv_slot_mgr->FindInvSlot(equip_slot_id);
  if (!dst_slot || !dst_slot->invSlotWnd || dst_slot->invSlotWnd->SlotID != equip_slot_id) return false;

  Zeal::GameUI::CXWndManager *wnd_mgr = Zeal::Game::get_wnd_manager();
  if (!wnd_mgr) return false;

  Zeal::GameStructures::GAMEITEMINFO *src_item =
      reinterpret_cast<Zeal::GameStructures::GAMEITEMINFO *>(src_slot->Item);
  if (!src_item) return false;

  WORD src_item_id = src_item->ID;

  // Save and override keyboard modifiers (SHIFT forces full stack pickup).
  BYTE shift = wnd_mgr->ShiftKeyState;
  BYTE ctrl = wnd_mgr->ControlKeyState;
  BYTE alt = wnd_mgr->AltKeyState;
  wnd_mgr->ShiftKeyState = 1;
  wnd_mgr->ControlKeyState = 0;
  wnd_mgr->AltKeyState = 0;

  bool success = false;

  // (1) Pick up item from bag to cursor.
  src_slot->HandleLButtonUp();
  if (char_info->CursorItem && char_info->CursorItem->ID == src_item_id) {
    Zeal::GameStructures::GAMEITEMINFO *dst_item = char_info->InventoryItem[equip_slot_index];
    WORD dst_item_id = dst_item ? dst_item->ID : 0;

    // (2) Place into equipment slot (swaps existing item to cursor).
    dst_slot->HandleLButtonUp();

    if (dst_item_id && char_info->CursorItem && char_info->CursorItem->ID == dst_item_id) {
      // (3) Put the swapped-out item back in the bag slot we emptied.
      src_slot->HandleLButtonUp();
    }
    success = true;
  }

  wnd_mgr->ShiftKeyState = shift;
  wnd_mgr->ControlKeyState = ctrl;
  wnd_mgr->AltKeyState = alt;
  return success;
}

// Swaps the configured instruments into the equipment slots for the active gem.
void Bandoleer::swap_instruments_in() {
  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();
  if (!char_info || active_gem < 0) return;

  initialize_ini_filename();
  std::string section = "Gem" + std::to_string(active_gem + 1);

  active_swaps.clear();

  for (int slot : kManagedSlots) {
    std::string key_id = std::string(get_slot_label(slot)) + "_ID";
    std::string key_name = std::string(get_slot_label(slot)) + "_Name";
    int target_id = ini.getValue<int>(section, key_id);
    std::string target_name = ini.getValue<std::string>(section, key_name);

    if (target_id <= 0) continue;

    // Check if the correct item is already equipped.
    Zeal::GameStructures::GAMEITEMINFO *equipped = char_info->InventoryItem[slot];
    if (equipped && equipped->ID == target_id && target_name == equipped->Name) continue;

    // Record what is currently equipped so we can restore it later.
    SwapRecord record;
    record.equip_slot = slot;
    if (equipped) {
      record.orig_name = equipped->Name;
      record.orig_id = equipped->ID;
    }

    // Find the instrument in inventory bags and swap it in.
    int bag_slot_id = find_item_in_bags(target_id, target_name);
    if (bag_slot_id < 0) continue;

    if (swap_item(bag_slot_id, slot))
      active_swaps.push_back(record);
  }
}

// Restores the original weapons that were displaced by a bandoleer instrument swap.
void Bandoleer::swap_weapons_back() {
  Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();
  if (!char_info || active_swaps.empty()) return;

  for (auto &record : active_swaps) {
    if (record.orig_id <= 0) continue;  // Slot was empty before, no weapon to restore.

    // Check if the original item is already back (e.g., manual swap by player).
    Zeal::GameStructures::GAMEITEMINFO *equipped = char_info->InventoryItem[record.equip_slot];
    if (equipped && equipped->ID == record.orig_id && record.orig_name == equipped->Name) continue;

    // Find the original weapon in bags and swap it back.
    int bag_slot_id = find_item_in_bags(record.orig_id, record.orig_name);
    if (bag_slot_id < 0) continue;

    swap_item(bag_slot_id, record.equip_slot);
  }

  active_swaps.clear();
}

// Called by Melody right BEFORE casting a new song to restore weapons from any previous swap.
void Bandoleer::restore_if_swapped() {
  if (state == State::Swapped) {
    swap_weapons_back();
    state = State::Idle;
    active_gem = -1;
  }
}

// Called by Melody right AFTER a new song starts casting.
void Bandoleer::notify_song_start(int gem_index) {
  if (has_config_for_gem(gem_index)) {
    active_gem = gem_index;
    state = State::Monitoring;
  } else {
    active_gem = -1;
    state = State::Idle;
  }
}

// Called by Melody when it ends.
void Bandoleer::notify_melody_stop() {
  if (state == State::Swapped) {
    swap_weapons_back();
  }
  state = State::Idle;
  active_gem = -1;
}

// Bandoleer tick: monitors the casting timer and swaps instruments in during the last second.
void Bandoleer::tick() {
  if (state != State::Monitoring || active_gem < 0) return;

  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
  auto *display = Zeal::Game::get_display();
  if (!self || !self->ActorInfo || !display) return;

  // Only act while a spell is actively being cast.
  if (self->ActorInfo->CastingSpellId == kInvalidSpellId) return;

  DWORD game_time = display->GameTimeMs;
  DWORD cast_finish = self->ActorInfo->CastingTimeout;

  // Sanity check: cast_finish should be ahead of game_time and within a reasonable range.
  if (cast_finish <= game_time || (cast_finish - game_time) > 30000) return;

  // Check if we are within the last kSwapThresholdMs of the cast.
  if ((cast_finish - game_time) < kSwapThresholdMs) {
    swap_instruments_in();
    if (!active_swaps.empty())
      state = State::Swapped;
    else
      state = State::Idle;  // No swaps were needed or possible.
  }
}

Bandoleer::Bandoleer(ZealService *zeal) {
  if (!Zeal::Game::is_new_ui()) return;

  // Register a tick callback for monitoring the casting timer.
  zeal->callbacks->AddGeneric([this]() { tick(); });
  zeal->callbacks->AddGeneric([this]() { notify_melody_stop(); }, callback_type::CharacterSelect);

  zeal->commands_hook->Add(
      "/bandoleer", {"/ban"},
      "Melody instrument swaps. Usage: /bandoleer set/clear <gem#>, /bandoleer list",
      [this](std::vector<std::string> &args) {
        if (!Zeal::Game::is_in_game() || !Zeal::Game::get_char_info()) {
          Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "Must be in game to use bandoleer.");
          return true;
        }

        if (args.size() == 2 && Zeal::String::compare_insensitive(args[1], "list")) {
          list();
          return true;
        }

        if (args.size() == 3) {
          int gem_number = 0;
          if (!Zeal::String::tryParse(args[2], &gem_number) || gem_number < 1 || gem_number > GAME_NUM_SPELL_GEMS) {
            Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "Invalid gem number. Use 1-%i.", GAME_NUM_SPELL_GEMS);
            return true;
          }
          if (Zeal::String::compare_insensitive(args[1], "set")) {
            save(gem_number);
            return true;
          }
          if (Zeal::String::compare_insensitive(args[1], "clear")) {
            clear(gem_number);
            return true;
          }
        }

        Zeal::Game::print_chat("Usage: /bandoleer set <gem#> - saves current weapons for that gem");
        Zeal::Game::print_chat("       /bandoleer clear <gem#> - clears saved weapons for that gem");
        Zeal::Game::print_chat("       /bandoleer list - shows all gem assignments");
        Zeal::Game::print_chat("During melody, instruments swap in ~1s before each song lands,");
        Zeal::Game::print_chat("then your weapons are restored before the next song.");
        return true;
      });
}

Bandoleer::~Bandoleer() {}

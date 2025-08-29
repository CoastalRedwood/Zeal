#include "utils.h"

#include <utility>
#include <vector>

#include "commands.h"
#include "game_addresses.h"
#include "game_structures.h"
#include "game_ui.h"
#include "string_util.h"
#include "zeal.h"

Utils::Utils(ZealService *zeal) {
  zeal->commands_hook->Add(
      "/locktogglebagslot", {}, "Sets the locked open bag slot (1 - 8, 0 = off).",
      [this](const std::vector<std::string> &args) {
        int slot = 0;
        if (args.size() == 2 && Zeal::String::tryParse(args[1], &slot, true) && slot >= 0 && slot <= 8)
          setting_lock_toggle_bag_slot.set(slot);
        else
          Zeal::Game::print_chat("Usage: /locktogglebagslot # (slot 1 to 8, 0 = off)");
        Zeal::Game::print_chat("locktogglebagslot set to %d", setting_lock_toggle_bag_slot.get());
        if (update_options_ui_callback) update_options_ui_callback();
        return true;
      });
}

void Utils::handle_toggle_all_containers() const {
  auto *self = Zeal::Game::get_self();
  auto char_info = self ? self->CharInfo : nullptr;
  auto manager = *Zeal::Game::ptr_ContainerMgr;
  if (!char_info || !manager) return;

  bool is_a_bag_open = false;
  bool is_a_bag_closed = false;
  for (int i = 0; i < 8; i++)  // 8 inventory slots for containers
  {
    Zeal::GameStructures::GAMEITEMINFO *item = char_info->InventoryPackItem[i];
    if (item && item->Type == 1 && item->Container.Capacity > 0) {
      is_a_bag_open = is_a_bag_open || item->Container.IsOpen;
      is_a_bag_closed = is_a_bag_closed || !item->Container.IsOpen;
    }
  }
  if (is_a_bag_open && !is_a_bag_closed)  // All the containers are open, so toggle close.
  {
    int lock_slot = setting_lock_toggle_bag_slot.get();
    if (lock_slot <= 0 || lock_slot > 8) {
      manager->CloseAllContainers();
    } else {
      Zeal::GameStructures::GAMEITEMINFO *locked_slot = char_info->InventoryPackItem[lock_slot - 1];
      for (int i = 0; i < 0x11; ++i) {
        auto wnd = manager->pPCContainers[i];
        auto container = wnd ? wnd->pContainerInfo : nullptr;
        if (!container || container == locked_slot) continue;  // Locked so skip.
        if (container->Type == 1 && container->Container.Capacity > 0 && container->Container.IsOpen)
          manager->CloseContainer(container, true);
      }
    }
  } else {
    for (int i = 0; i < 8; ++i) {
      Zeal::GameStructures::GAMEITEMINFO *item = char_info->InventoryPackItem[i];
      if (item && item->Type == 1 && item->Container.Capacity > 0 && !item->Container.IsOpen)
        manager->OpenContainer(item, 22 + i);
    }
  }
}

Utils::~Utils() {}

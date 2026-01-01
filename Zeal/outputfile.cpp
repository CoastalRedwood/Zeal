#include "outputfile.h"

#include <fstream>

#include "commands.h"
#include "game_functions.h"
#include "game_structures.h"
#include "hook_wrapper.h"
#include "string_util.h"
#include "zeal.h"

using Zeal::GameEnums::EquipSlot::EquipSlot;

static std::string IDToEquipSlot(int equipSlot, bool new_format) {
  switch (equipSlot) {
    case EquipSlot::LeftEar:
      return new_format ? "Ear1" : "Ear";
    case EquipSlot::RightEar:
      return new_format ? "Ear2" : "Ear";
    case EquipSlot::Head:
      return "Head";
    case EquipSlot::Face:
      return "Face";
    case EquipSlot::Neck:
      return "Neck";
    case EquipSlot::Shoulder:
      return "Shoulders";
    case EquipSlot::Arms:
      return "Arms";
    case EquipSlot::Back:
      return "Back";
    case EquipSlot::LeftWrist:
      return new_format ? "Wrist1" : "Wrist";
    case EquipSlot::RightWrist:
      return new_format ? "Wrist2" : "Wrist";
    case EquipSlot::Range:
      return "Range";
    case EquipSlot::Hands:
      return "Hands";
    case EquipSlot::Primary:
      return "Primary";
    case EquipSlot::Secondary:
      return "Secondary";
    case EquipSlot::LeftFinger:
      return new_format ? "Finger1" : "Fingers";
    case EquipSlot::RightFinger:
      return new_format ? "Finger2" : "Fingers";
    case EquipSlot::Chest:
      return "Chest";
    case EquipSlot::Legs:
      return "Legs";
    case EquipSlot::Feet:
      return "Feet";
    case EquipSlot::Waist:
      return "Waist";
    case EquipSlot::Ammo:
      return "Ammo";
    default: {
    } break;
  }
  return "Unknown";
}

static bool ItemIsContainer(Zeal::GameStructures::GAMEITEMINFO *item) {
  return (item->Type == 1 && item->Container.Capacity > 0);
}

static bool ItemIsStackable(Zeal::GameStructures::GAMEITEMINFO *item) {
  return ((item->Common.IsStackable) && (item->Common.SpellId == 0));
}

void OutputFile::export_inventory(const std::vector<std::string> &args) {
  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();

  bool new_format = (setting_export_format.get() != 0);
  std::ostringstream oss;
  std::string t = "\t";  // output spacer
  const char *count_col_title = new_format ? "Count/Charges" : "Count";
  oss << "Location" << t << "Name" << t << "ID" << t << count_col_title << t << "Slots" << std::endl;

  // Processing Equipment
  for (size_t i = 0; i < GAME_NUM_INVENTORY_SLOTS; ++i) {
    Zeal::GameStructures::GAMEITEMINFO *item = self->CharInfo->InventoryItem[i];
    // GAMEITEMINFO->EquipSlot value only updates when a load happens. Don't use it for this.
    if (item) {
      int count = item->Common.StackCount;  // Union with charges.
      oss << IDToEquipSlot(i, new_format) << t << item->Name << t << item->ID << t << count << t << 0 << std::endl;
    } else {
      oss << IDToEquipSlot(i, new_format) << t << "Empty" << t << 0 << t << 0 << t << 0 << std::endl;
    }
  }

  {  // Processing Inventory Slots
    for (size_t i = 0; i < GAME_NUM_INVENTORY_PACK_SLOTS; ++i) {
      Zeal::GameStructures::GAMEITEMINFO *item = self->CharInfo->InventoryPackItem[i];
      if (item) {
        if (ItemIsContainer(item)) {
          int capacity = static_cast<int>(item->Container.Capacity);
          oss << "General" << i + 1 << t << item->Name << t << item->ID << t << 1 << t << capacity << std::endl;
          for (int j = 0; j < capacity; ++j) {
            Zeal::GameStructures::GAMEITEMINFO *bag_item = item->Container.Item[j];
            if (bag_item) {
              int count = bag_item->Common.StackCount;  // Union with charges.
              oss << "General" << i + 1 << "-Slot" << j + 1 << t << bag_item->Name << t << bag_item->ID << t << count
                  << t << 0 << std::endl;
            } else {
              oss << "General" << i + 1 << "-Slot" << j + 1 << t << "Empty" << t << 0 << t << 0 << t << 0 << std::endl;
            }
          }
        } else {
          int count = item->Common.StackCount;  // Union with charges.
          oss << "General" << i + 1 << t << item->Name << t << item->ID << t << count << t << 0 << std::endl;
        }
      } else {
        oss << "General" << i + 1 << t << "Empty" << t << 0 << t << 0 << t << 0 << std::endl;
      }
    }
    ULONGLONG coin = 0;
    coin += static_cast<ULONGLONG>(self->CharInfo->Platinum) * 1000;
    coin += static_cast<ULONGLONG>(self->CharInfo->Gold) * 100;
    coin += static_cast<ULONGLONG>(self->CharInfo->Silver) * 10;
    coin += self->CharInfo->Copper;
    oss << "General-Coin" << t << "Currency" << t << 0 << t << coin << t << 0 << std::endl;
  }

  {  // Process Cursor Item
    Zeal::GameStructures::GAMEITEMINFO *item = self->CharInfo->CursorItem;
    if (item) {
      if (ItemIsContainer(item)) {
        int capacity = static_cast<int>(item->Container.Capacity);
        oss << "Held" << t << item->Name << t << item->ID << t << 1 << t << capacity << std::endl;
        for (int i = 0; i < capacity; ++i) {
          Zeal::GameStructures::GAMEITEMINFO *bag_item = item->Container.Item[i];
          if (bag_item) {
            int count = bag_item->Common.StackCount;  // Union with charges.
            oss << "Held"
                << "-Slot" << i + 1 << t << bag_item->Name << t << bag_item->ID << t << count << t << 0 << std::endl;
          } else {
            oss << "Held"
                << "-Slot" << i + 1 << t << "Empty" << t << 0 << t << 0 << t << 0 << std::endl;
          }
        }
      } else {
        int count = item->Common.StackCount;  // Union with charges.
        oss << "Held" << t << item->Name << t << item->ID << t << count << t << 0 << std::endl;
      }
    } else {
      ULONGLONG coin = 0;
      coin += static_cast<ULONGLONG>(self->CharInfo->CursorPlatinum) * 1000;
      coin += static_cast<ULONGLONG>(self->CharInfo->CursorGold) * 100;
      coin += static_cast<ULONGLONG>(self->CharInfo->CursorSilver) * 10;
      coin += self->CharInfo->CursorCopper;

      if (coin != 0)
        oss << "Held" << t << "Currency" << t << 0 << t << coin << t << 0 << std::endl;
      else
        oss << "Held" << t << "Empty" << t << 0 << t << 0 << t << 0 << std::endl;
    }
  }

  {  // Process Bank Items
    int num_bank_slots = Zeal::Game::get_num_personal_bank_slots();
    for (int i = 0; i < Zeal::Game::get_num_total_bank_slots(); ++i) {
      Zeal::GameStructures::GAMEITEMINFO *item = self->CharInfo->InventoryBankItem[i];
      const char *label = (i < num_bank_slots) ? "Bank" : "SharedBank";
      int slot = (i < num_bank_slots) ? (i + 1) : (i + 1 - num_bank_slots);
      if (item) {
        if (ItemIsContainer(item)) {
          int capacity = static_cast<int>(item->Container.Capacity);
          oss << label << slot << t << item->Name << t << item->ID << t << 1 << t << capacity << std::endl;
          for (int j = 0; j < capacity; ++j) {
            Zeal::GameStructures::GAMEITEMINFO *bag_item = item->Container.Item[j];
            if (bag_item) {
              int count = bag_item->Common.StackCount;  // Union with charges.
              oss << label << slot << "-Slot" << j + 1 << t << bag_item->Name << t << bag_item->ID << t << count << t
                  << 0 << std::endl;
            } else {
              oss << label << slot << "-Slot" << j + 1 << t << "Empty" << t << 0 << t << 0 << t << 0 << std::endl;
            }
          }
        } else {
          int count = item->Common.StackCount;  // Union with charges.
          oss << label << slot << t << item->Name << t << item->ID << t << count << t << 0 << std::endl;
        }
      } else {
        oss << label << slot << t << "Empty" << t << 0 << t << 0 << t << 0 << std::endl;
      }
    }
    ULONGLONG coin = 0;
    coin += static_cast<ULONGLONG>(self->CharInfo->BankPlatinum) * 1000;
    coin += static_cast<ULONGLONG>(self->CharInfo->BankGold) * 100;
    coin += static_cast<ULONGLONG>(self->CharInfo->BankSilver) * 10;
    coin += self->CharInfo->BankCopper;
    oss << "Bank-Coin" << t << "Currency" << t << 0 << t << coin << t << 0 << std::endl;
  }

  std::string optional_name = "";  // Blank optional_name results in "<char_name>-Inventory.txt".
  if (args.size() > 2) {
    optional_name = args[2];
  }
  write_to_file(oss.str(), "Inventory", optional_name, new_format);
}

void OutputFile::export_spellbook(const std::vector<std::string> &args) {
  Zeal::GameStructures::Entity *self = Zeal::Game::get_self();

  std::stringstream oss;
  oss << "Index\tSpellId\tLevel\tName" << std::endl;
  for (size_t i = 0; i < GAME_NUM_SPELL_BOOK_SPELLS; ++i) {
    WORD SpellId = self->CharInfo->SpellBook[i];
    int Level = Zeal::Game::get_spell_level(SpellId);
    const char *Name = Zeal::Game::get_spell_name(SpellId);
    Name = Name ? Name : "Unknown";
    if (SpellId && SpellId != USHRT_MAX) {
      oss << i << "\t" << SpellId << "\t" << Level << "\t" << Name << std::endl;
    }
  }

  std::string optional_name = "";
  if (args.size() > 2) {
    optional_name = args[2];
  }
  write_to_file(oss.str(), "Spellbook", optional_name, setting_export_format.get() != 0);
}

void OutputFile::export_raidlist(std::vector<std::string> &args) {
  std::vector<Zeal::GameStructures::RaidMember *> raid_member_list = Zeal::Game::get_raid_list();
  if (raid_member_list.size() > 0) {
    std::stringstream oss;

    std::string points = "1";
    if (args.size() > 2) {
      points = args[2];
    }
    std::string timestamp = Zeal::Game::generateTimestamp();

    oss << "Player\tLevel\tClass\tTimestamp\tPoints" << std::endl;

    for (auto &raid_member : raid_member_list) {
      oss << raid_member->Name << "\t" << raid_member->PlayerLevel << "\t" << raid_member->Class << "\t" << timestamp
          << "\t" << points << std::endl;
    }
    std::string fname = "RaidTick-" + timestamp;
    write_to_file(oss.str(), "", fname);
    Zeal::Game::print_chat("Raid tick saved to: %s", fname.c_str());
  } else {
    Zeal::Game::print_chat("Currently not in a raid.");
  }
}

void OutputFile::write_to_file(std::string data, std::string file_arg, std::string optional_name, bool add_host_tag) {
  std::string filename = optional_name;
  if (filename.empty()) {
    filename = Zeal::Game::get_self()->CharInfo->Name;
    filename += "-" + file_arg;
    if (add_host_tag) filename += Zeal::Game::get_host_tag();
  }
  filename += ".txt";

  std::ofstream file;
  file.open(filename);
  file << data;
  file.close();
}

// This replaces the previous /camp command with a direct hook of the game ::Camp() client
// method in order to support all camping pathways (buttons, hotkeyed button, and /camp).
static void __fastcall GameCamp(void *this_game, int unused_edx) {
  // Support auto-sitting (but peek ahead to see if the camp command is likely allowed).
  if (Zeal::Game::is_in_game() && !Zeal::Game::GameInternal::IsNoSlashWndActive()) {
    if (Zeal::Game::is_mounted()) Zeal::Game::dismount();
    Zeal::Game::sit();
  }
  if (ZealService::get_instance()->outputfile->setting_export_on_camp.get()) {
    ZealService::get_instance()->outputfile->export_inventory();
    ZealService::get_instance()->outputfile->export_spellbook();
  }
  ZealService::get_instance()->hooks->hook_map["GameCamp"]->original(GameCamp)(this_game, unused_edx);
}

OutputFile::OutputFile(ZealService *zeal) {
  zeal->commands_hook->Add(
      "/outputfile", {"/output", "/out"}, "Outputs your inventory,spellbook, or raidlist to file.",
      [this](std::vector<std::string> &args) {
        if (args.size() == 2 || args.size() == 3) {
          if (Zeal::String::compare_insensitive(args[1], "inventory")) {
            Zeal::Game::print_chat("Outputting inventory...");
            export_inventory(args);
            return true;
          } else if (Zeal::String::compare_insensitive(args[1], "spellbook")) {
            Zeal::Game::print_chat("Outputting spellbook...");
            export_spellbook(args);
            return true;
          } else if (Zeal::String::compare_insensitive(args[1], "raidlist")) {
            export_raidlist(args);
            return true;
          }
        }

        if (args.size() == 3 && Zeal::String::compare_insensitive(args[1], "format")) {
          int format = 0;
          if (Zeal::String::tryParse(args[2], &format) && (format == 0 || format == 1)) {
            setting_export_format.set(format);
            Zeal::Game::print_chat("Output format set to %d", setting_export_format.get());
            return true;
          }
        }
        Zeal::Game::print_chat("usage: /outputfile [inventory | spellbook | raidlist] [optional filename]");
        Zeal::Game::print_chat("usage: /outputfile format [0 | 1]");
        return true;
      });
  zeal->hooks->Add("GameCamp", 0x00530c7b, GameCamp, hook_type_detour);
}

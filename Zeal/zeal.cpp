#include "zeal.h"

#include <Windows.h>
#include <crtdbg.h>

#include <filesystem>
#include <iostream>
#include <memory>
#include <typeinfo>

#include "alarm.h"
#include "assist.h"
#include "autofire.h"
#include "binds.h"
#include "buff_timers.h"
#include "callbacks.h"
#include "camera_mods.h"
#include "character_select.h"
#include "chat.h"
#include "chatfilter.h"
#include "commands.h"
#include "crash_handler.h"
#include "cycle_target.h"
#include "directx.h"
#include "entity_manager.h"
#include "equip_item.h"
#include "experience.h"
#include "find_pattern.h"
#include "floating_damage.h"
#include "game_addresses.h"
#include "game_str.h"
#include "helm_manager.h"
#include "hook_wrapper.h"
#include "io_ini.h"
#include "item_display.h"
#include "labels.h"
#include "looting.h"
#include "melody.h"
#include "music.h"
#include "named_pipe.h"
#include "nameplate.h"
#include "netstat.h"
#include "npc_give.h"
#include "outputfile.h"
#include "patches.h"
#include "physics.h"
#include "player_movement.h"
#include "raid.h"
#include "spellsets.h"
#include "string_util.h"
#include "survey.h"
#include "target_ring.h"
#include "tellwindows.h"
#include "tick.h"
#include "tooltip.h"
#include "triggers.h"
#include "ui_manager.h"
#include "ui_skin.h"
#include "utils.h"
#include "zone_map.h"

extern HMODULE this_module;

#define MakeCheckedUnique(T, ...) MakeCheckedUniqueImpl<T>(__FILE__, __LINE__, this, __VA_ARGS__)

template <typename T, typename Parent, typename... Args>
using constructable = std::is_constructible<T, Parent *, Args...>;

static int heap_failed_line = 0;

template <typename T, typename Parent, typename... Args>
std::unique_ptr<T> MakeCheckedUniqueImpl(const char *file, int line, Parent *parent, Args &&...args) {
  std::unique_ptr<T> ptr;
  static bool HasBeenNotified = false;
  if constexpr (constructable<T, Parent,
                              Args...>::value) {  // if it will accept the this pointer from zeal then pass it
    // in, if not just use the arguments given
    ptr = std::make_unique<T>(parent, std::forward<Args>(args)...);
  } else {
    ptr = std::make_unique<T>(std::forward<Args>(args)...);
  }
  int result1 = HeapValidate(GetProcessHeap(), 0, NULL);
  int result2 = HeapValidate(*Zeal::Game::Heap, 0, NULL);
  if (result1 && result2) return ptr;
  if (HasBeenNotified) return ptr;
  heap_failed_line = line;
  std::stringstream ss;
  ss << "Heap corruption detected after allocating " << typeid(T).name() << " at " << file << ":" << line << "\n";
  ss << "This may be a false positive or it may be real and the game *might* crash later.\n";
  ss << "You can choose to either abort so you can restart the game, retry the check, or ignore this and continue..\n";
  int result_id = MessageBoxA(NULL, ss.str().c_str(), "Zeal boot heap check", MB_ABORTRETRYIGNORE | MB_ICONWARNING);
  if (result_id == IDABORT) throw std::bad_alloc();  // Will crash out the program.
  HasBeenNotified = true;
  return ptr;
}

int ZealService::get_heap_failed_line() { return heap_failed_line; }

ZealService *ZealService::ptr_service = nullptr;

ZealService::ZealService() {
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_LEAK_CHECK_DF);

  // Make sure the service pointer is populated immediately in case some submodule makes use of it.
  // Making the core framework classes would reduce much of the need for this.
  ZealService::ptr_service = this;

  // Install crash handler first in case of an initialization error.
  crash_handler = MakeCheckedUnique(CrashHandler);

  // Core framework classes (minimal internal dependencies).
  ini = MakeCheckedUnique(IO_ini, IO_ini::kZealIniFilename);
  hooks = MakeCheckedUnique(HookWrapper);
  callbacks = MakeCheckedUnique(CallbackManager);     // Uses hooks.
  commands_hook = MakeCheckedUnique(ChatCommands);    // Uses hooks.
  entity_manager = MakeCheckedUnique(EntityManager);  // Uses hooks.
  binds_hook = MakeCheckedUnique(Binds);              // Uses hooks and callbacks.

  // Configure font size (which impacts Zeal xml paths) early.
  UISkin::initialize_mode(this);  // Dependent on hooks and ini.
  UISkin::configuration_check();  // First order check that the required uifiles exist.

  // Create classes that use core framework and game client specific classes only.
  game_patches = MakeCheckedUnique(Patches);
  physics = MakeCheckedUnique(Physics);
  dx = MakeCheckedUnique(DirectX);
  gamestr_hook = MakeCheckedUnique(GameStr);
  cycle_target = MakeCheckedUnique(CycleTarget);
  camera_mods = MakeCheckedUnique(CameraMods);
  raid_hook = MakeCheckedUnique(Raid);
  tooltips = MakeCheckedUnique(Tooltip);
  assist = MakeCheckedUnique(Assist);
  outputfile = MakeCheckedUnique(OutputFile);
  movement = MakeCheckedUnique(PlayerMovement);
  music = MakeCheckedUnique(MusicManager);
  alarm = MakeCheckedUnique(Alarm);
  melody = MakeCheckedUnique(Melody);
  autofire = MakeCheckedUnique(AutoFire);
  netstat = MakeCheckedUnique(Netstat);
  tick = MakeCheckedUnique(Tick);
  buff_timers = MakeCheckedUnique(BuffTimers);
  helm = MakeCheckedUnique(HelmManager);

  // Adds DirectX (and UISkin for resource file paths) dependencies.
  target_ring = MakeCheckedUnique(TargetRing);
  floating_damage = MakeCheckedUnique(FloatingDamage);  // Uses target ring method.

  // Classes that add more explicit dependencies on the new UI.
  utils = MakeCheckedUnique(Utils);                 // Uses container manager.
  experience = MakeCheckedUnique(Experience);       // Uses new UI AA window calcs.
  labels_hook = MakeCheckedUnique(Labels);          // Uses tick and experience.
  item_displays = MakeCheckedUnique(ItemDisplay);   // Uses new UI ItemDisplayWnd.
  equip_item_hook = MakeCheckedUnique(EquipItem);   // Uses new UI InvSlotWnd.
  chatfilter_hook = MakeCheckedUnique(chatfilter);  // Uses new UI ChatWnd
  chat_hook = MakeCheckedUnique(Chat);              // Uses chatfilter.
  triggers = MakeCheckedUnique(Triggers);           // Uses chat_hook.
  nameplate = MakeCheckedUnique(NamePlate);         // Uses target ring blink rate, chat, chatfilter.
  tells = MakeCheckedUnique(TellWindows);           // Uses new UI ChatManager.
  looting_hook = MakeCheckedUnique(Looting);        // Uses new UI Loot window (and ChatManager).
  give = MakeCheckedUnique(NPCGive);                // Uses new Ui Trade & Give and also looting.

  // More complex new UI classes.
  zone_map = MakeCheckedUnique(ZoneMap);            // Uses ui and ui->options (post construction).
  ui = MakeCheckedUnique(UIManager);                // Has many dependencies (especially ui->options).
  charselect = MakeCheckedUnique(CharacterSelect);  // Uses ui->zoneselect.
  spell_sets = MakeCheckedUnique(SpellSets);        // Uses ui->inputDialog.
  survey = MakeCheckedUnique(Survey);               // Uses UI manager and input dialog.

  callbacks->AddGeneric([this]() {
    if (Zeal::Game::is_in_game() && print_buffer.size()) {
      for (auto &str : print_buffer) Zeal::Game::print_chat(USERCOLOR_SHOUT, "Zeal: %s", str.c_str());
      print_buffer.clear();
    }
  });

  AddCommands();  // Add more chat /commands with more dependencies.
  AddBinds();     // Register custom keybinds.

  // Connect up the pipe last since it spawns another thread (paranoia).
  pipe = MakeCheckedUnique(NamedPipe);  // Modify so it registers callbacks with labels, ticks, chat.
}

ZealService::~ZealService() { ZealService::ptr_service = nullptr; }

// Commands and keybinds.

// Consents all class types in the raid.
static bool handle_consent_class(Zeal::GameEnums::ClassTypes target_class) {
  const char *class_name = (target_class == Zeal::GameEnums::ClassTypes::Rogue) ? "rogues" : "monks";
  Zeal::GameStructures::RaidInfo *raid_info = Zeal::Game::RaidInfo;
  if (!raid_info->is_in_raid()) {
    Zeal::Game::print_chat("/consent%s only works when in a raid", class_name);
    return true;
  }

  bool consented = false;
  for (int i = 0; i < Zeal::GameStructures::RaidInfo::kRaidMaxMembers; ++i) {
    const auto &member = raid_info->MemberList[i];
    if (member.Name[0] && member.ClassValue == target_class) {
      consented = true;
      Zeal::Game::do_consent(member.Name);
    }
  }
  if (!consented) Zeal::Game::print_chat("No %s in raid to consent", class_name);
  return true;
}

// Sends a "Consent me" tell to the owner of the targeted corpse.
static bool handle_tell_consent() {
  auto target = Zeal::Game::get_target();
  if (!target) {  // If no target, target the nearest corpse within a short distance.
    target = ZealService::get_instance()->cycle_target->get_nearest_ent(50, 3);
    if (target) {
      Zeal::Game::set_target(target);
    } else {
      Zeal::Game::print_chat("/tellconsent could not find a player corpse to target.");
      return true;
    }
  }

  if (target->Type != Zeal::GameEnums::EntityTypes::PlayerCorpse) {
    Zeal::Game::print_chat("/tellconsent only works on player corpse targets.");
    return true;
  }

  auto name = Zeal::Game::strip_name(target->Name);
  std::string tell_message = std::string(name) + " " + "Consent me";
  Zeal::Game::do_tell(tell_message.c_str());
  return true;
}

// Sends a "Consent me" tell to the owner of the targeted corpse.
static bool handle_reply_consent() {
  const char(*tell_list)[64] = reinterpret_cast<const char(*)[64]>(0x007CE45C);
  const char *last_teller = tell_list[0];
  if (last_teller[0] == 0) {
    Zeal::Game::print_chat("No players in recent tell history.");
    return true;
  }
  Zeal::Game::do_consent(last_teller);
  return true;
}

void ZealService::AddCommands() {
  commands_hook->Add(
      "/alarm", {}, "Open the alarm ui and gives alarm functionality on old ui.",
      [this](std::vector<std::string> &args) {
        if (Zeal::Game::is_new_ui()) {
          if (Zeal::Game::Windows->Alarm)
            Zeal::Game::Windows->Alarm->IsVisible = true;
          else
            Zeal::Game::print_chat("Alarm window not found");

          return true;
        } else {
          if (args.size() == 1) {
            std::ostringstream oss;
            oss << "-- ALARM COMMANDS --" << std::endl << "/alarm set #m#s" << std::endl << "/alarm halt" << std::endl;
            Zeal::Game::print_chat(oss.str());
            return true;
          }
          if (args.size() > 1 && args.size() < 4) {
            if (Zeal::String::compare_insensitive(args[1], "set")) {
              int minutes = 0;
              int seconds = 0;
              size_t delim[2] = {args[2].find("m"), args[2].find("s")};
              if (Zeal::String::tryParse(args[2].substr(0, delim[0]), &minutes) &&
                  Zeal::String::tryParse(args[2].substr(delim[0] + 1, delim[1]), &seconds)) {
                alarm->Set(minutes, seconds);
                return true;
              } else {
                Zeal::Game::print_chat("[Alarm] Failed to parse the specified duration.");
                return true;
              }
            } else if (Zeal::String::compare_insensitive(args[1], "halt")) {
              alarm->Halt();
              return true;
            }
          }
        }
        return false;
      });

  commands_hook->Add(
      "/corpsedrag", {"/drag"}, "Attempts to corpse drag your current target. Use /corpsedrag nearest to auto-target.",
      [](std::vector<std::string> &args) {
        bool nearest = (args.size() == 2 && args[1] == "nearest");
        if (args.size() == 1 || nearest) {
          if (nearest) {
            auto *ent = ZealService::get_instance()->cycle_target->get_nearest_ent(250, Zeal::GameEnums::PlayerCorpse);
            if (ent) Zeal::Game::set_target(ent);
          }

          if (Zeal::Game::get_target() && (Zeal::Game::get_target()->Type == Zeal::GameEnums::PlayerCorpse)) {
            Zeal::Packets::CorpseDrag_Struct tmp;
            memset(&tmp, 0, sizeof(tmp));
            strcpy_s(tmp.CorpseName, 30, Zeal::Game::get_target()->Name);
            strcpy_s(tmp.DraggerName, 30, Zeal::Game::get_self()->Name);
            Zeal::Game::send_message(Zeal::Packets::opcodes::CorpseDrag, (int *)&tmp, sizeof(tmp), 0);
          } else if (nearest)
            Zeal::Game::print_chat("No corpse found nearby to drag.");
          else
            Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "Need to target a corpse to /drag (or use /drag nearest)");

          return true;  // return true to stop the game from processing any further on this command, false if you want
                        // to just add features to an existing cmd
        }
        return false;
      });
  commands_hook->Add("/corpsedrop", {"/drop"},
                     "Attempts to drop a corpse (your current target). To drop all use /corpsedrop all",
                     [](std::vector<std::string> &args) {
                       if (args.size() == 1) {
                         if (Zeal::Game::get_target()) {
                           Zeal::Packets::CorpseDrag_Struct tmp;
                           memset(&tmp, 0, sizeof(tmp));
                           strcpy_s(tmp.CorpseName, 30, Zeal::Game::get_target()->Name);
                           strcpy_s(tmp.DraggerName, 30, Zeal::Game::get_self()->Name);
                           Zeal::Game::send_message(Zeal::Packets::opcodes::CorpseDrop, (int *)&tmp, sizeof(tmp), 0);
                         } else
                           Zeal::Game::print_chat("Need to target a corpse to /drop (or use /drop all)");
                         return true;  // return true to stop the game from processing any further on this command,
                                       // false if you want to just add features to an existing cmd
                       } else if (Zeal::String::compare_insensitive(args[1], "all")) {
                         Zeal::Game::send_message(Zeal::Packets::opcodes::CorpseDrop, 0, 0, 0);
                         return true;
                       }
                       return false;
                     });
  commands_hook->Add("/trade", {"/opentrade", "/ot"}, "Opens a trade window with your current target.",
                     [](std::vector<std::string> &args) {
                       if (args.size() == 1) {
                         if (Zeal::Game::Windows->Trade->IsVisible || Zeal::Game::Windows->Give->IsVisible) {
                           // Disabled the auto-drop from the cursor since it needs more work (see /singlegive notes).
                           Zeal::Game::print_chat("Trade window already open");
                         } else {
                           if (Zeal::Game::get_target()) {
                             Zeal::Packets::TradeRequest_Struct tmp;
                             memset(&tmp, 0, sizeof(tmp));
                             tmp.from_id = Zeal::Game::get_self()->SpawnId;
                             tmp.to_id = Zeal::Game::get_target()->SpawnId;
                             Zeal::Game::send_message(Zeal::Packets::opcodes::RequestTrade, (int *)&tmp, sizeof(tmp),
                                                      0);
                           }
                         }
                         return true;  // return true to stop the game from processing any further on this command,
                                       // false if you want to just add features to an existing cmd
                       }
                       return false;
                     });
  commands_hook->Add(
      "/useitem", {}, "Use an item's right click function arugment is 0-29 which indicates the slot",
      [](std::vector<std::string> &args) {
        Zeal::GameStructures::GAMECHARINFO *char_info = Zeal::Game::get_char_info();
        Zeal::GameStructures::Entity *self = Zeal::Game::get_self();
        if (!char_info || !self || !self->ActorInfo) {
          Zeal::Game::print_chat(USERCOLOR_SHOUT, "[Fatal Error] Failed to get entity for useitem!");
          return true;
        }
        int item_index = 0;
        if (args.size() > 1 && Zeal::String::tryParse(args[1], &item_index)) {
          if (char_info->Class == Zeal::GameEnums::ClassTypes::Bard &&
              ZealService::get_instance()->melody->use_item(item_index))
            return true;
          bool quiet = args.size() > 2 && args[2] == "quiet";
          Zeal::Game::use_item(item_index, quiet);
        } else {
          Zeal::Game::print_chat(USERCOLOR_SPELL_FAILURE, "useitem requires an item slot between 0 and 29");
          Zeal::Game::print_chat("0: Left ear, 1: Head, 2: Face, 3: Right Ear, 4: Neck, 5: Shoulders");
          Zeal::Game::print_chat("6: Arms, 7: Back, 8: Left Wrist, 9: Right Wrist, 10: Ranged");
          Zeal::Game::print_chat("11: Hands, 12: Primary, 13: Secondary, 14: Left Finger, 15: Right Finger");
          Zeal::Game::print_chat("16: Chest, 17: Legs, 18: Feet, 19: Waist, 20: Ammo");
          Zeal::Game::print_chat("Inventory: 22: Top Left, 25: Bottom left, 26: Top Right, 29: Bottom Right");
        }
        return true;  // return true to stop the game from processing any further on this command, false if you want to
                      // just add features to an existing cmd
      });

  commands_hook->Add("/zeal", {"/zea"}, "Help and version information.", [this](std::vector<std::string> &args) {
    if (args.size() == 1) {
      Zeal::Game::print_chat("Available args: version, help");  // leave room for more args on this command for later
      return true;
    }
    if (args.size() > 1 && Zeal::String::compare_insensitive(args[1], "version")) {
      std::stringstream ss;
      ss << "Zeal version: " << ZEAL_VERSION << " (" << ZEAL_BUILD_VERSION << ")" << std::endl;
      Zeal::Game::print_chat(ss.str());
      HMODULE eqw_dll = GetModuleHandleA("eqw.dll");
      FARPROC eqw_fn = GetProcAddress(eqw_dll, "GetVersionStr");  // Exists in new eqw.dll only.
      const char *eqw_version = eqw_fn ? reinterpret_cast<const char *>(eqw_fn()) : "Legacy";
      Zeal::Game::print_chat("eqw.dll version: %s", eqw_version);
      HMODULE eqgame_dll = GetModuleHandleA("eqgame.dll");
      FARPROC eqgame_fn = GetProcAddress(eqgame_dll, "GetVersionStr");  // Exists in new eqgame.dll only.
      const char *eqgame_version = eqgame_fn ? reinterpret_cast<const char *>(eqgame_fn()) : "Unknown";
      Zeal::Game::print_chat("eqgame.dll version: %s", eqgame_version);
      return true;
    }
    if (args.size() == 2 && args[1] == "era") {  // TODO: Remove, temporary testing.
      auto char_info = Zeal::Game::get_char_info();
      BYTE char_expansions = char_info ? char_info->Expansions : 0;

      // OP_ExpansionInfo values:
      BYTE op_expansions = 0;
      if (*reinterpret_cast<DWORD *>(0x007cf1e8))  // gKunarkEnabled_007cf1e8
        op_expansions = op_expansions | 0x01;
      if (*reinterpret_cast<DWORD *>(0x007cf1ec))  // gVeliousEnabled_007cf1ec
        op_expansions = op_expansions | 0x02;
      if (*reinterpret_cast<DWORD *>(0x007cf1f0))  // gLuclinEnabled_007cf1f0
        op_expansions = op_expansions | 0x04;
      if (*reinterpret_cast<DWORD *>(0x007cf1f4))  // gPlanesOfPowerEnabled_007cf1f4
        op_expansions = op_expansions | 0x08;

      Zeal::Game::print_chat("Era bits: Character: 0x%02x, Op_ExpansionInfo: 0x%02x", char_expansions, op_expansions);
    }
    if (args.size() == 2 && args[1] == "bank")  // Temporary for bank patch testing.
    {
      Zeal::Game::print_chat("total: %i, personal: %i, shared: %i, size: 0x%x", Zeal::Game::get_num_total_bank_slots(),
                             Zeal::Game::get_num_personal_bank_slots(), Zeal::Game::get_num_shared_bank_slots(),
                             sizeof(Zeal::GameStructures::GAMECHARINFO));
      return true;
    }
    if (args.size() == 2 && args[1] == "entities") {
      ZealService::get_instance()->entity_manager.get()->Dump();
      return true;
    }
    if (args.size() == 3 && args[1] == "aa") {
      int index = 0;
      if (Zeal::String::tryParse(args[2], &index) && index <= 227)
        Zeal::Game::print_chat("AA[%d] = %d", index, Zeal::Game::get_self()->ActorInfo->AAAbilities[index]);
      else
        for (int i = 0; i <= 227; ++i) {
          BYTE value = Zeal::Game::get_self()->ActorInfo->AAAbilities[i];
          if (value) Zeal::Game::print_chat("AA[%d] = %d", i, value);
        }
      return true;
    }
    if (args.size() == 2 && args[1] == "check")  // Run a heap / memory check.
    {
      int heap_valid1 = HeapValidate(GetProcessHeap(), 0, NULL);
      Zeal::Game::print_chat("Process HeapValidate: %s", heap_valid1 ? "Pass" : "Fail");
      int heap_valid2 = HeapValidate(*Zeal::Game::Heap, 0, NULL);
      Zeal::Game::print_chat("Game HeapValidate: %s", heap_valid2 ? "Pass" : "Fail");

      HEAP_SUMMARY heap_summary;
      memset(&heap_summary, 0, sizeof(heap_summary));
      heap_summary.cb = sizeof(heap_summary);
      HeapSummary(*Zeal::Game::Heap, 0, &heap_summary);
      Zeal::Game::print_chat("Game Heap: Alloc: %d MB, Commit: %d MB", heap_summary.cbAllocated / 1024 / 1024,
                             heap_summary.cbCommitted / 1024 / 1024);
      return true;
    }
    if (args.size() == 3 && args[1] == "spell") {
      int spell_id = -1;
      if (Zeal::String::tryParse(args[2], &spell_id)) Zeal::Game::dump_spell_info(spell_id);
      return true;
    }
    if (args.size() == 2 && args[1] == "spelleffects") {
      const auto *spell_mgr = Zeal::Game::get_spell_mgr();
      int count = 0;
      for (int i = 1; i < GAME_NUM_SPELLS; ++i) {
        const auto *spell = spell_mgr->Spells[i];
        if (spell && spell->OldParticleEffect && !spell->NewParticleEffect) {
          Zeal::Game::dump_spell_info(i);
          count++;
        }
      }
      Zeal::Game::print_chat("Number of spells missing new effects: %d", count);
      return true;
    }
    if (args.size() == 3 && args[1] == "get_command") {
      auto command = Zeal::Game::get_command_struct(args[2]);
      if (command)
        Zeal::Game::print_chat("%s: id: %i, name: %s, localized: %s, gm: %i, category: %i, fn: 0x%08x", args[2].c_str(),
                               command->string_id, command->name ? command->name : "null",
                               command->localized_name ? command->localized_name : "null", command->gm_command,
                               command->category, command->fn);
      else
        Zeal::Game::print_chat("no matches");

      return true;
    }
    if (args.size() == 2 && args[1] == "time") {
      Zeal::Game::print_chat("Game timebase: %u", Zeal::Game::get_game_time());
      Zeal::Game::print_chat("CPU first timestamp ticks: %lld",
                             *reinterpret_cast<LONGLONG *>(0x008092c8));  // i64FirstTimeStampTicks
      Zeal::Game::print_chat("CPU ticks / second: %lld",
                             *reinterpret_cast<LONGLONG *>(0x008092d0));  // i64CPUTicksPerMillisecond
      return true;
    }
    if (args.size() == 2 && args[1] == "list_keybinds")  // Just a utility to check native keybind mapping.
    {
      binds_hook->print_keybinds();
      return true;
    }
    if (args.size() == 2 && args[1] == "target_name")  // Report name parsing of current target.
    {
      Zeal::GameStructures::Entity *target = Zeal::Game::get_target();
      if (target) {
        std::string original = target->Name;
        std::string trimmed = Zeal::Game::trim_name(target->Name);
        std::string stripped = Zeal::Game::strip_name(target->Name);
        Zeal::Game::print_chat("Raw: %s, Trim: %s, Strip: %s, Equal: %s", original.c_str(), trimmed.c_str(),
                               stripped.c_str(), trimmed == stripped ? "true" : "false");
        if (target->ActorInfo && target->ActorInfo->DagHeadPoint && target->ActorInfo->DagHeadPoint->StringSprite) {
          auto &sprite = *target->ActorInfo->DagHeadPoint->StringSprite;
          if (sprite.MagicValue == sprite.kMagicValidValue)
            Zeal::Game::print_chat("Sprite: %s, len: %i", sprite.Text, sprite.TextLength);
        }
        Zeal::Game::print_chat("Target: %#08x, Self: %#08x, Controlled: %#08x", target, Zeal::Game::get_self(),
                               Zeal::Game::get_controlled());
        if (target->ActorInfo && target->ActorInfo->ViewActor_)
          Zeal::Game::print_chat("Flags: %#08x", target->ActorInfo->ViewActor_->Flags);
        if (target->ActorInfo && target->ActorInfo->Mount) {
          auto mount = target->ActorInfo->Mount;
          Zeal::Game::print_chat("Mount: %#08x", mount);
          if (mount->ActorInfo && mount->ActorInfo->ViewActor_)
            Zeal::Game::print_chat("Mount flags: %#08x", mount->ActorInfo->ViewActor_->Flags);
        }
      }
      return true;
    }
    int sound_index = 0;
    if (args.size() == 3 && args[1] == "wave_play" && Zeal::String::tryParse(args[2], &sound_index)) {
      Zeal::Game::WavePlay(sound_index);
      return true;
    }
    if (args.size() > 1 && Zeal::String::compare_insensitive(args[1], "help")) {
      commands_hook->print_commands();
      return true;
    }
    return false;
  });

  commands_hook->Add(
      "/mystats", {"/mystat"}, "Calculate and report your current stats.", [this](std::vector<std::string> &args) {
        using Zeal::Game::print_chat;
        const char kMarker = 0x12;  // Link marker.
        int item_id = 0;
        if (args.size() == 2 && args[1] == "info") {
          print_chat(CHANNEL_MYSTATS, "---- mystats Beta info ----");
          print_chat(CHANNEL_MYSTATS, "Known simplifications:");
          print_chat(CHANNEL_MYSTATS, "  - Anti-twink defensive logic may not be accurate");
          print_chat(CHANNEL_MYSTATS, "  - All disciplines (offensive, defensive) are ignored");
          print_chat(CHANNEL_MYSTATS, "  - Range weapons, duel wield, double-attack will be in future update");
          print_chat(CHANNEL_MYSTATS, "Stat descriptions (all values include current spell effects):");
          print_chat(CHANNEL_MYSTATS,
                     "Mitigation: modifies incoming damage based on offense vs mitigation (0.1x to 2.0x factor)");
          print_chat(CHANNEL_MYSTATS,
                     "Mitigation (melee) ~= item_ac*4/3 + defense_skill/3 + agility/20 + spell_ac/4 + class_ac");
          print_chat(CHANNEL_MYSTATS,
                     "Note: The spell_ac value is an internal calc from the database. Sites like pqdi already include "
                     "the /4.");
          print_chat(CHANNEL_MYSTATS, "Avoidance: modifies probability of taking zero damage");
          print_chat(CHANNEL_MYSTATS, "Avoidance ~= (defense_skill*400/225 + 36 + (min(200,agi)-75)*2/15)*(1+AA_pct)");
          print_chat(CHANNEL_MYSTATS, "To Hit: sets probability of hitting based on to hit vs avoidance");
          print_chat(CHANNEL_MYSTATS, "To Hit ~= 7 + offense_skill + weap_skill + bonuses (item, spell, AA)");
          print_chat(CHANNEL_MYSTATS, "Offense: impacts both mitigation factor and damage multiplier");
          print_chat(CHANNEL_MYSTATS, "Offense ~= weap_skill_value + spell_atk + item_atk + max(0, (str-75)*2/3)");
          print_chat(CHANNEL_MYSTATS,
                     "Damage multiplier: Chance for bonus damage factor based on level, weapon skill, and offense");
          print_chat(CHANNEL_MYSTATS,
                     "Average damage: Mitigation factor = 1, damage multiplier = average after both rolls");
        } else if (args.size() == 2 && args[1] == "affects") {
          auto char_info = Zeal::Game::get_char_info();
          if (char_info) {
            const int SE_ArmorClass = 1;
            print_chat(CHANNEL_MYSTATS, "TotalSpellAffects: AC: %d",
                       Zeal::Game::total_spell_affects(char_info, SE_ArmorClass, true, nullptr));
          }
        } else if (args.size() >= 2 && args[1].size() >= 8 && args[1].front() == kMarker) {
          std::string link = args[1];  // Only need the first item ID part of the link (name doesn't matter).
          std::string item_id_str = link.substr(2, 6);
          if (link.front() == kMarker && Zeal::String::tryParse(item_id_str, &item_id) && item_id > 0) {
            auto zeal = this;
            const Zeal::GameStructures::GAMEITEMINFO *weapon = nullptr;
            if (zeal && zeal->item_displays && zeal->item_displays->get_cached_item(item_id)) {
              const auto weapon = zeal->item_displays->get_cached_item(item_id);
              Zeal::Game::print_melee_attack_stats(true, weapon, CHANNEL_MYSTATS);
              Zeal::Game::print_melee_attack_stats(false, weapon, CHANNEL_MYSTATS);
            } else
              print_chat(CHANNEL_MYSTATS, "Unable to locate a local copy of information for item %d", item_id);
          } else
            print_chat(CHANNEL_MYSTATS, "Failed to parse item link.");
        } else if (args.size() == 1) {
          bool is_luclin_enabled = (Zeal::Game::get_era() >= Zeal::Game::Era::Luclin);
          auto self = Zeal::Game::get_self();
          if (self) {
            Zeal::Game::print_chat(CHANNEL_MYSTATS, "---- Misc stats ----");
            auto horse = self->ActorInfo ? self->ActorInfo->Mount : nullptr;
            float speed = horse ? horse->MovementSpeed : self->MovementSpeed;
            print_chat(CHANNEL_MYSTATS, "Movement speed: %d%%", (int)(speed / 0.7 * 100 + 0.5));
            if (!horse && self->ActorInfo)
              print_chat(CHANNEL_MYSTATS, "Movement modifier: %+d%%",
                         (int)(self->ActorInfo->MovementSpeedModifier / 0.7 * 100 + 0.5));
          }
          Zeal::Game::print_chat(CHANNEL_MYSTATS, "---- Defensive stats ----");
          print_chat(CHANNEL_MYSTATS, "AC (display): %i = (Mit: %i  + Avoidance: %i) * 1000/847",
                     Zeal::Game::get_display_AC(), Zeal::Game::get_mitigation(), Zeal::Game::get_avoidance());
          print_chat(CHANNEL_MYSTATS, "Mitigation: %i (%s: %i)", Zeal::Game::get_mitigation(true),
                     is_luclin_enabled ? "softcap" : "hardcap", Zeal::Game::get_mitigation_softcap());
          print_chat(CHANNEL_MYSTATS, "Avoidance: %i (with AAs)",
                     Zeal::Game::get_avoidance(true));  // Includes combat_agility.
          Zeal::Game::print_melee_attack_stats(true, nullptr, CHANNEL_MYSTATS);
          Zeal::Game::print_melee_attack_stats(false, nullptr, CHANNEL_MYSTATS);
        } else
          print_chat(CHANNEL_MYSTATS, "Usage: /mystats, /mystats info, /mystats <item_id>, /mystats <item_link>");

        return true;
      });
  commands_hook->Add("/consentrogues", {}, "Consents all rogues in the raid.", [](std::vector<std::string> &args) {
    return handle_consent_class(Zeal::GameEnums::ClassTypes::Rogue);
  });
  commands_hook->Add("/consentmonks", {}, "Consents all monks in the raid.", [](std::vector<std::string> &args) {
    return handle_consent_class(Zeal::GameEnums::ClassTypes::Monk);
  });
  commands_hook->Add("/tellconsent", {"/tc"}, "Sends a tell with 'Consent me' to the owner of the targeted corpse.",
                     [](std::vector<std::string> &args) { return handle_tell_consent(); });
  commands_hook->Add("/replyconsent", {"/rc"}, "Does a /consent to the sender of most recent tell.",
                     [](std::vector<std::string> &args) { return handle_reply_consent(); });
  commands_hook->Add("/targetprevious", {}, "Switches to previous target (can toggle last two).",
                     [this](std::vector<std::string> &args) {
                       cycle_target->handle_toggle_last_two(true, true);
                       return true;
                     });
}

void ZealService::AddBinds() {
  binds_hook->replace_cmd(28, [this](int state) {
    if (state && !Zeal::Game::GameInternal::UI_ChatInputCheck()) {
      Zeal::GameStructures::Entity *ent = ZealService::get_instance()->cycle_target->get_nearest_ent(250, 0);
      if (ent) Zeal::Game::set_target(ent);
    }
    return true;
  });  // nearest pc
  binds_hook->replace_cmd(29, [this](int state) {
    if (state && !Zeal::Game::GameInternal::UI_ChatInputCheck()) {
      Zeal::GameStructures::Entity *ent = ZealService::get_instance()->cycle_target->get_nearest_ent(250, 1);
      if (ent) Zeal::Game::set_target(ent);
    }
    return true;
  });  // nearest npc
  binds_hook->replace_cmd(3, [this](int state) {
    ZealService::get_instance()->movement->handle_movement_binds(3, state);
    return false;
  });  // foward

  binds_hook->replace_cmd(4, [this](int state) {
    ZealService::get_instance()->movement->handle_movement_binds(4, state);
    return false;
  });  // back

  binds_hook->replace_cmd(5, [this](int state) {
    ZealService::get_instance()->movement->handle_movement_binds(5, state);
    return false;
  });  // turn right

  binds_hook->replace_cmd(6, [this](int state) {
    ZealService::get_instance()->movement->handle_movement_binds(6, state);
    return false;
  });  // turn left

  binds_hook->replace_cmd(30, [this](int state) {
    ZealService::get_instance()->netstat->toggle_netstat(state);
    return false;
  });

  for (int bind_index = 51; bind_index < 59; ++bind_index) {
    binds_hook->replace_cmd(bind_index, [this, bind_index](int state) {
      ZealService::get_instance()->movement->handle_spellcast_binds(bind_index);
      return false;
    });  // spellcasting auto-stand
  }

  binds_hook->replace_cmd(0xC8, [this](int state) {
    if (!Zeal::Game::is_new_ui()) return false;

    auto zeal = ZealService::get_instance();
    if (zeal->ui && zeal->ui->inputDialog && zeal->ui->inputDialog->isVisible()) {
      zeal->ui->inputDialog->hide();
      return true;
    }
    if (Zeal::Game::get_target()) {
      Zeal::Game::set_target(0);
      return true;
    }

    if (!zeal->ui->options->setting_escape_raid_lock.get()) {
      if (Zeal::Game::Windows && Zeal::Game::Windows->RaidOptions && Zeal::Game::Windows->RaidOptions->IsVisible) {
        Zeal::Game::Windows->RaidOptions->show(0, false);
        return true;
      }
      if (Zeal::Game::Windows && Zeal::Game::Windows->Raid && Zeal::Game::Windows->Raid->IsVisible) {
        Zeal::Game::execute_cmd(109, 1, 0);
        Zeal::Game::execute_cmd(109, 0, 0);
        return true;
      }
    }

    if (zeal->ui->options->setting_escape.get())  // toggle is set to not close any windows
      return true;

    if (zeal->item_displays && zeal->item_displays->close_latest_window()) return true;

    return false;
  });  // handle escape

  // just start binds at 211 to avoid overwriting any existing cmd/bind
  binds_hook->add_bind(211, "Strafe Left", "StrafeLeft", key_category::Movement, [this](int key_down) {});    // stub
  binds_hook->add_bind(212, "Strafe Right", "StrafeRight", key_category::Movement, [this](int key_down) {});  // stub
  binds_hook->add_bind(213, "Cycle through nearest NPCs", "CycleTargetNPC", key_category::Target,
                       [this](int key_down) { cycle_target->handle_next_target(key_down, Zeal::GameEnums::NPC); });
  binds_hook->add_bind(214, "Cycle through nearest PCs", "CycleTargetPC", key_category::Target,
                       [this](int key_down) { cycle_target->handle_next_target(key_down, Zeal::GameEnums::Player); });
  binds_hook->add_bind(215, "Toggle all containers", "OpenCloseContainers", key_category::UI | key_category::Commands,
                       [this](int key_down) {
                         if (key_down && !Zeal::Game::GameInternal::UI_ChatInputCheck())
                           utils->handle_toggle_all_containers();
                       });
  binds_hook->add_bind(216, "Toggle last two targets", "ToggleLastTwo", key_category::Target,
                       [this](int key_down) { cycle_target->handle_toggle_last_two(key_down); });
  binds_hook->add_bind(217, "Reply Target", "ReplyTarget", key_category::Target, [this](int key_down) {
    if (key_down && !Zeal::Game::GameInternal::UI_ChatInputCheck()) {
      Zeal::Game::GameInternal::ReplyTarget(Zeal::Game::get_self(), "");
    }
  });
  binds_hook->add_bind(218, "Pet Attack", "PetAttack", key_category::Commands,
                       [this](int key_down) {  // probably need to add a check if you have a pet
                         if (key_down && !Zeal::Game::GameInternal::UI_ChatInputCheck()) {
                           Zeal::GameStructures::Entity *target = Zeal::Game::get_target();
                           if (target) Zeal::Game::pet_command(Zeal::GameEnums::PetCommand::Attack, target->SpawnId);
                         }
                       });
  binds_hook->add_bind(219, "Pet Guard", "PetGuard", key_category::Commands, [this](int key_down) {
    if (key_down && !Zeal::Game::GameInternal::UI_ChatInputCheck()) {
      Zeal::Game::pet_command(Zeal::GameEnums::PetCommand::Guard, 0);
    }
  });
  binds_hook->add_bind(220, "Pet Back", "PetBack", key_category::Commands, [this](int key_down) {
    if (key_down && !Zeal::Game::GameInternal::UI_ChatInputCheck()) {
      Zeal::Game::pet_command(Zeal::GameEnums::PetCommand::Back, 0);
    }
  });
  binds_hook->add_bind(221, "Pet Follow", "PetFollow", key_category::Commands, [this](int key_down) {
    if (key_down && !Zeal::Game::GameInternal::UI_ChatInputCheck()) {
      Zeal::Game::pet_command(Zeal::GameEnums::PetCommand::Follow, 0);
    }
  });
  binds_hook->add_bind(222, "Pet Sit", "PetSit", key_category::Commands, [this](int key_down) {
    if (key_down && !Zeal::Game::GameInternal::UI_ChatInputCheck()) {
      Zeal::Game::pet_command(Zeal::GameEnums::PetCommand::Sit, 0);
    }
  });
  binds_hook->add_bind(223, "Slow Turn Right", "SlowMoveRight", key_category::Movement, [this](int key_down) {
    ZealService::get_instance()->movement->handle_slow_turn_right(key_down);
  });
  binds_hook->add_bind(224, "Slow Turn Left", "SlowMoveLeft", key_category::Movement, [this](int key_down) {
    ZealService::get_instance()->movement->handle_slow_turn_left(key_down);
  });
  binds_hook->add_bind(225, "Auto Fire", "AutoFire", key_category::Commands, [this](int key_down) {
    if (key_down && !Zeal::Game::GameInternal::UI_ChatInputCheck()) {
      ZealService::get_instance()->autofire->SetAutoFire(!ZealService::get_instance()->autofire->autofire, true);
    }
  });
  binds_hook->add_bind(226, "Target Nearest NPC Corpse", "TargetNPCCorpse", key_category::Target, [](int key_down) {
    if (key_down && !Zeal::Game::GameInternal::UI_ChatInputCheck()) {
      Zeal::GameStructures::Entity *ent = ZealService::get_instance()->cycle_target->get_nearest_ent(250, 2);
      if (ent) Zeal::Game::set_target(ent);
    }
  });
  binds_hook->add_bind(227, "Target Nearest PC Corpse", "TargetPCCorpse", key_category::Target, [](int key_down) {
    if (key_down && !Zeal::Game::GameInternal::UI_ChatInputCheck()) {
      Zeal::GameStructures::Entity *ent = ZealService::get_instance()->cycle_target->get_nearest_ent(250, 3);
      if (ent) Zeal::Game::set_target(ent);
    }
  });
  binds_hook->add_bind(228, "Toggle Map", "ToggleMap", key_category::UI, [this](int key_down) {
    if (key_down && !Zeal::Game::GameInternal::UI_ChatInputCheck()) {
      ZealService::get_instance()->zone_map->set_enabled(!ZealService::get_instance()->zone_map->is_enabled());
    }
  });
  binds_hook->add_bind(229, "Toggle Map Background", "ToggleMapBackground", key_category::UI, [this](int key_down) {
    if (key_down && !Zeal::Game::GameInternal::UI_ChatInputCheck()) {
      ZealService::get_instance()->zone_map->toggle_background();
    }
  });
  binds_hook->add_bind(230, "Toggle Map Zoom", "ToggleMapZoom", key_category::UI, [this](int key_down) {
    if (key_down && !Zeal::Game::GameInternal::UI_ChatInputCheck()) {
      ZealService::get_instance()->zone_map->toggle_zoom();
    }
  });
  binds_hook->add_bind(231, "Toggle Map Labels", "ToggleMapLabels", key_category::UI, [this](int key_down) {
    if (key_down && !Zeal::Game::GameInternal::UI_ChatInputCheck()) {
      ZealService::get_instance()->zone_map->toggle_labels();
    }
  });
  binds_hook->add_bind(232, "Toggle Map Level Up", "ToggleMapLevelUp", key_category::UI, [this](int key_down) {
    if (key_down && !Zeal::Game::GameInternal::UI_ChatInputCheck()) {
      ZealService::get_instance()->zone_map->toggle_level_up();
    }
  });
  binds_hook->add_bind(233, "Toggle Map Level Down", "ToggleMapLevelDown", key_category::UI, [this](int key_down) {
    if (key_down && !Zeal::Game::GameInternal::UI_ChatInputCheck()) {
      ZealService::get_instance()->zone_map->toggle_level_down();
    }
  });
  binds_hook->add_bind(234, "Toggle Map Show Raid", "ToggleMapShowRaid", key_category::UI, [this](int key_down) {
    if (key_down && !Zeal::Game::GameInternal::UI_ChatInputCheck()) {
      ZealService::get_instance()->zone_map->set_show_raid(
          !ZealService::get_instance()->zone_map->is_show_raid_enabled(), false);
    }
  });
  binds_hook->add_bind(235, "Toggle Nameplate Colors", "ToggleNameplateColors", key_category::Target,
                       [this](int key_down) {
                         if (key_down && !Zeal::Game::GameInternal::UI_ChatInputCheck())
                           ZealService::get_instance()->nameplate->setting_colors.toggle(false);
                       });
  binds_hook->add_bind(236, "Toggle Nameplate Con Colors", "ToggleNameplateConColors", key_category::Target,
                       [this](int key_down) {
                         if (key_down && !Zeal::Game::GameInternal::UI_ChatInputCheck())
                           ZealService::get_instance()->nameplate->setting_con_colors.toggle(false);
                       });
  binds_hook->add_bind(237, "Toggle Map Member Names", "FlashMapMemberNames", key_category::UI, [this](int key_down) {
    // Left the short name as "Flash" to stay consistent with previous keybinds.
    if (key_down && !Zeal::Game::GameInternal::UI_ChatInputCheck()) {
      ZealService::get_instance()->zone_map->set_show_all_names_override(
          !ZealService::get_instance()->zone_map->is_show_all_names_override());
    }
  });
  binds_hook->add_bind(238, "Toggle Nameplate Self", "ToggleNameplateSelf", key_category::Target, [this](int key_down) {
    if (key_down && !Zeal::Game::GameInternal::UI_ChatInputCheck())
      ZealService::get_instance()->nameplate->setting_hide_self.toggle(false);
  });
  binds_hook->add_bind(239, "Toggle Nameplate Self as X", "ToggleNameplateX", key_category::Target,
                       [this](int key_down) {
                         if (key_down && !Zeal::Game::GameInternal::UI_ChatInputCheck())
                           ZealService::get_instance()->nameplate->setting_x.toggle(false);
                       });
  binds_hook->add_bind(240, "Toggle Nameplate Raid Pets", "ToggleNameplateRaidPets", key_category::Target,
                       [this](int key_down) {
                         if (key_down && !Zeal::Game::GameInternal::UI_ChatInputCheck())
                           ZealService::get_instance()->nameplate->setting_hide_raid_pets.toggle(false);
                       });
  binds_hook->add_bind(241, "Toggle Map Grid Lines", "ToggleMapGridLines", key_category::UI, [this](int key_down) {
    if (key_down && !Zeal::Game::GameInternal::UI_ChatInputCheck())
      ZealService::get_instance()->zone_map->set_show_grid(
          !ZealService::get_instance()->zone_map->is_show_grid_enabled(), false);
  });
  binds_hook->add_bind(242, "Toggle Map Interactive Mode", "ToggleMapInteractiveMode", key_category::UI,
                       [this](int key_down) {
                         if (key_down && !Zeal::Game::GameInternal::UI_ChatInputCheck())
                           ZealService::get_instance()->zone_map->set_interactive_enable(
                               !ZealService::get_instance()->zone_map->is_interactive_enabled(), false);
                       });
  binds_hook->add_bind(
      243, "Cycle through near PC corpses", "CycleTargetPCCorpses", key_category::Target,
      [this](int key_down) { cycle_target->handle_next_target(key_down, Zeal::GameEnums::PlayerCorpse); });
  binds_hook->add_bind(244, "Buy/Sell Stack", "BuySell", key_category::UI, [](int key_down) {
    if (key_down && !Zeal::Game::GameInternal::UI_ChatInputCheck()) {
      if (Zeal::Game::is_in_game() && Zeal::Game::Windows && Zeal::Game::Windows->Merchant &&
          Zeal::Game::Windows->Merchant->IsVisible) {
        Zeal::GameUI::CXWndManager *wnd_mgr = Zeal::Game::get_wnd_manager();
        if (!wnd_mgr) return;
        DWORD selected_slot = Zeal::Game::Windows->Merchant->InventoryItemSlot;
        if (selected_slot >= 6000 && selected_slot < 6080) {
          // Buying an item
          if (!Zeal::Game::Windows->Merchant->BuyButton || !Zeal::Game::Windows->Merchant->BuyButton->IsEnabled) return;
        } else {
          // Selling an Item
          if (!Zeal::Game::Windows->Merchant->SellButton || !Zeal::Game::Windows->Merchant->SellButton->IsEnabled)
            return;
        }
        BYTE shift = wnd_mgr->ShiftKeyState;
        BYTE ctrl = wnd_mgr->ControlKeyState;
        BYTE alt = wnd_mgr->AltKeyState;
        wnd_mgr->ShiftKeyState = 1;
        wnd_mgr->ControlKeyState = 0;
        wnd_mgr->AltKeyState = 0;
        int quantity = -1;
        Zeal::Game::Windows->Merchant->WndNotification((int)Zeal::Game::Windows->Merchant, 29, (int)&quantity);
        wnd_mgr->ShiftKeyState = shift;
        wnd_mgr->ControlKeyState = ctrl;
        wnd_mgr->AltKeyState = alt;
      }
    }
  });
  binds_hook->add_bind(245, "Close all tell windows", "CloseAllTellWindows", key_category::Chat, [](int key_down) {
    if (key_down && !Zeal::Game::GameInternal::UI_ChatInputCheck())
      ZealService::get_instance()->tells->CloseAllWindows();
  });
  binds_hook->add_bind(246, "Loot target", "LootTarget", key_category::Commands, [](int key_down) {
    if (key_down && !Zeal::Game::GameInternal::UI_ChatInputCheck())
      reinterpret_cast<void (*)(void)>(0x004fb5ae)();  // do_loot().
  });
  binds_hook->add_bind(247, "Pet Health", "PetHealth", key_category::Commands, [this](int key_down) {
    if (key_down && !Zeal::Game::GameInternal::UI_ChatInputCheck())
      Zeal::Game::pet_command(Zeal::GameEnums::PetCommand::Health, 0);
  });
  binds_hook->add_bind(248, "Close most recent tell window", "CloseRecentTellWindow", key_category::Chat,
                       [](int key_down) {
                         if (key_down && !Zeal::Game::GameInternal::UI_ChatInputCheck())
                           ZealService::get_instance()->tells->CloseMostRecentWindow();
                       });
  binds_hook->add_bind(249, "Pet Hold", "PetHold", key_category::Commands, [this](int key_down) {
    if (key_down && !Zeal::Game::GameInternal::UI_ChatInputCheck()) {
      Zeal::Game::pet_command(Zeal::GameEnums::PetCommand::Hold, 0);
    }
  });
  binds_hook->add_bind(250, "Assist", "Assist", key_category::Target, [this](int key_down) {
    if (key_down && !Zeal::Game::GameInternal::UI_ChatInputCheck()) {
      auto do_assist_fn = reinterpret_cast<void (*)(Zeal::GameStructures::Entity *, const char *)>(0x004fd7dc);
      do_assist_fn(Zeal::Game::get_self(), "");
    }
  });

  binds_hook->add_bind(255, "Auto Inventory", "AutoInventory", key_category::Commands | key_category::Macros,
                       [](int key_down) {
                         if (key_down) {
                           if (Zeal::Game::can_inventory_item(Zeal::Game::get_char_info()->CursorItem)) {
                             Zeal::Game::GameInternal::auto_inventory(Zeal::Game::get_char_info(),
                                                                      &Zeal::Game::get_char_info()->CursorItem, 0);
                           } else {
                             if (Zeal::Game::get_char_info()->CursorItem)
                               Zeal::Game::print_chat(USERCOLOR_LOOT, "Cannot auto inventory %s",
                                                      Zeal::Game::get_char_info()->CursorItem->Name);
                           }
                         }
                       });
}

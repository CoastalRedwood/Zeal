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
#include "survey.h"
#include "target_ring.h"
#include "tellwindows.h"
#include "tick.h"
#include "tooltip.h"
#include "ui_manager.h"
#include "ui_skin.h"
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
  ss << "Note: This is known issue so not worth reporting. Unknown if Zeal can fix. Just abort and retry until it "
        "works.\n";
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
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
  // since the hooked functions are called back via a different thread, make sure the service ptr is available
  // immediately
  ZealService::ptr_service = this;  // this setup makes it not unit testable but since the caller functions of the hooks
                                    // don't know the pointers I had to make a method to retrieve the base atleast
  crash_handler = MakeCheckedUnique(CrashHandler);
  hooks = MakeCheckedUnique(HookWrapper);
  ini = MakeCheckedUnique(IO_ini, ZealSetting<bool>::kIniFilename, true);  // other functions rely on this hook
  UISkin::initialize_mode(this);  // Configure font size (patches client and impacts path for Zeal UI xml files).
  UISkin::configuration_check();  // First order check that the required uifiles exist.
  dx = MakeCheckedUnique(DirectX);
  // initialize the hooked function classes
  commands_hook = MakeCheckedUnique(ChatCommands);  // other classes below rely on this class on initialize
  callbacks = MakeCheckedUnique(CallbackManager);   // other functions rely on this hook
  looting_hook = MakeCheckedUnique(Looting);
  labels_hook = MakeCheckedUnique(Labels);
  pipe = MakeCheckedUnique(NamedPipe);  // other classes below rely on this class on initialize
  binds_hook = MakeCheckedUnique(Binds);
  raid_hook = MakeCheckedUnique(Raid);
  gamestr_hook = MakeCheckedUnique(GameStr);
  equip_item_hook = MakeCheckedUnique(EquipItem);
  spell_sets = MakeCheckedUnique(SpellSets);
  item_displays = MakeCheckedUnique(ItemDisplay);
  tooltips = MakeCheckedUnique(Tooltip);
  floating_damage = MakeCheckedUnique(FloatingDamage);
  give = MakeCheckedUnique(NPCGive);
  game_patches = MakeCheckedUnique(Patches);
  nameplate = MakeCheckedUnique(NamePlate);
  tells = MakeCheckedUnique(TellWindows);
  helm = MakeCheckedUnique(HelmManager);
  music = MakeCheckedUnique(MusicManager);
  charselect = MakeCheckedUnique(CharacterSelect);
  entity_manager = MakeCheckedUnique(EntityManager);
  camera_mods = MakeCheckedUnique(CameraMods);
  cycle_target = MakeCheckedUnique(CycleTarget);
  assist = MakeCheckedUnique(Assist);
  experience = MakeCheckedUnique(Experience);
  chat_hook = MakeCheckedUnique(Chat);
  chatfilter_hook = MakeCheckedUnique(chatfilter);
  outputfile = MakeCheckedUnique(OutputFile);
  buff_timers = MakeCheckedUnique(BuffTimers);
  movement = MakeCheckedUnique(PlayerMovement);
  alarm = MakeCheckedUnique(Alarm);
  netstat = MakeCheckedUnique(Netstat);
  ui = MakeCheckedUnique(UIManager);
  melody = MakeCheckedUnique(Melody);
  autofire = MakeCheckedUnique(AutoFire);
  physics = MakeCheckedUnique(Physics);
  target_ring = MakeCheckedUnique(TargetRing);
  zone_map = MakeCheckedUnique(ZoneMap);
  tick = MakeCheckedUnique(Tick);
  survey = MakeCheckedUnique(Survey);

  callbacks->AddGeneric([this]() {
    if (Zeal::Game::is_in_game() && print_buffer.size()) {
      for (auto &str : print_buffer) Zeal::Game::print_chat(USERCOLOR_SHOUT, "Zeal: %s", str.c_str());
      print_buffer.clear();
    }
  });
}

ZealService::~ZealService() { ZealService::ptr_service = nullptr; }

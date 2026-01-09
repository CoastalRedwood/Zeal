#pragma once

#include <memory>
#include <string>
#include <vector>

#define ZEAL_VERSION "1.3.5"
#ifndef ZEAL_BUILD_VERSION               // Set by github actions
#define ZEAL_BUILD_VERSION "UNOFFICIAL"  // Local build
#endif

// Singleton-like class that instantiates the Zeal functionality. The dll main should create a
// single copy of this class to install and activate Zeal.
class ZealService {
 private:
  ZealService();  // Use the create() for this singleton object.
  ZealService(const ZealService &) = delete;
  ZealService &operator=(const ZealService &) = delete;

 public:
  ~ZealService();

  // Creates this singleton object. Split from get_instance() since known when it occurs.
  static void create() {
    if (!ptr_service) new ZealService();
  }

  // Returns a pointer to the singleton object initialized in create.
  static ZealService *get_instance() { return ptr_service; }

  // Returns the first line where a heap integrity failure was detected (if any).
  static int get_heap_failed_line();

  // Defers the message until the UI is ready to print it.
  void queue_chat_message(const std::string &message) { print_buffer.push_back(message); }

  // Public class members to allow inter-member calls.
  // The list should be ordered in dependency order starting with the base/common classes
  // (hooks, callbacks, ini) to general utility (just in case the destructor order ever matters).
  std::unique_ptr<class CrashHandler> crash_handler = nullptr;
  std::unique_ptr<class IO_ini> ini = nullptr;
  std::unique_ptr<class HookWrapper> hooks = nullptr;
  std::unique_ptr<class CallbackManager> callbacks = nullptr;
  std::unique_ptr<class ChatCommands> commands_hook = nullptr;
  std::unique_ptr<class EntityManager> entity_manager = nullptr;
  std::unique_ptr<class Binds> binds_hook = nullptr;

  std::unique_ptr<class Patches> game_patches = nullptr;
  std::unique_ptr<class Physics> physics = nullptr;
  std::unique_ptr<class DirectX> dx = nullptr;
  std::unique_ptr<class GameStr> gamestr_hook = nullptr;
  std::unique_ptr<class CycleTarget> cycle_target = nullptr;
  std::unique_ptr<class CameraMods> camera_mods = nullptr;
  std::unique_ptr<class Raid> raid_hook = nullptr;
  std::unique_ptr<class Tooltip> tooltips = nullptr;
  std::unique_ptr<class Assist> assist = nullptr;
  std::unique_ptr<class OutputFile> outputfile = nullptr;
  std::unique_ptr<class PlayerMovement> movement = nullptr;
  std::unique_ptr<class MusicManager> music = nullptr;
  std::unique_ptr<class Alarm> alarm = nullptr;
  std::unique_ptr<class Melody> melody = nullptr;
  std::unique_ptr<class AutoFire> autofire = nullptr;
  std::unique_ptr<class Netstat> netstat = nullptr;
  std::unique_ptr<class Tick> tick = nullptr;
  std::unique_ptr<class BuffTimers> buff_timers = nullptr;
  std::unique_ptr<class HelmManager> helm = nullptr;

  std::unique_ptr<class Triggers> triggers = nullptr;
  std::unique_ptr<class TargetRing> target_ring = nullptr;
  std::unique_ptr<class FloatingDamage> floating_damage = nullptr;

  std::unique_ptr<class Utils> utils = nullptr;
  std::unique_ptr<class Experience> experience = nullptr;
  std::unique_ptr<class Labels> labels_hook = nullptr;
  std::unique_ptr<class ItemDisplay> item_displays = nullptr;
  std::unique_ptr<class EquipItem> equip_item_hook = nullptr;
  std::unique_ptr<class chatfilter> chatfilter_hook = nullptr;
  std::unique_ptr<class Chat> chat_hook = nullptr;
  std::unique_ptr<class NamePlate> nameplate = nullptr;
  std::unique_ptr<class TellWindows> tells = nullptr;
  std::unique_ptr<class Looting> looting_hook = nullptr;
  std::unique_ptr<class NPCGive> give = nullptr;

  std::unique_ptr<class ZoneMap> zone_map = nullptr;
  std::unique_ptr<class UIManager> ui = nullptr;
  std::unique_ptr<class CharacterSelect> charselect = nullptr;
  std::unique_ptr<class SpellSets> spell_sets = nullptr;
  std::unique_ptr<class Survey> survey = nullptr;

  std::unique_ptr<class NamedPipe> pipe = nullptr;

 private:
  void AddCommands();  // Registers module dependent chat commands.
  void AddBinds();     // Registers module dependent key binds.

  static ZealService *ptr_service;        // Pointer to this singleton-like object.
  std::vector<std::string> print_buffer;  // Queues/defers prints until UI is ready.
};

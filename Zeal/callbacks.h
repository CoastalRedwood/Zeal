#pragma once
#include <Windows.h>

#include <functional>
#include <string>
#include <unordered_map>

#include "game_packets.h"
#include "game_structures.h"
#include "game_ui.h"

// Types used for generic notification or for signaling in custom handlers.
enum class callback_type {
  CharacterSelect,      // Called at entry of DoCharacterSelect
  InitCharSelectUI,     // Called after allocation of new ui objects for character select.
  CharacterSelectLoop,  // Hooked into Render_MinWorld that is repeatedly called in character select.
  CleanCharSelectUI,    // Called at exit of DoCharacterSelect
  EnterZone,            // Called before entering a zone (character is known at this point).
  InitUI,               // Called after allocation of new UI objects. Happens after EnterZone.
  MainLoop,             // Called at start of inner DoMainLoop loop.
  DrawWindows,          // Called in DoMainLoop and character select loop (CXWndManager::DrawWindows).
  Render,               // Called at end of inner DoMainLoop loop (Hooks CDisplay::Render_World).
  RenderUI,             // Called internally by t3dUpdateDisplay and t3dRenderPartialScene (char select and in game).
  DeactivateUI,         // Called upon exiting zone (and elsewhere)
  CleanUI,              // Called before deallocation of new UI objects (before InitUI and in Display destructor).
  EndMainLoop,          // Called after return of DoMainLoop (exiting zone).
  WorldMessage,         // Incoming server message.
  SendMessage_,         // Outgoing server message.
  ExecuteCmd,           // Intercepts commands (e.g. keybinds) for optional overriding.
  DXReset,              // Hooked into d3d driver's Reset (before call).
  DXResetComplete,      // Hooked into d3d driver's Reset (after call).
  EndScene,             // Hooked into d3d driver's EndScene vtable call.
  ReportSuccessfulHitPost,  // Called after client hooked call executes.
  EntitySpawn,              // New entity object added to the world.
  EntityDespawn,            // Existing entity object removed from the world.
};

class CallbackManager {
 public:
  void AddGeneric(std::function<void()> callback_function, callback_type fn = callback_type::MainLoop);
  void AddPacket(std::function<bool(UINT, char *, UINT)> callback_function,
                 callback_type fn = callback_type::WorldMessage);
  void AddCommand(std::function<bool(UINT, BOOL)> callback_function, callback_type fn = callback_type::ExecuteCmd);
  void AddDelayed(std::function<void()> callback_function, int ms);
  void AddEntity(std::function<void(struct Zeal::GameStructures::Entity *)> callback_function, callback_type cb);
  void AddOutputText(
      std::function<void(struct Zeal::GameUI::ChatWnd *&wnd, std::string &msg, short &channel)> callback_function);
  void AddReportSuccessfulHit(
      std::function<void(struct Zeal::GameStructures::Entity *source, struct Zeal::GameStructures::Entity *target,
                         WORD type, short spell_id, short damage, char output_text)>
          callback_function);
  void invoke_ReportSuccessfulHit(struct Zeal::Packets::Damage_Struct *dmg, char output_text);
  void invoke_player(struct Zeal::GameStructures::Entity *ent, callback_type cb);
  void invoke_generic(callback_type fn);
  bool invoke_packet(callback_type fn, UINT opcode, char *buffer, UINT len);
  bool invoke_command(callback_type fn, UINT opcode, bool state);
  void invoke_outputtext(struct Zeal::GameUI::ChatWnd *&wnd, std::string &msg, short &channel);
  void invoke_delayed();
  std::string get_trace() const;
  CallbackManager(class ZealService *zeal);
  ~CallbackManager();

 private:
  std::vector<std::pair<ULONGLONG, std::function<void()>>> delayed_functions;
  std::unordered_map<callback_type, std::vector<std::function<void()>>> generic_functions;
  std::unordered_map<callback_type, std::vector<std::function<bool(UINT, char *, UINT)>>> packet_functions;
  std::unordered_map<callback_type, std::vector<std::function<bool(UINT, BOOL)>>> cmd_functions;
  std::unordered_map<callback_type, std::vector<std::function<void(struct Zeal::GameStructures::Entity *)>>>
      player_spawn_functions;
  std::vector<std::function<void(struct Zeal::GameUI::ChatWnd *&wnd, std::string &msg, short &channel)>>
      output_text_functions;
  std::vector<
      std::function<void(struct Zeal::GameStructures::Entity *source, struct Zeal::GameStructures::Entity *victim,
                         WORD type, short spell_id, short damage, char output_text)>>
      ReportSuccessfulHit_functions;
};

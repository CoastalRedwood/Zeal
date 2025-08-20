#pragma once
#include <Windows.h>

#include <functional>
#include <string>
#include <unordered_map>

#include "game_packets.h"
#include "game_structures.h"
#include "game_ui.h"

enum class callback_type {
  MainLoop,
  Zone,
  CleanUI,
  Render,
  CharacterSelect,
  InitUI,
  EndMainLoop,
  WorldMessage,
  SendMessage_,
  ExecuteCmd,
  Delayed,
  RenderUI,
  EndScene,
  DrawWindows,
  DXReset,
  DXResetComplete,
  EntitySpawn,
  EntityDespawn,
  AddOutputText,
  ReportSuccessfulHit,
  DeactivateUI,
  CharacterSelectLoop,
  InitCharSelectUI
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
  void eml();

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

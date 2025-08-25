#pragma once
#include <Windows.h>

#include "game_packets.h"
#include "zeal_settings.h"

class HelmManager {
 public:
  HelmManager(class ZealService *zeal);
  ~HelmManager();

  static WORD GetHelmMaterial(Zeal::GameStructures::GAMEITEMINFO *item);
  static WORD ToCanonicalHelmID(WORD material, WORD race);
  static WORD ToRacialHelmMaterial(WORD material, WORD race, BYTE gender);

  // Hook implementations
  void HandleWearChangeArmor(Zeal::GameStructures::Display *cDisplay, Zeal::GameStructures::Entity *spawn,
                             int wear_slot, WORD new_material, WORD old_material, DWORD color_tint, int local_only);
  int HandleSwapHead(Zeal::GameStructures::Display *cDisplay, Zeal::GameStructures::Entity *entity, int new_material,
                     int old_material, DWORD color, int local_only);
  int HandleIllusion(Zeal::GameStructures::Entity *entity, Zeal::Packets::Illusion_Struct *illusion);

  // Refreshes your helmet graphics to your current equipment and ShowHelm toggle.
  void RefreshHelm(bool local_only = false, int delay_ms = 64);

  // Toggles ShowHelm status
  ZealSetting<bool> ShowHelmEnabled = {true, "Zeal", "ShowHelm", true, [this](bool v) {
                                         SyncShowHelm(false);
                                         RefreshHelm(false, 0);
                                       }};

 private:
  bool Handle_In_OP_WearChange(Zeal::Packets::WearChange_Struct *data);
  bool Handle_Out_OP_WearChange(Zeal::Packets::WearChange_Struct *data);
  bool Handle_Showhelm(const std::vector<std::string> &args);

  // Checks if this race/gender combination needs special handling for Velious helms
  bool IsHelmBuggedOldModel(WORD race, BYTE gender);

  void RefreshHelmCallback(bool local_only);

  // Does any necessary helm adjustments/initializaiton on zone.
  void OnZone();

  // Sends the '#showhelm' command to the server with our current toggle.
  void SyncShowHelm(bool server_silent);

  // This call generally doesn't succeed until partway into the enter-world process.
  // So we do just-in-time detection when the first SwapHead() hook is called.
  void DetectInstalledFixes();

  // Suppresses outbound helm WearChange packets from being sent when positive.
  // Sometimes enabled when we are internally manipulating things and don't want that to generate uncessary outbound
  // packets. Usage is to increment to suppress, and drecement to unsuppress (Works recursively).
  UINT block_outbound_wearchange = 0;

  // Used by RefreshHelmetCallback to prevent running multiple times if queued in succession.
  bool refresh_helm_pending = false;
  Zeal::Packets::WearChange_Struct last_wc = {0};

  // We do some initialization on enter world once per-character
  std::string last_seen_character = "";

  // Auto-detected flags on startup that tell us if their Velious asset patch is installed
  bool DetectInstalledFixesComplete = false;
  bool UseHumanFemaleFix = false;
  bool UseBarbarianFemaleFix = false;
  bool UseEruditeMaleFix = false;
  bool UseEruditeFemaleFix = false;
  bool UseWoodElfFemaleFix = false;
  bool UseDarkElfFemaleFix = false;
};
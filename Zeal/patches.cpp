#include "patches.h"

#include "commands.h"
#include "game_functions.h"
#include "game_packets.h"
#include "game_structures.h"
#include "hook_wrapper.h"
#include "memory.h"
#include "string_util.h"
#include "zeal.h"

void __fastcall GetZoneInfoFromNetwork(int *t, int unused, char *p1) {
  int *backup_this = t;

  ZealService::get_instance()->hooks->hook_map["GetZoneInfoFromNetwork"]->original(GetZoneInfoFromNetwork)(t, unused,
                                                                                                           p1);
  int retry_count = 0;
  while (!t) {
    retry_count++;
    Sleep(100);
    t = backup_this;
    ZealService::get_instance()->hooks->hook_map["GetZoneInfoFromNetwork"]->original(GetZoneInfoFromNetwork)(t, unused,
                                                                                                             p1);
    if (retry_count >= 15 && !t) {
      MessageBoxA(NULL, "Zeal attempted to retry GetZoneInfoFromNetwork but has failed", "Crash", 0);
      break;
    }
  }
}

// Base client is tagging other players that die as Type = 2 (NPCCorpse) while tagging a self death
// as Type = 3. Upon camping and rejoining, other player corpses are tagged as Type = 3, so this
// looks like a client bug to patch (impacts corpse nameplates and targeting).
static void __fastcall ProcessDeath(uint32_t passthruECX, uint32_t unusedEDX,
                                    Zeal::Packets::Death_Struct *death_struct) {
  auto *ent = Zeal::Game::get_entity_by_id(death_struct->spawn_id);
  bool player_death = (ent != nullptr && ent->Type == Zeal::GameEnums::Player);
  ZealService::get_instance()->hooks->hook_map["ProcessDeath"]->original(ProcessDeath)(passthruECX, unusedEDX,
                                                                                       death_struct);
  if (player_death && ent->Type == Zeal::GameEnums::NPCCorpse) ent->Type = Zeal::GameEnums::PlayerCorpse;
}

// There is a client startup crash issue where it looks like the CBreathWnd::OnProcessFrame or
// game _3DView::DisplaySpells is calling CanIBreathe with a GAMECHARINFO that has a null SpawnInfo entry.
// The other path calling CanIBreathe protects against this.
static int32_t __fastcall CanIBreathe(Zeal::GameStructures::GAMECHARINFO *self_char_info, uint32_t unusedEDX) {
  if (!self_char_info) return 1;  // Not expected to happen, so just default to true, can breathe.

  // Patch the crashing case (null SpawnInfo) here.
  if (!self_char_info->SpawnInfo) {
    self_char_info->IsSwimmingUnderwater = 0;  // Match the updating behavior of CanIBreathe with an assumption.
    return 1;                                  // And just respond that yes can breathe (for now).
  }

  return ZealService::get_instance()->hooks->hook_map["CanIBreathe"]->original(CanIBreathe)(self_char_info, unusedEDX);
}

void Patches::SetBrownSkeletons() {
  if (BrownSkeletons.get()) {
    mem::write<BYTE>(0x49f297, 0xEB);
  } else {
    mem::write<BYTE>(0x49f297, 0x75);
  }
}

// Support an option to simply no-op the DoSpriteEffects() call to avoid the dvps.dll crash.
void Patches::SyncDisableSprites() {
  const BYTE kOpcodeNop = 0x90;
  const int kDoSpriteEffectAddr = 0x0052cbb1;
  bool disable = setting_DisableSprites.get();
  bool currently_disabled = (*reinterpret_cast<BYTE *>(kDoSpriteEffectAddr) == kOpcodeNop);
  if (disable == currently_disabled) return;

  if (disable) {
    mem::set(kDoSpriteEffectAddr, kOpcodeNop, 5);  // No-op out the call.
  } else {
    const BYTE orig_code[5] = {0xe8, 0x89, 0xeb, 0xff, 0xff};  // Restore call.
    mem::write(kDoSpriteEffectAddr, orig_code);
  }
}

static constexpr int kNumBardEffects = 3;

// Optional bard effects modes to reduce the super bright blue sparklies of constant bard song refreshes.
bool Patches::SyncBardEffects() {
  // Option 1: Use the pulsing sphere from Selo's (SpellAnim 98).
  // Option 2: Use the more subtle floating notes animation from Nature's Melody (SpellAnim 45).
  // Option 3: Use the even more subtle snowflake-like from Hymn/Cantata's (SpellAnim 69).
  static constexpr int kReferenceSongIds[kNumBardEffects] = {717, 2050, 7};
  static const char *kReferenceSongNames[kNumBardEffects] = {"Selo`s Accelerando", "Nature's Melody",
                                                             "Hymn of Restoration"};

  // These songs are by default all using old particle effects with SpellAffectIndex = 6.
  static constexpr std::pair<int, const char *> kSongsToModify[] = {
      {709, "Guardian Rhythms"},
      {710, "Elemental Rhythms"},
      {711, "Purifying Rhythms"},
      {712, "Psalm of Warmth"},
      {713, "Psalm of Cooling"},
      {714, "Psalm of Mystic Shielding"},
      {715, "Psalm of Vitality"},
      {716, "Psalm of Purity"},
      {722, "Jaxan`s Jig o` Vigor"},
      {723, "Cassindra`s Chorus of Clarity"},
      {1287, "Cassindra`s Chant of Clarity"},
      {2607, "Elemental Chorus"},
      {2608, "Purifying Chorus"},
      {3368, "Psalm of Veeshan"},
  };

  // First check we have access to the spell list and the reference song is a match.
  const auto *spell_mgr = Zeal::Game::get_spell_mgr();
  if (!spell_mgr) return false;

  int mode = setting_BardEffects.get();
  DWORD new_effect = (DWORD) nullptr;  // Default uses the old particle effect.
  if (mode > 0 && mode <= kNumBardEffects) {
    int index = mode - 1;
    const auto ref_spell = spell_mgr->Spells[kReferenceSongIds[index]];
    if (!ref_spell || !ref_spell->Name || !ref_spell->NewParticleEffect ||
        strcmp(ref_spell->Name, kReferenceSongNames[index]))
      return false;
    new_effect = ref_spell->NewParticleEffect;
  }

  // For every song, double-check it's a match (maybe the db gets updated) and apply the target
  // effect only if there's a match.
  bool no_errors = true;
  for (auto &pair : kSongsToModify) {
    auto spell = spell_mgr->Spells[pair.first];
    if (!spell || !spell->Name || spell->SpellAffectIndex != 6 || strcmp(spell->Name, pair.second))
      no_errors = false;
    else
      spell->NewParticleEffect = new_effect;
  }
  return no_errors;
}

bool Patches::HandleSpellEffectsCommand(const std::vector<std::string> &args) {
  if (args.size() == 2 && args[1] == "nosprites") {
    setting_DisableSprites.toggle();
    Zeal::Game::print_chat("No sprites: %s", setting_DisableSprites.get() ? "True" : "False");
  } else if (args.size() == 3 && args[1] == "bard") {
    int mode = 0;
    if (!Zeal::String::tryParse(args[2], &mode, true) || mode < 0 || mode > kNumBardEffects) {
      Zeal::Game::print_chat("Error: bard effects mode must be between 0 and %d", kNumBardEffects);
      return true;
    }
    setting_BardEffects.set(mode);
    Zeal::Game::print_chat("Bard effects mode: %d", setting_BardEffects.get());
    // This sync happens in the set above but call again to see if there was an error.
    if (!SyncBardEffects()) Zeal::Game::print_chat("Unable to modify bard effects (spell db change?)");
  } else {
    Zeal::Game::print_chat("Usage: /spelleffects nosprites (toggles mode), /spelleffects bard <0, 1, 2, 3>");
    Zeal::Game::print_chat(
        "nosprites: Disables the minor sprite enhancement of the 180 songs (out of 4000) that can cause a crash"
        " when `/showspelleffects on` is enabled");
    Zeal::Game::print_chat(
        "bard: Sets the effects mode (0 = default, 1, 2, 3 = alternatives) of 14 bard songs to optionally"
        " be more subtle (0 is invisible with /showspelleffects off)");
  }
  return true;
}

// Returns the class / level / monk-epic dependent hand to hand delay in milliseconds.
static int get_hand_to_hand_delay_ms() { return Zeal::Game::get_hand_to_hand_delay() * 100; }

Patches::Patches() {
  const char sit_stand_patch[] = {(char)0xEB, (char)0x1A};
  mem::write(0x42d14d, sit_stand_patch);  // fix pet sit shortcut crash (makes default return of function the sit/stand
                                          // button not sure why its passing in 0)

  // disable client sided hp ticking
  // mem::set(0x4b9141, 0x90, 6);
  SetBrownSkeletons();

  // fix attack delay in DoPassageOfTime() for ItemTypeMartial (0x2d) by replacing unused type 0xd.
  mem::write<BYTE>(0x004c1d97 + 2, 0x2d);  // 004c1d97 80 f9 0d

  // fix hand2hand delay calculation in DoPassageOfTime() for monks and bst
  // replace a load from the fixed 3500 ms in skill dict with a call to our calculation.
  const int h2h_addr = 0x004c1dad;  // 10-byte long (7-byte + 3-byte opcodes) load to EAX sequence.
  unsigned char h2h_patch[10] = {0xe8, 0, 0, 0, 0, 0x90, 0x90, 0x90, 0x90, 0x90};  // call + nops.
  *reinterpret_cast<int *>(&h2h_patch[1]) = reinterpret_cast<int>(&get_hand_to_hand_delay_ms) - (h2h_addr + 5);
  mem::write(h2h_addr, h2h_patch);

  // disable client sided mana ticking
  mem::set(0x4C3F93, 0x90, 7);
  mem::set(0x4C7642, 0x90, 7);

  // disable client sided health ticking
  mem::set(0x4C28B5, 0x90, 9);
  mem::set(0x4C28EF, 0x90, 1);
  mem::set(0x4C28EF + 1, 0xE9, 1);
  mem::set(0x4C298B, 0x90, 2);
  mem::set(0x4C2991, 0x90, 5);
  mem::set(0x4C2BB4, 0x90, 9);

  mem::write<BYTE>(0x40f07a, 0);     // disable character select rotation by default
  mem::write<BYTE>(0x40f07d, 0xEB);  // uncheck rotate button defaultly

  // Replace "Spawning_Your_Characters01" with exact size string.
  const char zeal_patch[27] = "Patching_random_Zeal_crash";
  mem::write<char[27]>(0x005ff96c, zeal_patch);

  // the following does not work entirely needs more effort
  // mem::write<BYTE>(0x4A594B, 15); //load font sizes 1 to 14 (default is 6)
  // mem::write<BYTE>(0X4FDB6A, 15); //allow /chatfontsize to be larger than 5

  mem::write<BYTE>(0x4A14CF,
                   0xEB);  // don't print Your XML files are not compatible with current client files, certain windows
                           // may not perform correctly.  Use "/loadskin Default 1" to load the default game skin.

  ZealService::get_instance()->hooks->Add("GetZoneInfoFromNetwork", 0x53D026, GetZoneInfoFromNetwork, hook_type_detour);

  ZealService::get_instance()->hooks->Add("ProcessDeath", 0x00528E16, ProcessDeath, hook_type_detour);
  ZealService::get_instance()->hooks->Add("CanIBreathe", 0x004C0DAB, CanIBreathe, hook_type_detour);

  ZealService::get_instance()->commands_hook->Add(
      "/spelleffects", {}, "Modify spell effects (prevent crashes, make less flashy, etc).",
      [this](std::vector<std::string> &args) { return HandleSpellEffectsCommand(args); });
}
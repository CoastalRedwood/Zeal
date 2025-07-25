#include "nameplate.h"

#include "game_addresses.h"
#include "string_util.h"
#include "zeal.h"

// Test cases:
// - Command line toggling of options and triggered options menu updates
// - Options menu toggling of options
// - Keybind toggling of options
// - Various states: AFK, LFG, role, anon
// - /showname command 0, 1, 2, 3, 4, 5, 6, 7
// - Options mixed with guilds, raids, and pets
// - Tab cycle targeting updates of text and tint

static float z_position_offset = 1.5f;  // Static global to allow parse overrides during evaluation.

static void ChangeDagStringSprite(Zeal::GameStructures::GAMEDAGINFO *dag, int fontTexture, const char *str) {
  reinterpret_cast<int(__thiscall *)(void *_this_ptr, Zeal::GameStructures::GAMEDAGINFO *dag, int fontTexture,
                                     const char *text)>(0x4B0AA8)(*(void **)0x7F9510, dag, fontTexture, str);
}

static int __fastcall SetNameSpriteTint(void *this_display, void *not_used, Zeal::GameStructures::Entity *entity) {
  if (ZealService::get_instance()->nameplate->handle_SetNameSpriteTint(entity))
    return 1;  // SetNameSpriteTint returns 1 if a tint was applied, 0 if not able to update.

  return ZealService::get_instance()->hooks->hook_map["SetNameSpriteTint"]->original(SetNameSpriteTint)(
      this_display, not_used, entity);
}

static int __fastcall SetNameSpriteState(void *this_display, void *unused_edx, Zeal::GameStructures::Entity *entity,
                                         int show) {
  if (ZealService::get_instance()->nameplate->handle_SetNameSpriteState(this_display, entity, show))
    return 0;  // The callers of SetNameSpriteState do not check the result so just return 0.

  return ZealService::get_instance()->hooks->hook_map["SetNameSpriteState"]->original(SetNameSpriteState)(
      this_display, unused_edx, entity, show);
}

// Promotes a SetNameSpriteTint call to a SetNameSpriteState call (for faster target updates) if visible.
static int __fastcall SetNameSpriteTint_UpdateState(void *this_display, void *not_used,
                                                    Zeal::GameStructures::Entity *entity) {
  bool show = (entity && ((entity->Type == Zeal::GameEnums::Player && Zeal::Game::get_show_pc_names()) ||
                          (entity->Type == Zeal::GameEnums::NPC && Zeal::Game::get_show_npc_names())));
  if (show) {
    SetNameSpriteState(this_display, not_used, entity, show);  // Calls SetNameSpriteTint internally.
    return 1;                                                  // SetNameSpriteTint returns 1 if a tint was applied.
  }
  return SetNameSpriteTint(this_display, not_used, entity);
}

bool NamePlate::handle_shownames_command(const std::vector<std::string> &args) {
  if (!setting_extended_nameplate.get()) return false;

  // No arguments - show our custom help message and suppress original
  if (args.size() < 2) {
    Zeal::Game::print_chat("Format: /shownames <off/1/2/3/4/5/6/7>");
    return true;  // Suppress the original command
  }

  // Check if it's one of our extended cases
  int value;
  if (Zeal::String::tryParse(args[1], &value)) {
    if (value == 5) {
      Zeal::Game::print_chat("Title and first names.");
      return false;  // Let original command run to set the value
    } else if (value == 6) {
      Zeal::Game::print_chat("Title, first, and last names.");
      return false;  // Let original command run to set the value
    } else if (value == 7) {
      Zeal::Game::print_chat("First and guild names.");
      return false;  // Let original command run to set the value
    }
  }

  // For all other cases (off/1/2/3/4), let the original command handle it
  return false;
}

NamePlate::NamePlate(ZealService *zeal) {
  // mem::write<byte>(0x4B0B3D, 0); //arg 2 for SetStringSpriteYonClip (extended nameplate)

  zeal->hooks->Add("SetNameSpriteState", 0x4B0BD9, SetNameSpriteState, hook_type_detour);
  zeal->hooks->Add("SetNameSpriteTint", 0x4B114D, SetNameSpriteTint, hook_type_detour);

  // Replace the tint only updates in RealRender_World with one that also updates the text
  // when there is a change in target. This processing happens shortly after the DoPassageOfTime()
  // processing where it normally happens, but that processing is gated by an update rate.
  zeal->hooks->Add("SetNameSpriteTint_UpdateState", 0x004aaff5, SetNameSpriteTint_UpdateState,
                   hook_type_replace_call);  // Old target - New is null
  zeal->hooks->Add("SetNameSpriteTint_UpdateState", 0x004aafa5, SetNameSpriteTint_UpdateState,
                   hook_type_replace_call);  // Old target - New not null
  zeal->hooks->Add("SetNameSpriteTint_UpdateState", 0x004aafba, SetNameSpriteTint_UpdateState,
                   hook_type_replace_call);  // New target

  zeal->commands_hook->Add("/nameplate", {}, "Toggles nameplate options on/off.",
                           [this](std::vector<std::string> &args) {
                             parse_args(args);
                             return true;
                           });

  zeal->commands_hook->Add("/shownames", {"/show", "/showname"}, "Show names command with extended support.",
                           [this](std::vector<std::string> &args) {
                             return handle_shownames_command(
                                 args);  // Return the result to control suppression, Let the original command run too
                           });

  zeal->callbacks->AddGeneric([this]() { clean_ui(); }, callback_type::InitUI);  // Just release all resources.
  zeal->callbacks->AddGeneric([this]() { clean_ui(); }, callback_type::CleanUI);
  zeal->callbacks->AddGeneric([this]() { clean_ui(); }, callback_type::DXReset);  // Just release all resources.
  zeal->callbacks->AddGeneric([this]() { render_ui(); }, callback_type::RenderUI);

  // Ensure our local entity cache is flushed when an entity despawns.
  zeal->callbacks->AddEntity(
      [this](struct Zeal::GameStructures::Entity *entity) {
        auto it = nameplate_info_map.find(entity);
        if (it != nameplate_info_map.end()) nameplate_info_map.erase(it);
      },
      callback_type::EntityDespawn);
}

NamePlate::~NamePlate() {}

void NamePlate::parse_args(const std::vector<std::string> &args) {
  static std::unordered_map<std::string, ZealSetting<bool> *> command_map = {
      {"colors", &setting_colors},
      {"concolors", &setting_con_colors},
      {"targetcolor", &setting_target_color},
      {"charselect", &setting_char_select},
      {"hideself", &setting_hide_self},
      {"x", &setting_x},
      {"hideraidpets", &setting_hide_raid_pets},
      {"showpetownername", &setting_show_pet_owner_name},
      {"targetmarker", &setting_target_marker},
      {"targethealth", &setting_target_health},
      {"targetblink", &setting_target_blink},
      {"attackonly", &setting_attack_only},
      {"inlineguild", &setting_inline_guild},
      {"healthbars", &setting_health_bars},
      {"manabars", &setting_mana_bars},
      {"staminabars", &setting_stamina_bars},
      {"zealfont", &setting_zeal_fonts},
      {"dropshadow", &setting_drop_shadow},
      {"extendedshownames", &setting_extended_shownames},
  };

  if (args.size() == 2 && args[1] == "dump") {
    if (sprite_font) sprite_font->dump();
    return;
  }
  if (args.size() == 3 && args[1] == "offset") {
    float offset;
    if (Zeal::String::tryParse(args[2], &offset)) {
      z_position_offset = max(-1.f, min(25.f, offset));
      return;
    }
  } else if (args.size() == 3 && args[1] == "shadowfactor") {
    float factor;
    if (Zeal::String::tryParse(args[2], &factor)) {
      factor = max(0.005f, min(0.1f, factor));
      setting_shadow_offset_factor.set(factor);
      if (sprite_font) sprite_font->set_shadow_offset_factor(setting_shadow_offset_factor.get());
      return;
    }
  }

  if (args.size() == 2 && command_map.find(args[1]) != command_map.end()) {
    auto setting = command_map[args[1]];
    setting->toggle();
    Zeal::Game::print_chat("Nameplate option %s set to %s", args[1].c_str(), setting->get() ? "Enabled" : "Disabled");
    if (ZealService::get_instance() && ZealService::get_instance()->ui && ZealService::get_instance()->ui->options)
      ZealService::get_instance()->ui->options->UpdateOptionsNameplate();
    return;
  }

  Zeal::Game::print_chat("Usage: /nameplate option where option is one of");
  Zeal::Game::print_chat("tint:  colors, concolors, targetcolor, targetblink, attackonly, charselect");
  Zeal::Game::print_chat("text:  hideself, x, hideraidpets, showpetownername, targetmarker, targethealth, inlineguild");
  Zeal::Game::print_chat("font:  zealfont, dropshadow, healthbars, manabars, staminabars");
  Zeal::Game::print_chat("misc:  extendedshownames");
  Zeal::Game::print_chat("shadows: /nameplate shadowfactor <float> (0.005 to 0.1 range)");
}

std::vector<std::string> NamePlate::get_available_fonts() const {
  return BitmapFont::get_available_fonts();  // Note that we customize the "default" one.
}

// Loads the sprite font for real-time text rendering to screen.
void NamePlate::load_sprite_font() {
  if (sprite_font || !setting_zeal_fonts.get()) return;

  IDirect3DDevice8 *device = ZealService::get_instance()->dx->GetDevice();
  if (device == nullptr) return;

  std::string font_filename = setting_fontname.get();
  if (font_filename.empty() || font_filename == BitmapFont::kDefaultFontName) font_filename = "arial_bold_24";

  sprite_font = SpriteFont::create_sprite_font(*device, font_filename);
  if (!sprite_font) return;  // Let caller deal with the failure.
  sprite_font->set_drop_shadow(setting_drop_shadow.get());
  sprite_font->set_align_bottom(true);  // Bottom align for multi-line and font sizes.
  sprite_font->set_shadow_offset_factor(setting_shadow_offset_factor.get());
}

void NamePlate::clean_ui() {
  nameplate_info_map.clear();
  sprite_font.reset();  // Relying on spritefont destructor to be invoked to release resources.
}

// Approximation for the client behavior. Exact formula is unknown.
static float get_nameplate_z_offset(const Zeal::GameStructures::Entity &entity) { return z_position_offset; }

// The server currently only sends reliable HP updates for target, self, self pet,
// and group members.  See Mob::SendHPUpdate().
static bool is_hp_updated(const Zeal::GameStructures::Entity *entity) {
  if (!entity) return false;
  if (entity->Type != Zeal::GameEnums::EntityTypes::NPC && entity->Type != Zeal::GameEnums::EntityTypes::Player)
    return false;  // No hp bars on corpses.
  if (entity == Zeal::Game::get_target()) return true;
  const auto self = Zeal::Game::get_self();
  if (entity == self) return true;
  if (self && self->SpawnId && (entity->PetOwnerSpawnId == self->SpawnId)) return true;
  if (Zeal::Game::GroupInfo->is_in_group())
    for (int i = 0; i < GAME_NUM_GROUP_MEMBERS; i++) {
      auto member = Zeal::Game::GroupInfo->EntityList[i];
      if (entity == member) return true;
      if (member && (entity->PetOwnerSpawnId == member->SpawnId)) return true;
    }
  return false;
}

// Returns -1 if mana is not available to display.
static int get_mana_percent(const Zeal::GameStructures::Entity *entity) {
  if (!entity || entity->Type != Zeal::GameEnums::Player || !entity->CharInfo) return -1;

  if (entity == Zeal::Game::get_self()) {
    int mana = entity->CharInfo->mana();
    int max_mana = entity->CharInfo->max_mana();
    return (max_mana > 0) ? max(0, min(100, (mana * 100 / max_mana))) : -1;
  }

  return -1;  // TODO: Support entities besides self.
}

// Returns -1 if stamina is not available to display.
static int get_stamina_percent(const Zeal::GameStructures::Entity *entity) {
  if (!entity || entity->Type != Zeal::GameEnums::Player || !entity->CharInfo) return -1;

  if (entity == Zeal::Game::get_self())
    return max(0, min(100, 100 - entity->CharInfo->Stamina));  // 100 = empty, 0 = full.

  return -1;  // TODO: Support entities besides self.
}

void NamePlate::render_ui() {
  if (!setting_zeal_fonts.get() || Zeal::Game::get_gamestate() != GAMESTATE_INGAME) return;

  if (!sprite_font) load_sprite_font();
  if (!sprite_font) {
    Zeal::Game::print_chat("Nameplate: Failed to load zeal fonts, disabling");
    setting_zeal_fonts.set(false, false);  // Fallback to native nameplates.
    if (ZealService::get_instance() && ZealService::get_instance()->ui && ZealService::get_instance()->ui->options)
      ZealService::get_instance()->ui->options->UpdateOptionsNameplate();
    return;
  }

  // Go through world visible list.
  const float kMaxDist = 400;  // Quick testing of client extended nameplates was ~ 375.
  auto visible_entities = Zeal::Game::get_world_visible_actor_list(kMaxDist, false);
  auto self = Zeal::Game::get_self();
  if (self && *Zeal::Game::camera_view != Zeal::GameEnums::CameraView::FirstPerson &&
      !Zeal::Game::GameInternal::is_invisible(Zeal::Game::Display, 0, self, self))
    visible_entities.push_back(self);  // Add self nameplate.

  std::vector<RenderInfo> render_list;
  for (const auto &entity : visible_entities) {
    // Added Unknown0003 check due to some bad results with 0x05 at startup causing a crash.
    if (!entity || entity->StructType != 0x03 || !entity->ActorInfo || !entity->ActorInfo->DagHeadPoint ||
        !entity->ActorInfo->ViewActor_ ||
        entity->ActorInfo->ViewActor_->Flags & 0x20000000)  // Empirically found flag for incomplete actor.
      continue;
    auto it = nameplate_info_map.find(entity);
    if (it == nameplate_info_map.end()) continue;

    NamePlateInfo &info = it->second;
    Vec3 position = entity->ActorInfo->DagHeadPoint->Position;
    position.z += get_nameplate_z_offset(*entity);

    // Support an optional healthbar for zeal font mode only.
    std::string full_text = info.text;
    if (setting_health_bars.get() && is_hp_updated(entity)) {
      const char healthbar[4] = {'\n', BitmapFontBase::kStatsBarBackground, BitmapFontBase::kHealthBarValue, 0};
      full_text += healthbar;
      int hp_percent = entity->HpCurrent;  // NPC value is stored as a percent.
      if (entity->Type == Zeal::GameEnums::EntityTypes::Player)
        hp_percent = (entity->HpCurrent > 0 && entity->HpMax > 0) ? (entity->HpCurrent * 100) / entity->HpMax : 0;
      sprite_font->set_hp_percent(hp_percent);
    }
    int mana_percent = setting_mana_bars.get() ? get_mana_percent(entity) : -1;
    if (mana_percent >= 0) {
      const char manabar[4] = {'\n', BitmapFontBase::kStatsBarBackground, BitmapFontBase::kManaBarValue, 0};
      full_text += manabar;
      sprite_font->set_mana_percent(mana_percent);
    }
    int stamina_percent = setting_stamina_bars.get() ? get_stamina_percent(entity) : -1;
    if (stamina_percent >= 0) {
      const char staminabar[4] = {'\n', BitmapFontBase::kStatsBarBackground, BitmapFontBase::kStaminaBarValue, 0};
      full_text += staminabar;
      sprite_font->set_stamina_percent(stamina_percent);
    }

    sprite_font->queue_string(full_text.c_str(), position, true, info.color | 0xff000000);
  }
  sprite_font->flush_queue_to_screen();
}

bool NamePlate::is_group_member(const Zeal::GameStructures::Entity &entity) const {
  for (int i = 0; i < GAME_NUM_GROUP_MEMBERS; i++) {
    Zeal::GameStructures::Entity *groupmember = Zeal::Game::GroupInfo->EntityList[i];
    if (groupmember && &entity == groupmember) {
      return true;
    }
  }
  return false;
}

bool NamePlate::is_raid_member(const Zeal::GameStructures::Entity &entity) const {
  const char *spawn_name = entity.Name;
  for (int i = 0; i < Zeal::GameStructures::RaidInfo::kRaidMaxMembers; ++i) {
    const Zeal::GameStructures::RaidMember &member = Zeal::Game::RaidInfo->MemberList[i];
    if ((strlen(member.Name) != 0) && (strcmp(member.Name, entity.Name) == 0)) {
      return true;
    }
  }
  return false;
}

// Helper function for selecting the player color.
NamePlate::ColorIndex NamePlate::get_player_color_index(const Zeal::GameStructures::Entity &entity) const {
  if (entity.IsPlayerKill == 1) {
    // Your color is always red/pvp
    if (&entity == Zeal::Game::get_self()) return ColorIndex::PVP;
    // Friendly PVP Targets
    if (entity.GuildId != -1 && entity.GuildId == Zeal::Game::get_self()->GuildId) return ColorIndex::PvpAlly;
    if (Zeal::Game::GroupInfo->is_in_group() && is_group_member(entity)) return ColorIndex::PvpAlly;
    if (Zeal::Game::RaidInfo->is_in_raid() && is_raid_member(entity)) return ColorIndex::PvpAlly;
    // Enemy PVP Target
    return ColorIndex::PVP;
  }

  if (entity.IsAwayFromKeyboard == 1) return ColorIndex::AFK;

  if (entity.IsLinkDead == 1) return ColorIndex::LD;

  if (entity.ActorInfo && entity.ActorInfo->IsLookingForGroup) {
    if (entity.GuildId && (entity.GuildId == Zeal::Game::get_self()->GuildId)) return ColorIndex::GuildLFG;
    return ColorIndex::LFG;
  }

  if (Zeal::Game::GroupInfo->is_in_group()) {
    if (&entity == Zeal::Game::get_self()) return ColorIndex::Group;

    if (is_group_member(entity)) return ColorIndex::Group;
  }

  if (Zeal::Game::RaidInfo->is_in_raid()) {
    if (is_raid_member(entity)) return ColorIndex::Raid;
  }

  if (entity.AnonymousState == 2)  // Roleplay
    return ColorIndex::Role;

  if (entity.GuildId == -1)  // Not in a guild.
    return ColorIndex::Adventurer;

  return (entity.GuildId == Zeal::Game::get_self()->GuildId) ? ColorIndex::MyGuild : ColorIndex::OtherGuild;
}

// Internal helper for retrieving the color for a pet (for raid or group).
NamePlate::ColorIndex NamePlate::get_pet_color_index(const Zeal::GameStructures::Entity &entity) const {
  if (entity.PetOwnerSpawnId == Zeal::Game::get_self()->SpawnId)  // Self Pet
    return ColorIndex::Group;                                     // Always a group member.

  if (Zeal::Game::GroupInfo->is_in_group()) {
    for (int i = 0; i < GAME_NUM_GROUP_MEMBERS; i++) {
      Zeal::GameStructures::Entity *groupmember = Zeal::Game::GroupInfo->EntityList[i];
      if (groupmember && entity.PetOwnerSpawnId == groupmember->SpawnId) return ColorIndex::Group;
    }
  }

  if (Zeal::Game::is_raid_pet(entity)) return ColorIndex::Raid;

  return ColorIndex::UseClient;
}

NamePlate::ColorIndex NamePlate::get_color_index(const Zeal::GameStructures::Entity &entity) {
  if (!entity.ActorInfo) return ColorIndex::UseClient;

  // Special handling for character select.
  if (Zeal::Game::get_gamestate() == GAMESTATE_CHARSELECT)
    return setting_char_select.get() ? ColorIndex::Adventurer : ColorIndex::UseClient;

  // Target setting overrides all other choices.
  if (&entity == Zeal::Game::get_target() && setting_target_color.get()) return ColorIndex::Target;

  // Otherwise tint based on entity Type and other properties.
  auto color_index = ColorIndex::UseClient;
  switch (entity.Type) {
    case Zeal::GameEnums::EntityTypes::Player:
      if (setting_colors.get())
        color_index = get_player_color_index(entity);
      else if (setting_con_colors.get() && &entity != Zeal::Game::get_self())
        color_index = ColorIndex::UseConsider;
      break;
    case Zeal::GameEnums::EntityTypes::NPC:
      if (setting_colors.get() && entity.PetOwnerSpawnId) color_index = get_pet_color_index(entity);
      if (color_index == ColorIndex::UseClient && setting_con_colors.get()) color_index = ColorIndex::UseConsider;
      break;
    case Zeal::GameEnums::EntityTypes::NPCCorpse:
      if (setting_colors.get()) color_index = ColorIndex::NpcCorpse;
      break;
    case Zeal::GameEnums::EntityTypes::PlayerCorpse:
      if (setting_colors.get()) color_index = ColorIndex::PlayerCorpse;
      break;
    default:
      break;
  }

  return color_index;
}

bool NamePlate::handle_SetNameSpriteTint(Zeal::GameStructures::Entity *entity) {
  if (!entity || !entity->ActorInfo || !entity->ActorInfo->DagHeadPoint) return false;

  auto color_index = get_color_index(*entity);

  bool zeal_fonts = setting_zeal_fonts.get() && Zeal::Game::get_gamestate() != GAMESTATE_CHARSELECT;
  if (color_index == ColorIndex::UseClient && !zeal_fonts) return false;

  auto color = D3DCOLOR_XRGB(128, 255, 255);  // Approximately the default nameplate color.
  if (color_index == ColorIndex::UseConsider)
    color = Zeal::Game::GetLevelCon(entity);
  else if (color_index != ColorIndex::UseClient)
    color = ZealService::get_instance()->ui->options->GetColor(static_cast<int>(color_index));

  if (entity == Zeal::Game::get_target() && setting_target_blink.get()) {
    // Share the flash speed slider with the target_ring so they aren't beating.
    float flash_speed = ZealService::get_instance()->target_ring->flash_speed.get();
    float fade_factor = Zeal::Game::get_target_blink_fade_factor(flash_speed, setting_attack_only.get());
    if (fade_factor < 1.0f) {
      BYTE faded_red = static_cast<BYTE>(((color >> 16) & 0xFF) * fade_factor);
      BYTE faded_green = static_cast<BYTE>(((color >> 8) & 0xFF) * fade_factor);
      BYTE faded_blue = static_cast<BYTE>((color & 0xFF) * fade_factor);
      color = D3DCOLOR_ARGB(0xff, faded_red, faded_green, faded_blue);
    }
  }

  if (zeal_fonts) {
    auto it = nameplate_info_map.find(entity);
    if (it != nameplate_info_map.end()) it->second.color = color;
    return true;
  }

  if (!entity->ActorInfo->DagHeadPoint->StringSprite || entity->ActorInfo->DagHeadPoint->StringSprite->MagicValue !=
                                                            Zeal::GameStructures::GAMESTRINGSPRITE::kMagicValidValue)
    return false;
  entity->ActorInfo->DagHeadPoint->StringSprite->Color = color;
  return true;
}

// Implements the racial specific hidden nameplates. Logic copied from the client disassembly.
bool NamePlate::is_nameplate_hidden_by_race(const Zeal::GameStructures::Entity &entity) const {
  if (entity.Type == Zeal::GameEnums::Player)  // Never hide the player by race.
    return false;

  // Zeal modification: Never hide corpse nameplates based on race.
  if (entity.Type == Zeal::GameEnums::NPCCorpse || entity.Type == Zeal::GameEnums::PlayerCorpse) return false;

  // Zeal modification: Never hide the current target nameplate (ie, skelly on the ground).
  if (&entity == Zeal::Game::get_target()) return false;

  if (entity.Race == 0xf4)  // 0xf4 = "Ent"
    return true;

  auto animation = entity.ActorInfo->Animation;
  if (entity.Race == 0x3c)  // 0x3c = "Skeleton"
    return !entity.PetOwnerSpawnId && (animation == 0x10 || animation == 0x21 || animation == 0x26);

  // 0x1d = "Gargoyle", 0x34 = "Mimic", 0x118 = "Nightmare Gargoyle"
  if (entity.Race == 0x1d || entity.Race == 0x34 || entity.Race == 0x118)
    return (animation == 0x21 || animation == 0x26);

  return false;
}

// Handles the target health and target markers at the end of the name.
std::string NamePlate::generate_target_postamble(const Zeal::GameStructures::Entity &entity) const {
  std::string text;

  if (setting_target_health.get()) {
    if (entity.Type == Zeal::GameEnums::NPC) {
      int hp_percent = entity.HpCurrent;  // NPC value is stored as a percent.
      text += std::format(" {}%", hp_percent);
    } else if (entity.Type == Zeal::GameEnums::Player && entity.HpCurrent > 0 && entity.HpMax > 0) {
      int hp_percent = (entity.HpCurrent * 100) / entity.HpMax;  // Calculate % health.
      text += std::format(" {}%", hp_percent);
    }
  }
  if (setting_target_marker.get()) text += "<<";
  return text;
}

// Duplicates most of the client logic in SetNameSpriteState except for the unused return values.
std::string NamePlate::generate_nameplate_text(const Zeal::GameStructures::Entity &entity, int show) const {
  // Handle some of the always disabled nameplates.
  if (is_nameplate_hidden_by_race(entity)) return std::string();  // Returns empty text.

  // Handle character select formatted output when active.
  if (Zeal::Game::is_new_ui() && Zeal::Game::Windows->CharacterSelect &&
      Zeal::Game::Windows->CharacterSelect->Activated) {
    return std::format("{} [{} {}]\n{}", entity.Name, entity.Level, Zeal::Game::get_class_desc(entity.Class),
                       Zeal::Game::get_full_zone_name(entity.ZoneId));
  }

  // Handle client decision to explicitly not show the nameplate.
  if (show == 0) return std::string();

  const uint32_t show_name = Zeal::Game::get_showname();
  const bool is_self = (&entity == Zeal::Game::get_self());
  const bool is_target = (&entity == Zeal::Game::get_target());
  if (!is_target &&
      ((is_self && setting_hide_self.get()) || (setting_hide_raid_pets.get() && Zeal::Game::is_raid_pet(entity))))
    return std::string();

  if (is_self && setting_x.get()) return std::string((entity.IsHidden == 0x01) ? "(X)" : "X");

  if (entity.Race >= 0x8cd)  // Some sort of magic higher level races w/out name trimming.
    return std::string(entity.Name);

  // Helper functions for new showname levels
  auto should_show_title = [](uint32_t show_name) -> bool {
    return (show_name == 4) || (show_name == 5) || (show_name == 6);
  };

  auto should_show_last_name = [](uint32_t show_name) -> bool {
    return (show_name == 2) || (show_name == 3) || (show_name == 4) || (show_name == 6);
  };

  auto should_show_guild = [](uint32_t show_name) -> bool {
    return (show_name == 3) || (show_name == 4) || (show_name == 7);
  };

  std::string text;
  if (is_target && setting_target_marker.get()) text += ">>";

  // Handle the simpler NPC and corpses first.
  if (entity.Type != Zeal::GameEnums::Player) {
    text += std::string(Zeal::Game::trim_name(entity.Name));
    if (is_target) text += generate_target_postamble(entity);
    if (setting_show_pet_owner_name.get() && Zeal::Game::is_player_pet(entity)) {
      auto pet_owner = Zeal::Game::get_entity_by_id(entity.PetOwnerSpawnId);
      if (pet_owner && pet_owner != Zeal::Game::get_self())
        text += std::format("\n({}'s Pet)", Zeal::Game::trim_name(pet_owner->Name));
      else
        text += "\n(Pet)";  // Self-pet or missing owner.
    }
    return text;
  }

  if (entity.ActorInfo->IsTrader == 1 && Zeal::Game::get_self() && Zeal::Game::get_self()->ZoneId == 0x97)
    text += "Trader ";  // String id 0x157f.

  else if (entity.AlternateAdvancementRank > 0 && entity.Gender != 2 && should_show_title(show_name)) {
    text += Zeal::Game::get_title_desc(entity.Class, entity.AlternateAdvancementRank, entity.Gender);
    text += " ";
  }

  // Finally work on the primary player name with embellishments.
  if (entity.IsHidden == 0x01)  // Client code only does () on normal invisibility.
    text += "(";
  text += Zeal::Game::trim_name(entity.Name);
  if (entity.IsHidden == 0x01) text += ")";

  if (should_show_last_name(show_name) && entity.LastName[0]) {
    text += " ";
    text += Zeal::Game::trim_name(entity.LastName);
  }

  const bool is_anonymous = (entity.AnonymousState == 1) ? true : false;
  const bool show_guild = !is_anonymous && should_show_guild(show_name) && entity.GuildId != -1;
  const bool show_guild_newline = Zeal::Game::is_new_ui() && !setting_inline_guild.get();
  if (show_guild && !show_guild_newline)
    text += std::format(" <{}>", Zeal::Game::get_player_guild_name(entity.GuildId));

  if (entity.ActorInfo->IsLookingForGroup) text += " LFG";  // String id 0x301a.
  if (!entity.IsPlayerKill) {
    if (entity.IsAwayFromKeyboard) text += " AFK";  // String id 0x3017.
    if (entity.IsLinkDead) text += " LD";           // String id 0x8c0.
  }
  if (is_target) text += generate_target_postamble(entity);
  if (show_guild && show_guild_newline)
    text += std::format("\n<{}>", Zeal::Game::get_player_guild_name(entity.GuildId));

  return text;
}

// Returns true if it updated the nameplate state. False if the default code needs to run.
bool NamePlate::handle_SetNameSpriteState(void *this_display, Zeal::GameStructures::Entity *entity, int show) {
  // Note: The this_display pointer should be equal to *Zeal::Game::Display.
  if (!entity || !entity->ActorInfo || !entity->ActorInfo->DagHeadPoint)
    return false;  // Note: Possibly change to true to avoid client handler from running.

  // Let the default client path handle things when the world display isn't active. That code
  // will handle any needed deallocations.
  int world_display_started = *(int *)((int)this_display + 0x2ca4);  // Set in CDisplay::StartWorldDisplay().
  int font_texture = *(int *)((int)this_display + 0x2e08);
  if (!world_display_started || !font_texture) return false;

  std::string text = generate_nameplate_text(*entity, show);
  const char *string_sprite_text = text.c_str();
  if (setting_zeal_fonts.get() && Zeal::Game::get_gamestate() != GAMESTATE_CHARSELECT) {
    if (!text.empty())
      nameplate_info_map[entity] = {.text = text, .color = D3DCOLOR_XRGB(255, 255, 255)};
    else {
      auto it = nameplate_info_map.find(entity);
      if (it != nameplate_info_map.end()) nameplate_info_map.erase(it);
    }
    string_sprite_text = nullptr;  // This disables the client's sprite in call below.
  }

  ChangeDagStringSprite(entity->ActorInfo->DagHeadPoint, font_texture, string_sprite_text);

  if (!text.empty()) SetNameSpriteTint(this_display, nullptr, entity);
  return true;
}

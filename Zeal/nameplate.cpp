#include "nameplate.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "callbacks.h"
#include "chat.h"
#include "chatfilter.h"
#include "commands.h"
#include "game_addresses.h"
#include "game_ui.h"
#include "hook_wrapper.h"
#include "string_util.h"
#include "tag_arrows.h"
#include "target_ring.h"
#include "zeal.h"

// Test cases:
// - Command line toggling of options and triggered options menu updates
// - Options menu toggling of options
// - Keybind toggling of options
// - Various states: AFK, LFG, role, anon
// - /showname command 0, 1, 2, 3, 4, 5, 6, 7
// - Options mixed with guilds, raids, and pets
// - Tab cycle targeting updates of text and tint

static constexpr char kZealTagHeader[] = "ZEALTAG";
static constexpr char kZealTagHeaderAbbreviated[] = "ZT";  // Possible migration path; add support now.
static constexpr char kDelimiter[] = " | ";
static constexpr char kZealTagChannelPrefix[] = "Zt";  // Chat channels only allow first character capitalization.
static constexpr char kZealTagChannelBroadcastPrefix[] = "ChatChannel: ";
static constexpr int kTagChannelJoinPending = -2;

enum TagArrowColor : DWORD {
  Off = 0,  // Alpha == 0 for these special cases.
  Nameplate = 1,
  Paw = D3DCOLOR_XRGB(0x20, 0xc0, 0x20),       // Ensure this is unique.
  StopSign = D3DCOLOR_XRGB(0xf0, 0x00, 0x00),  // Ensure this is unique.
  Red = D3DCOLOR_XRGB(0xff, 0, 0),
  Orange = D3DCOLOR_XRGB(0xff, 0x80, 0),
  Yellow = D3DCOLOR_XRGB(0xff, 0xff, 0),
  Green = D3DCOLOR_XRGB(0, 0xff, 0),
  Blue = D3DCOLOR_XRGB(0, 0, 0xff),
  White = D3DCOLOR_XRGB(0xff, 0xff, 0xff),
};

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

// Handles the nameplate update call in the entity destructor.
static int __fastcall SetNameSpriteState_Destructor(void *this_display, void *unused_edx,
                                                    Zeal::GameStructures::Entity *entity, int show) {
  ZealService::get_instance()->nameplate->handle_entity_destructor(entity);

  // Bypass the unnecessary call to our own setnamesprite handler and reroute directly to the client's.
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

// Flushes any deleted entities in the info_map cache.
void NamePlate::handle_entity_destructor(Zeal::GameStructures::Entity *entity) {
  auto it = nameplate_info_map.find(entity);
  if (it != nameplate_info_map.end()) nameplate_info_map.erase(it);
}

bool NamePlate::handle_shownames_command(const std::vector<std::string> &args) {
  if (!setting_extended_nameplate.get()) return false;

  if (args.size() <= 1) {
    Zeal::Game::print_chat("Format: /shownames <off/1/2/3/4/5/6/7>");
    return true;  // Suppress original command so only showing new usage above.
  }

  int value = -1;
  if (!Zeal::String::tryParse(args[1], &value, true) || (value < 1) || (value > 7))
    value = (args[1].starts_with("off")) ? 0 : 4;  // Show all is the default except for "off".

  // Add some confirmation text missing for the extended nameplates.
  if (value == 5)
    Zeal::Game::print_chat("Showing title and first names.");
  else if (value == 6)
    Zeal::Game::print_chat("Showing title, first, and last names.");
  else if (value == 7)
    Zeal::Game::print_chat("Showing first and guild names.");

  // Keep the UI options in sync. Immediately write to some globals now that the original command will perform
  // later so the update options call below works correctly. The original command will update the Show PC
  // Names
  *reinterpret_cast<int32_t *>(0x007d01e4) = value;   // Update the current shownames level.
  *reinterpret_cast<int *>(0x00798af4) = value != 0;  // Update the depressed button Show PC Names button state.
  if (update_options_ui_callback) update_options_ui_callback();

  return false;  // Let the original command run fully update (beyond shortcuts above).
}

// Support adding text_tag as a tooltip of the target window.
int __fastcall TargetWnd_PostDraw(Zeal::GameUI::SidlWnd *this_ptr, void *not_used) {
  ZealService::get_instance()->nameplate->handle_targetwnd_postdraw(this_ptr);
  return 0;  // The original just returns zero. Skip for efficiency.
}

NamePlate::NamePlate(ZealService *zeal) {
  // mem::write<byte>(0x4B0B3D, 0); //arg 2 for SetStringSpriteYonClip (extended nameplate)

  zeal->hooks->Add("SetNameSpriteState", 0x4B0BD9, SetNameSpriteState, hook_type_detour);
  zeal->hooks->Add("SetNameSpriteTint", 0x4B114D, SetNameSpriteTint, hook_type_detour);
  zeal->hooks->Add("TargetWnd_PostDraw", 0x005e6f78, TargetWnd_PostDraw, hook_type_vtable);

  // Intercept the call within the entity destructor to properly flush the nameplate_info_map cache.
  zeal->hooks->Add("SetNameSpriteState_Destructor", 0x0050724c, SetNameSpriteState_Destructor, hook_type_replace_call);

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
                             if (update_options_ui_callback) update_options_ui_callback();
                             return true;
                           });

  zeal->commands_hook->Add("/shownames", {"/show", "/showname"}, "Show names command with extended support.",
                           [this](std::vector<std::string> &args) {
                             return handle_shownames_command(
                                 args);  // Return the result to control suppression, Let the original command run too
                           });

  zeal->commands_hook->Add("/tag", {}, "Tag currently targeted nameplate with a string and shape",
                           [this](std::vector<std::string> &args) {
                             handle_tag_command(args);
                             if (update_options_ui_callback) update_options_ui_callback();
                             return true;
                           });
  zeal->chat_hook->add_incoming_gsay_callback([this](const char *msg) { handle_tag_message(msg); });
  zeal->chat_hook->add_incoming_rsay_callback([this](const char *msg) { handle_tag_message(msg); });
  zeal->chat_hook->add_incoming_chat_callback(
      [this](const char *msg, int color_index) { return check_for_tag_channel_message(msg, color_index); });

  zeal->chatfilter_hook->AddZealSpamFilterCallback(
      [this](short &channel, std::string &msg) { return handle_zeal_spam_filter(channel, msg); });

  zeal->callbacks->AddGeneric([this]() { clean_ui(); }, callback_type::InitUI);  // Just release all resources.
  zeal->callbacks->AddGeneric([this]() { clean_ui(); }, callback_type::CleanUI);
  zeal->callbacks->AddGeneric([this]() { clean_ui(); }, callback_type::InitCharSelectUI);
  zeal->callbacks->AddGeneric([this]() { clean_ui(); }, callback_type::CleanCharSelectUI);
  zeal->callbacks->AddGeneric([this]() { clean_ui(); }, callback_type::DXReset);  // Just release all resources.
  zeal->callbacks->AddGeneric([this]() { clean_ui(); }, callback_type::DXCleanDevice);
  zeal->callbacks->AddGeneric([this]() { render_ui(); }, callback_type::RenderUI);
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
    if (tag_arrows) tag_arrows->Dump();
    dump();
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
    if (update_options_ui_callback) update_options_ui_callback();
    return;
  }

  Zeal::Game::print_chat("Usage: /nameplate option where option is one of");
  Zeal::Game::print_chat("tint:  colors, concolors, targetcolor, targetblink, attackonly, charselect");
  Zeal::Game::print_chat("text:  hideself, x, hideraidpets, showpetownername, targetmarker, targethealth, inlineguild");
  Zeal::Game::print_chat("font:  zealfont, dropshadow, healthbars, manabars, staminabars");
  Zeal::Game::print_chat("misc:  extendedshownames");
  Zeal::Game::print_chat("shadows: /nameplate shadowfactor <float> (0.005 to 0.1 range)");
}

void NamePlate::dump() const {
  int valid_count = 0;
  for (const auto &[entity, info] : nameplate_info_map) {
    Zeal::GameStructures::Entity *current_ent = Zeal::Game::get_entity_list();
    while (current_ent && current_ent != entity) current_ent = current_ent->Next;
    if (current_ent) valid_count++;
  }
  Zeal::Game::print_chat("Info_map: %d entries (%d valid)", nameplate_info_map.size(), valid_count);
  Zeal::Game::print_chat("Tag channel: %s, number: %d", setting_tag_channel.get().c_str(), tag_channel_number);
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

  // Tag text with zeal fonts and arrows are linked together so just create it here as well.
  if (!tag_arrows) tag_arrows = std::make_unique<TagArrows>(*device);
}

void NamePlate::clean_ui() {
  nameplate_info_map.clear();
  sprite_font.reset();      // Relying on spritefont destructor to be invoked to release resources.
  tag_arrows.reset();       // Also relying on destructor to release resources.
  tag_channel_number = -1;  // Force a reset.
}

// Approximation for the client behavior. Exact formula is unknown.
static float get_nameplate_z_offset(const Zeal::GameStructures::Entity &entity) { return z_position_offset; }

// The server currently only sends reliable HP updates for target, self, self pet,
// and group members unless a quarm specific server rule is set to also send raid members.
// See Mob::SendHPUpdate().
bool NamePlate::is_hp_updated(const Zeal::GameStructures::Entity *entity) const {
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
  if (entity->Type == Zeal::GameEnums::EntityTypes::Player && setting_raid_health_bars.get() &&
      Zeal::Game::RaidInfo->is_in_raid() && is_raid_member(*entity))
    return true;

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

// Returns bearing to target from self from -pi to +pi in the world coordinate system.
static float get_bearing(const Zeal::GameStructures::Entity *self, const Zeal::GameStructures::Entity *target) {
  if (!self || !target) return 0;
  float delta_y = target->Position.x - self->Position.x;
  float delta_x = target->Position.y - self->Position.y;
  delta_x = (delta_x >= 0) ? (max(delta_x, 1e-6)) : (min(delta_x, -1e-6));
  float bearing = std::atan2(delta_y, delta_x);  // From -pi to +pi.
  return bearing;
}

void NamePlate::render_ui() {
  if (!setting_zeal_fonts.get() || (!Zeal::Game::is_in_game() && !Zeal::Game::is_in_char_select())) return;

  if (!sprite_font) load_sprite_font();
  if (!sprite_font) {
    Zeal::Game::print_chat("Nameplate: Failed to load zeal fonts, disabling");
    setting_zeal_fonts.set(false, false);  // Fallback to native nameplates.
    if (update_options_ui_callback) update_options_ui_callback();
    return;
  }

  // Go through world visible list.
  const float kMaxDist = 400;  // Quick testing of client extended nameplates was ~ 375.
  auto visible_entities = Zeal::Game::get_world_visible_actor_list(kMaxDist, false);
  auto self = Zeal::Game::get_self();
  if (self && *Zeal::Game::camera_view != Zeal::GameEnums::CameraView::FirstPerson && Zeal::Game::is_targetable(self))
    visible_entities.push_back(self);  // Add self nameplate.

  std::vector<RenderInfo> render_list;
  for (const auto &entity : visible_entities) {
    // Added Unknown0003 check due to some bad results with 0x05 at startup causing a crash.
    if (!entity || entity->StructType != 0x03 || !entity->ActorInfo || !entity->ActorInfo->DagHeadPoint ||
        !entity->ActorInfo->ViewActor_ ||
        ((entity->ActorInfo->ViewActor_->Flags & 0x20000000)  // Empirically found flag for incomplete actor.
         && !entity->ActorInfo->Mount))  // Empirically found flag is unreliable when mounted (but always a player).
      continue;
    auto it = nameplate_info_map.find(entity);
    if (it == nameplate_info_map.end()) continue;

    NamePlateInfo &info = it->second;
    Vec3 position = entity->ActorInfo->DagHeadPoint->Position;
    position.z += get_nameplate_z_offset(*entity);

    // Support optional tag text and healthbar for zeal font mode only.
    bool is_corpse = entity->Type >= Zeal::GameEnums::NPCCorpse;
    bool add_text_tag = !is_corpse && !info.tag_text.empty();
    std::string full_text = add_text_tag ? info.tag_text + info.text : info.text;
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

    auto nameplate_color = info.color | 0xff000000;
    if (!full_text.empty()) sprite_font->queue_string(full_text.c_str(), position, true, nameplate_color);

    // If an explicit tag color was set, use that color otherwise use the nameplate color.
    if (!is_corpse && info.tag_color != TagArrowColor::Off) {
      auto tag_color = (info.tag_color == TagArrowColor::Nameplate) ? nameplate_color : info.tag_color;
      TagArrows::Shape shape = (tag_color == TagArrowColor::Paw)        ? TagArrows::Shape::Paw
                               : (tag_color == TagArrowColor::StopSign) ? TagArrows::Shape::Octagon
                                                                        : TagArrows::Shape::Arrow;
      position.z += sprite_font->get_text_height(full_text) + 1.5f;
      float bearing = (shape == TagArrows::Shape::Arrow) ? 0.0f : get_bearing(self, entity);
      tag_arrows->QueueTagShape(position, tag_color, shape, bearing);
    }
  }
  tag_arrows->FlushQueueToScreen();
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

  auto owner = Zeal::Game::get_entity_by_id(entity.PetOwnerSpawnId);
  if (!owner || owner->Type != Zeal::GameEnums::Player) return ColorIndex::UseClient;

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
  if (Zeal::Game::is_in_char_select())
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

  bool zeal_fonts = setting_zeal_fonts.get() && !Zeal::Game::is_in_char_select();
  if (color_index == ColorIndex::UseClient && !zeal_fonts) return false;

  bool is_target = (entity == Zeal::Game::get_target());
  bool is_corpse = (entity->Type >= Zeal::GameEnums::NPCCorpse);
  auto it = zeal_fonts ? nameplate_info_map.find(entity) : nameplate_info_map.end();
  if (!is_target && !is_corpse && it != nameplate_info_map.end() && !it->second.tag_text.empty() &&
      (it->second.tag_color == TagArrowColor::Off || it->second.tag_color == TagArrowColor::Nameplate))
    color_index = ColorIndex::Tagged;

  auto color = D3DCOLOR_XRGB(128, 255, 255);  // Approximately the default nameplate color.
  if (color_index == ColorIndex::UseConsider)
    color = Zeal::Game::GetLevelCon(entity);
  else if (color_index != ColorIndex::UseClient && ZealService::get_instance()->ui && get_color_callback)
    color = get_color_callback(static_cast<int>(color_index));

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

  if (is_self && setting_x.get()) return std::string((entity.VisibilityState == 0x01) ? "(X)" : "X");

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
    int display_rank = entity.AlternateAdvancementRank;

    // For self, apply local AA title choice
    if (&entity == Zeal::Game::get_self())
      display_rank = min(entity.AlternateAdvancementRank, setting_local_aa_title.get());

    if (display_rank > 0) {
      text += Zeal::Game::get_title_desc(entity.Class, display_rank, entity.Gender);
      text += " ";
    }
  }

  // Finally work on the primary player name with embellishments.
  if (entity.VisibilityState == 0x01)  // Client code only does () on normal invisibility.
    text += "(";
  text += Zeal::Game::trim_name(entity.Name);
  if (entity.VisibilityState == 0x01) text += ")";

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
  // Note: The this_display pointer should be equal to Zeal::Game::get_display().
  if (!entity || !entity->ActorInfo || !entity->ActorInfo->DagHeadPoint)
    return false;  // Note: Possibly change to true to avoid client handler from running.

  // Let the default client path handle things when the world display isn't active. That code
  // will handle any needed deallocations.
  int world_display_started = *(int *)((int)this_display + 0x2ca4);  // Set in CDisplay::StartWorldDisplay().
  int font_texture = *(int *)((int)this_display + 0x2e08);
  if (!world_display_started || !font_texture) return false;

  std::string text = generate_nameplate_text(*entity, show);
  const char *string_sprite_text = text.c_str();
  if (setting_zeal_fonts.get() &&
      (Zeal::Game::is_in_game() || (setting_char_select.get() && Zeal::Game::is_in_char_select()))) {
    auto it = nameplate_info_map.find(entity);
    auto color = Zeal::Game::is_in_char_select() ? D3DCOLOR_XRGB(0xf0, 0xf0, 0x00) : D3DCOLOR_XRGB(0xff, 0xff, 0xff);
    if (it == nameplate_info_map.end()) {
      nameplate_info_map[entity] = {.text = text, .tag_text = "", .color = color, .tag_color = TagArrowColor::Off};
    } else {  // Already exists, so leave tag_text and tag_color untouched.
      it->second.text = text;
      it->second.color = color;
    }
    string_sprite_text = nullptr;  // This disables the client's sprite in call below.
  }

  ChangeDagStringSprite(entity->ActorInfo->DagHeadPoint, font_texture, string_sprite_text);

  SetNameSpriteTint(this_display, nullptr, entity);
  return true;
}

void NamePlate::enable_tags(bool enable) {
  if (enable && !setting_zeal_fonts.get()) {
    Zeal::Game::print_chat("Nameplate tags requires Zeal fonts to be enabled");
    enable = false;
  }
  setting_tag_enable.set(enable);
  Zeal::Game::print_chat("Nameplate tags %s", enable ? "enabled" : "disabled");
  if (!enable) clear_tags();
}

void NamePlate::clear_tags() {
  for (auto &pair : nameplate_info_map) {
    pair.second.tag_text = "";
    pair.second.tag_color = TagArrowColor::Off;
  }
}

static constexpr int kMaxTagTextLength = 32;

// This may not be 100% correct in terms of visible nameplates but should be fairly good.
static bool is_taggable_target(const Zeal::GameStructures::Entity *target) {
  if (!target) return false;

  if (target->Type != Zeal::GameEnums::EntityTypes::Player && target->Type != Zeal::GameEnums::EntityTypes::NPC)
    return false;

  if (!target->ActorInfo || !target->ActorInfo->ViewActor_ || !target->ActorInfo->DagHeadPoint) return false;

  return true;
}

// Perform a quick horizontal distance comparison for the target sorting.
static bool distance_comparison(const Zeal::GameStructures::Entity *a, const Zeal::GameStructures::Entity *b) {
  auto self = Zeal::Game::get_self();
  if (!self || !a || !b) return true;  // Default to no change in order.

  float distance_a = (a->Position.x - self->Position.x) * (a->Position.x - self->Position.x) +
                     (a->Position.y - self->Position.y) * (a->Position.y - self->Position.y);
  float distance_b = (b->Position.x - self->Position.x) * (b->Position.x - self->Position.x) +
                     (b->Position.y - self->Position.y) * (b->Position.y - self->Position.y);
  return distance_a <= distance_b;  // No reason to do the sqrt().
}

bool NamePlate::handle_tag_target(const std::string &target_text) {
  // Scan all nameplates for tag_text that contains the target text.
  std::vector<Zeal::GameStructures::Entity *> matches;
  for (const auto &entry : nameplate_info_map) {
    const auto &tag_text = entry.second.tag_text;
    if (!entry.first || tag_text.empty() || tag_text.find(target_text) == std::string::npos) continue;

    // This sanity check that the entity is valid should not be necessary with the switch to the
    // SetNameSpriteState_destructor call but adding it out of paranoia against a stale cache.
    Zeal::GameStructures::Entity *current_ent = Zeal::Game::get_entity_list();
    while (current_ent && current_ent != entry.first) current_ent = current_ent->Next;
    if (!current_ent || entry.first->Type != Zeal::GameEnums::NPC) continue;

    // There's a substring match but do a secondary exact check also.
    auto split = Zeal::String::split_text(tag_text, kDelimiter);
    if (!split.empty() && !split.back().empty() && split.back().back() == '\n')
      split.back().erase(split.back().length() - 1);
    for (const auto &field : split) {
      if (field == target_text) {
        matches.push_back(entry.first);
        break;
      }
    }
  }

  if (matches.empty()) return false;

  // Prevent exploitation by limiting this to entities that can be tab targeted.
  const float kMaxDist = 250;  // Same distance as tab cycle targeting.
  auto visible_entities = Zeal::Game::get_world_visible_actor_list(kMaxDist, true);
  std::vector<Zeal::GameStructures::Entity *> candidates;
  for (const auto &match : matches) {
    if (std::find(visible_entities.begin(), visible_entities.end(), match) != visible_entities.end())
      candidates.push_back(match);
  }

  if (candidates.empty()) return false;
  if (candidates.size() > 1) std::sort(candidates.begin(), candidates.end(), distance_comparison);
  Zeal::Game::set_target(candidates[0]);  // Return closest after sorting by distance.
  return true;
}

void NamePlate::handle_tag_command(const std::vector<std::string> &args) {
  if (args.size() == 2 && (args[1] == "on" || args[1] == "off")) {
    enable_tags(args[1] == "on");
    return;
  }

  if (args.size() >= 3 && args[1] == "target") {
    std::string target_text = args[2];
    for (int i = 3; i < args.size(); ++i) target_text += " " + args[i];
    if (!handle_tag_target(target_text)) {
      Zeal::Game::print_chat("No valid (visible) tag target exact match found.");
      Zeal::Game::set_target(nullptr);  // Null existing target to make it obvious it failed.
    }
    return;
  }

  if (args.size() >= 2 && args[1] == "join") {
    std::string channel = (args.size() == 2) ? setting_tag_channel.get() : args[2];

    if (!join_tag_channel(channel))
      Zeal::Game::print_chat("Invalid chat channel. It must start with %s (like '%s123')", kZealTagChannelPrefix,
                             kZealTagChannelPrefix);
    return;
  }

  if (args.size() >= 2 && args[1] == "filter") {
    if (args.size() > 2) setting_tag_filter.set(args[2] == "on");
    Zeal::Game::print_chat("Tag message filter: %s", setting_tag_filter.get() ? "on" : "off");
    return;
  }

  if (args.size() >= 2 && args[1] == "suppress") {
    if (args.size() > 2) setting_tag_suppress.set(args[2] == "on");
    Zeal::Game::print_chat("Tag message suppression: %s", setting_tag_suppress.get() ? "on" : "off");
    return;
  }

  if (args.size() >= 2 && args[1] == "prettyprint") {
    if (args.size() > 2) setting_tag_prettyprint.set(args[2] == "on");
    Zeal::Game::print_chat("Tag prettyprint: %s", setting_tag_prettyprint.get() ? "on" : "off");
    return;
  }

  if (args.size() >= 2 && args[1] == "tooltip") {
    if (args.size() > 2) setting_tag_tooltip.set(args[2] == "on");
    Zeal::Game::print_chat("Tag text tooltip: %s", setting_tag_tooltip.get() ? "on" : "off");
    return;
  }

  if (args.size() == 3 && args[1] == "channel") {
    broadcast_tag_set_channel(args[2]);
    return;
  }

  if (args.size() == 2 && args[1] == "clear") {
    auto target = Zeal::Game::get_target();
    if (target) {
      auto it = nameplate_info_map.find(target);
      if (it != nameplate_info_map.end()) {
        it->second.tag_text = "";
        it->second.tag_color = TagArrowColor::Off;
      }
      Zeal::Game::print_chat("Target nameplate tag cleared");
      return;
    }
    clear_tags();
    Zeal::Game::print_chat("Nameplate tags cleared");
    return;
  }

  if (args.size() > 2 && (args[1] == "rsay" || args[1] == "gsay" || args[1] == "local" || args[1] == "chat")) {
    if (!setting_tag_enable.get()) enable_tags(true);  // Auto-set to on if sending a message.
    bool rsay = (args[1] == "rsay");
    bool gsay = (args[1] == "gsay");
    bool chat = (args[1] == "chat");
    if (rsay && !Zeal::Game::RaidInfo->is_in_raid()) {
      Zeal::Game::print_chat("Must be in a raid to rsay");
      return;
    } else if (gsay && !Zeal::Game::GroupInfo->is_in_group()) {
      Zeal::Game::print_chat("Must be in a group to gsay");
      return;
    } else if (chat && setting_tag_channel.get().empty()) {
      Zeal::Game::print_chat("Must have a chat channel set");
      return;
    }

    bool is_clear = args.size() == 3 && args[2] == "clear";
    auto target = Zeal::Game::get_target();
    if (!is_clear && !is_taggable_target(target)) {
      Zeal::Game::print_chat("Must have a valid target with a visible nameplate to tag");
      return;
    }

    // Assemble the broadcast message.
    std::string tag_text = args[2];
    for (int i = 3; i < args.size(); ++i) tag_text += " " + args[i];
    if (tag_text.size() > kMaxTagTextLength) tag_text.resize(kMaxTagTextLength);  // Constrain to reasonable length.
    for (char &c : tag_text)                                                      // Limit to visible ASCII.
      if (!std::isprint(static_cast<unsigned char>(c))) c = '?';
    if (setting_tag_alternate_symbols.get()) std::replace(tag_text.begin(), tag_text.end(), '*', '^');
    std::string name = is_clear ? "0" : Zeal::Game::strip_name(target->Name);
    int spawn_id = is_clear ? 0 : target->SpawnId;
    std::string message = std::format("{0}{1}{2}{3}{4}{5}{6}", kZealTagHeader, kDelimiter, tag_text, kDelimiter, name,
                                      kDelimiter, spawn_id);

    // The sender doesn't receive a rsay message but does receive a gsay message, so we want to update our
    // own local state here if not gsay. We also use this to verify the message is parseable before
    // broadcast spamming others. The chat channel also has an echo.
    if (handle_tag_message(message.c_str(), !gsay && !chat)) {
      if (rsay)
        Zeal::Game::send_raid_chat(message);
      else if (gsay)
        Zeal::Game::do_gsay(message);
      else if (chat)
        send_tag_message_to_channel(message);
    } else {
      Zeal::Game::print_chat("Tagging failed (Check message formatting or validity of target)");
    }
    return;
  }

  Zeal::Game::print_chat("Usage: /tag <on | off | clear>");
  Zeal::Game::print_chat("Usage: /tag <tooltip | filter | suppress | prettyprint> <on | off>");
  Zeal::Game::print_chat("Usage: /tag target <text_to_match>");
  Zeal::Game::print_chat("Usage: /tag <gsay | rsay | chat> local> <message | clear | channel>");
  Zeal::Game::print_chat("Usage: <message> prefixes: '+' to append, '^R^' or '*R:' for color arrow (R, O, Y, G, B, W)");
  Zeal::Game::print_chat("Example: /tag rsay Assist me");
  Zeal::Game::print_chat("Example: /tag gsay Off tank");
  Zeal::Game::print_chat("Example: /tag gsay clear (broadcasts a clear all tags)");
  return;
}

// Returns a RGB color based on the color_key (else TagArrowColor::Off if no match).
static D3DCOLOR GetTagArrowColor(char color_key) {
  color_key = std::tolower(color_key);
  switch (color_key) {
    case 'r':
      return TagArrowColor::Red;
    case 'o':
      return TagArrowColor::Orange;
    case 'y':
      return TagArrowColor::Yellow;
    case 'g':
      return TagArrowColor::Green;
    case 'b':
      return TagArrowColor::Blue;
    case 'w':
      return TagArrowColor::White;
    case 'p':
      return TagArrowColor::Paw;
    case 's':
      return TagArrowColor::StopSign;
    default:
      break;
  }
  return TagArrowColor::Off;
};

// Parses "raw" (w/out any channel prefix like "Bob tells the raid, '") tag message to
// confirm it is in a valid format and if apply is true updates the nameplate info map.
bool NamePlate::handle_tag_message(const char *message, bool apply) {
  if (!setting_tag_enable.get() || !setting_zeal_fonts.get()) return true;  // Quickly bail out.

  static const int header_length = strlen(kZealTagHeader);
  static const int header_length_abbrev = strlen(kZealTagHeaderAbbreviated);
  if (!message || (strncmp(message, kZealTagHeader, header_length) &&
                   strncmp(message, kZealTagHeaderAbbreviated, header_length_abbrev)))
    return false;

  std::string message_str = message;
  auto split = Zeal::String::split_text(message_str, kDelimiter);
  if (split.size() < 4) return false;

  bool is_new_header = (split[0] == kZealTagHeaderAbbreviated);
  bool is_old_header = !is_new_header && (split[0] == kZealTagHeader);
  if ((!is_new_header && !is_old_header) || split[1].empty()) return false;

  if (split[1] == "clear") {
    if (apply) clear_tags();
    return true;
  } else if (split[1].starts_with(kZealTagChannelBroadcastPrefix)) {
    if (apply) handle_tag_set_channel_message(split[1]);
    return true;
  }

  int spawn_id = 0;
  if (!Zeal::String::tryParse(split[3], &spawn_id, true)) return false;
  auto entity = Zeal::Game::get_entity_by_id(spawn_id);
  if (!entity) return false;
  auto it = nameplate_info_map.find(entity);
  if (it == nameplate_info_map.end()) return false;

  if (!apply) return true;

  // Convert any characters that are not visible ASCII to ?.
  std::string tag_text = split[1];
  for (char &c : tag_text)
    if (!std::isprint(static_cast<unsigned char>(c))) c = '?';

  int original_length = tag_text.size();
  bool append = tag_text.size() && tag_text[0] == '+';
  bool erase = !append && tag_text.size() && tag_text[0] == '-';
  if (append || erase) tag_text = tag_text.substr(1);
  if (erase) it->second.tag_text = "";

  // The tag arrow is either enabled explicitly with a specific color (which must be disabled explicitly)
  // or enabled by default on a NPC if there is any tag text (can suppress with ^-^).
  if (tag_text.size() > 2 && tag_text[0] == '^') {
    it->second.tag_color = GetTagArrowColor(tag_text[1]);
    tag_text = tag_text.substr(2);
  } else if (it->second.tag_color == TagArrowColor::Off || it->second.tag_color == TagArrowColor::Nameplate) {
    bool disable_arrow = !setting_tag_default_arrow.get() || entity->Type != Zeal::GameEnums::NPC ||
                         (tag_text.empty() && it->second.tag_text.empty());
    it->second.tag_color = disable_arrow ? TagArrowColor::Off : TagArrowColor::Nameplate;
  }

  // Support skipping any unrecognized future prefix.
  if (tag_text.size() != original_length) {
    auto prefix_end_index = tag_text.find('^');  // Legacy end of prefix format.
    if (prefix_end_index != std::string::npos)
      tag_text = (prefix_end_index == tag_text.length()) ? "" : tag_text.substr(prefix_end_index + 1);
  }

  // If empty now it was a prefix only command which were handled above (append is a no-op).
  // We also only allow text tag content on NPCs's.
  if (tag_text.empty() || entity->Type != Zeal::GameEnums::NPC) return true;

  // Check if the tag_text should be appended to existing text. Preserve atomic fields.
  if (append && !it->second.tag_text.empty()) {
    std::string orig_tag_text = tag_text;
    auto tag_split = Zeal::String::split_text(it->second.tag_text, kDelimiter);
    if (!tag_split.empty() && !tag_split.back().empty() && tag_split.back().back() == '\n')
      tag_split.back().erase(tag_split.back().length() - 1);  // Strip trailing \n
    for (auto it = tag_split.rbegin(); it != tag_split.rend(); ++it) {
      if (*it == orig_tag_text) continue;  // Skip duplication.
      if (tag_text.length() + it->length() + strlen(kDelimiter) > kMaxTagTextLength) break;
      tag_text = *it + kDelimiter + tag_text;  // Only prepend if full split will fit.
    }
  }

  if (tag_text.size() > kMaxTagTextLength) tag_text = tag_text.substr(tag_text.size() - kMaxTagTextLength);
  tag_text += "\n";

  it->second.tag_text = tag_text;

  // Update nameplate color immediately (otherwise there is typically a lag).
  if (entity != Zeal::Game::get_target() && ZealService::get_instance()->ui && get_color_callback)
    it->second.color = get_color_callback(static_cast<int>(ColorIndex::Tagged));

  return true;
}

// The chat channel callback requires an additional layer of filtering beyond the single-channel only
// /rsay and /gsay channels that ccan directly call handle_tag_message(). This callback also supports
// immediate suppression of the message.
bool NamePlate::check_for_tag_channel_message(const char *message, int color_index) {
  if (!message || !message[0] || !setting_tag_enable.get()) return false;

  // The tag_channel_number is not set immediately when joining so we opportunistically try
  // to update it here.
  if (tag_channel_number < 0 && !setting_tag_channel.get().empty())
    tag_channel_number = Zeal::Game::get_channel_number(setting_tag_channel.get().c_str());
  if (tag_channel_number < 0) return false;

  // Only scan the expected response channel (if not joined, channel will be -1 and bail out above).
  if ((USERCOLOR_CHAT_1 + tag_channel_number) != color_index &&
      (USERCOLOR_ECHO_CHAT_1 + tag_channel_number) != color_index)
    return false;

  // Unlike gsay and rsay, the chat channel includes the sender prefix so extract the contents.
  std::string msg(message);
  auto start_index = msg.find('\'');
  auto end_index = msg.length() - 1;  // Known to be >= 1 from above.
  if (start_index == std::string::npos || end_index < start_index + 10 || msg[end_index] != '\'') return false;

  // And do another very simple and quick initial check to bail out early.
  if (msg[start_index + 1] != 'Z') return false;

  std::string contents = msg.substr(start_index + 1, end_index - 1);
  if (!handle_tag_message(contents.c_str())) return false;

  return setting_tag_suppress.get();
}

void NamePlate::handle_tag_set_channel_message(const std::string &text) {
  static const int kPrefixLength = strlen(kZealTagChannelBroadcastPrefix);
  if (text.length() <= kPrefixLength) return;
  std::string channel = text.substr(kPrefixLength);
  if (!join_tag_channel(channel)) Zeal::Game::print_chat("Zeal tag error: failed to join channel %s", channel.c_str());
}

void NamePlate::broadcast_tag_set_channel(const std::string &channel) {
  bool in_raid = Zeal::Game::RaidInfo->is_in_raid();
  if (!in_raid && !Zeal::Game::GroupInfo->is_in_group()) {
    Zeal::Game::print_chat("Must be in a raid or group to broadcast the channel");
    return;
  }

  // Check that the channel name is valid before broadcasting. Also apply if in_raid but not group which echoes.
  if (!join_tag_channel(channel, in_raid)) {
    Zeal::Game::print_chat("Invalid chat channel. It must start with %s (like '%s123')", kZealTagChannelPrefix,
                           kZealTagChannelPrefix);
    return;
  }

  std::string text = kZealTagChannelBroadcastPrefix + channel;
  std::string message =
      std::format("{0}{1}{2}{3}{4}{5}{6}", kZealTagHeader, kDelimiter, text, kDelimiter, "0", kDelimiter, 0);

  if (in_raid)
    Zeal::Game::send_raid_chat(message.c_str());
  else
    Zeal::Game::do_gsay(message);
}

bool NamePlate::join_tag_channel(const std::string &channel_in, bool apply) {
  if (channel_in.empty()) return false;

  // Chat channels require the first character to be capitalized and the rest lowercase.
  std::string channel = channel_in;
  std::transform(channel.begin(), channel.end(), channel.begin(), [](unsigned char c) { return std::tolower(c); });
  channel[0] = std::toupper(channel[0]);

  static const int kPrefixLength = strlen(kZealTagChannelPrefix);
  if (!channel.starts_with(kZealTagChannelPrefix) || channel.length() <= kPrefixLength) return false;

  if (!apply) return true;

  setting_tag_channel.set(channel);

  // Join the channel to send and receive responses. First check if already joined though.
  tag_channel_number = Zeal::Game::get_channel_number(setting_tag_channel.get().c_str());
  if (tag_channel_number < 0) {
    tag_channel_number = kTagChannelJoinPending;
    Zeal::Game::print_chat("Attempting to join channel: %s", setting_tag_channel.get().c_str());
    Zeal::Game::do_join(Zeal::Game::get_self(), setting_tag_channel.get().c_str());
  }

  return true;
}

// Sends the formatted response back to the response channel.
void NamePlate::send_tag_message_to_channel(const std::string &message) {
  if (tag_channel_number < 0 && !setting_tag_channel.get().empty())
    tag_channel_number = Zeal::Game::get_channel_number(setting_tag_channel.get().c_str());

  if (tag_channel_number < 0) {
    Zeal::Game::print_chat("You must join a channel (/tag join) before broadcasting to chat");
    return;
  }

  Zeal::Game::send_to_channel(tag_channel_number, message.c_str());
}

static std::string prettyprint_tag_message(const std::string &msg) {
  auto split = Zeal::String::split_text(msg, kDelimiter);
  if (split.size() < 4 || split[1].empty() || split[2].empty()) return msg;

  std::string prefix;
  std::string text = split[1];  // Extract the tag text field first.

  // Decode the prefix (if any).
  auto original_length = text.length();
  if (text[0] == '+') {
    text = text.substr(1);  // Don't bother adding a prefix comment for Append.
  } else if (text[0] == '-') {
    text = text.substr(1);  // Don't bother adding a prefix comment for Erase.
  }

  if (text.size() > 2 && text[0] == '^') {
    if (text[1] == 's' || text[1] == 'S')
      prefix += "Stop";
    else if (text[1] == 'p' || text[1] == 'P')
      prefix += "Paw";
    else
      prefix += std::string("Arrow:") + text[1];
    text = text.substr(2);
  }

  // Prefix ends with a ^ if one exists. Adding this search here to try and support
  // future prefix enhancements so they just get ignored instead of breaking things.
  if (text.size() != original_length) {
    auto prefix_end_index = text.find('^');
    if (prefix_end_index != std::string::npos)
      text = (prefix_end_index == text.length()) ? "" : text.substr(prefix_end_index + 1);
  }

  text = text + " => " + split[2];                   // Append the target name.
  if (!prefix.empty()) text += " (" + prefix + ")";  // And any prettified labels.
  return text;
}

bool NamePlate::handle_zeal_spam_filter(short &channel, std::string &msg) {
  if (msg.empty() || !setting_tag_filter.get()) return false;

  // First do a quick channel check to bail out early.
  static constexpr int kGroupTextChannel = 2;  // For chan_num in ChannelMessage_Struct.
  static constexpr int kRaidTextChannel = 15;  // For chan_num in ChannelMessage_Struct.
  short tag_channel = (tag_channel_number < 0) ? -1 : (USERCOLOR_CHAT_1 + tag_channel_number);
  short tag_channel_echo = (tag_channel_number < 0) ? -1 : (USERCOLOR_ECHO_CHAT_1 + tag_channel_number);
  if (channel != USERCOLOR_GROUP && channel != USERCOLOR_ECHO_GROUP && channel != USERCOLOR_RAID_SAY &&
      channel != tag_channel && channel != tag_channel_echo)
    return false;

  // In all of these channels, the contents of the message lie between '' quotes.
  auto start_index = msg.find('\'');
  auto end_index = msg.length() - 1;
  if (start_index == std::string::npos || end_index < start_index + 10 || msg[end_index] != '\'') return false;

  // And do another very simple and quick initial check to bail out early.
  if (msg[start_index + 1] != 'Z') return false;

  // Then do a full check that it is a valid message.
  std::string contents = msg.substr(start_index + 1, end_index - 1);
  if (!handle_tag_message(contents.c_str(), false)) return false;

  // Future option: Clean up the messages (strip/translate prefix, merge target name).
  if (setting_tag_suppress.get())
    msg = "";  // Clear the message to suppress it (dropped downstream).
  else if (setting_tag_prettyprint.get())
    msg = msg.substr(0, start_index + 1) + prettyprint_tag_message(contents) + "'";

  channel = CHANNEL_ZEAL_SPAM;
  return true;
}

// If prettyprint is enabled, disable the /filter badword which mangles the prefix in SpeakBabel.
void NamePlate::synchronize_pretty_print() const {
  int *const kDisableBadWordFilter = reinterpret_cast<int *>(0x00798b24);
  if (setting_tag_prettyprint.get()) *kDisableBadWordFilter = 1;
}

static const char *get_tag_color_description(DWORD color) {
  switch (color) {
    case TagArrowColor::Off:
      return "";
    case TagArrowColor::Nameplate:
      return "Arrow";
    case TagArrowColor::Red:
      return "Red Arrow";
    case TagArrowColor::Orange:
      return "Orange Arrow";
    case TagArrowColor::Yellow:
      return "Yellow Arrow";
    case TagArrowColor::Green:
      return "Green Arrow";
    case TagArrowColor::Blue:
      return "Blue Arrow";
    case TagArrowColor::White:
      return "White Arrow";
    case TagArrowColor::Paw:
      return "Paw";
    case TagArrowColor::StopSign:
      return "Stop";
    default:
      break;
  }
  return "Unknown";
}

void NamePlate::handle_targetwnd_postdraw(Zeal::GameUI::SidlWnd *wnd) const {
  if (!wnd || !setting_tag_tooltip.get()) return;

  auto target = Zeal::Game::get_target();
  if (!target) return;

  // Find the text.
  const auto it = nameplate_info_map.find(target);
  if (it == nameplate_info_map.end()) return;
  const auto &tag_text = it->second.tag_text;
  const char *color_text = get_tag_color_description(it->second.tag_color);
  const char *text = tag_text.empty() ? color_text : tag_text.c_str();
  std::string combined_text;
  if (!tag_text.empty() && color_text && color_text[0]) {
    combined_text = std::string(color_text) + kDelimiter + tag_text;
    text = combined_text.c_str();
  }
  if (!text || !text[0]) return;

  // Just bail out if existing tooltip data. This isn't expected so keep it simple.
  if (wnd->ToolTipText.Data) return;

  // Add the text, draw it, and make sure to release to avoid a leak.
  wnd->ToolTipText.Set(text);
  Zeal::GameUI::CXRect relativeRect = wnd->GetScreenRect();
  int x = setting_tag_tooltip_align.get() ? relativeRect.Left : relativeRect.Right;
  int y = setting_tag_tooltip_align.get() ? relativeRect.Bottom : relativeRect.Top;
  wnd->DrawTooltipAtPoint(x, y);
  wnd->ToolTipText.FreeRep();
}
#pragma once
#include <Windows.h>

#include <functional>
#include <string>
#include <vector>

#include "bitmap_font.h"
#include "game_structures.h"
#include "game_ui.h"
#include "memory.h"
#include "zeal_settings.h"

class NamePlate {
 public:
  // The positive color indices must be kept in sync with the color options.
  enum class ColorIndex : int {
    UseConsider = -2,  // Special internal signal to use consider level.
    UseClient = -1,    // Internal signal to use the client default color.
    AFK = 0,
    LFG = 1,
    LD = 2,
    MyGuild = 3,
    Raid = 4,
    Group = 5,
    PVP = 6,
    Role = 7,
    OtherGuild = 8,
    Adventurer = 9,
    NpcCorpse = 10,
    PlayerCorpse = 11,
    Target = 18,
    Tagged = 29,
    GuildLFG = 30,
    PvpAlly = 31,
  };

  NamePlate(class ZealService *zeal);
  ~NamePlate();

  void add_options_callback(std::function<void()> callback) { update_options_ui_callback = callback; };

  void add_get_color_callback(std::function<unsigned int(int index)> callback) { get_color_callback = callback; };

  // Tint (color) settings.
  ZealSetting<bool> setting_colors = {false, "Zeal", "NameplateColors", false};
  ZealSetting<bool> setting_con_colors = {false, "Zeal", "NameplateConColors", false};
  ZealSetting<bool> setting_target_color = {false, "Zeal", "NameplateTargetColor", false};
  ZealSetting<bool> setting_char_select = {false, "Zeal", "NameplateCharSelect", false};

  // Tag settings.
  ZealSetting<bool> setting_tag_enable = {false, "Zeal", "NameplateTagEnable", false};
  ZealSetting<bool> setting_tag_tooltip = {false, "Zeal", "NameplateTagToolTip", false};
  ZealSetting<bool> setting_tag_tooltip_align = {false, "Zeal", "NameplateTagToolTipAlign", false};
  ZealSetting<bool> setting_tag_filter = {false, "Zeal", "NameplateTagFilter", false};
  ZealSetting<bool> setting_tag_suppress = {false, "Zeal", "NameplateTagSuppress", false,
                                            [this](bool val) { synchronize_pretty_print(); }};
  ZealSetting<bool> setting_tag_prettyprint = {false, "Zeal", "NameplateTagPrettyPrint", false};
  ZealSetting<bool> setting_tag_default_arrow = {true, "Zeal", "NameplateTagDefaultArrow", false};
  ZealSetting<bool> setting_tag_alternate_symbols = {false, "Zeal", "NameplateTagAlternateSymbols", false};
  ZealSetting<std::string> setting_tag_channel = {"", "Zeal", "NameplateTagChannel", false};

  // Text settings.
  ZealSetting<bool> setting_hide_self = {false, "Zeal", "NameplateHideSelf", false};
  ZealSetting<bool> setting_x = {false, "Zeal", "NameplateX", false};
  ZealSetting<bool> setting_hide_raid_pets = {false, "Zeal", "NameplateHideRaidPets", false};
  ZealSetting<bool> setting_show_pet_owner_name = {false, "Zeal", "NameplateShowPetOwnerName", false};
  ZealSetting<bool> setting_target_marker = {false, "Zeal", "NameplateTargetMarker", false};
  ZealSetting<bool> setting_target_health = {false, "Zeal", "NameplateTargetHealth", false};
  ZealSetting<bool> setting_target_blink = {true, "Zeal", "NameplateTargetBlink", false};
  ZealSetting<bool> setting_attack_only = {false, "Zeal", "NameplateAttackOnly", false};
  ZealSetting<bool> setting_inline_guild = {false, "Zeal", "NameplateInlineGuild", false};

  ZealSetting<bool> setting_extended_nameplate = {true, "Zeal", "NameplateExtended", false,
                                                  [this](const bool &val) { mem::write<BYTE>(0x4B0B3D, val ? 0 : 1); }};

  // Extended shownames (allows /shownames 5-7)
  ZealSetting<bool> setting_extended_shownames = {true, "Zeal", "NameplateExtendedShownames", false,
                                                  [this](const bool &val) {
                                                    if (val) {
                                                      // Verify we have the expected byte before patching
                                                      BYTE target_val = val ? 0x08 : 0x05;
                                                      BYTE current_val = 0;
                                                      mem::get(0x004ff8ff, 1, &current_val);
                                                      if (current_val != target_val)
                                                        mem::write<BYTE>(0x004ff8ff, target_val);
                                                    } else {
                                                      mem::write<BYTE>(0x004ff8ff, 0x05);  // Restore original value
                                                    }
                                                  }};
  // Local AA Title Choice
  ZealSetting<int> setting_local_aa_title = {3, "Zeal", "NameplateLocalAATitle", true};

  // Advanced fonts
  ZealSetting<bool> setting_health_bars = {false, "Zeal", "NameplateHealthBars", false};
  ZealSetting<bool> setting_raid_health_bars = {false, "Zeal", "NameplateRaidHealthBars", false};
  ZealSetting<bool> setting_mana_bars = {false, "Zeal", "NameplateManaBars", false};
  ZealSetting<bool> setting_stamina_bars = {false, "Zeal", "NameplateStaminaBars", false};
  ZealSetting<bool> setting_zeal_fonts = {false, "Zeal", "NamePlateZealFonts", false, [this](bool val) { clean_ui(); }};
  ZealSetting<bool> setting_drop_shadow = {false, "Zeal", "NamePlateDropShadow", false,
                                           [this](bool val) { clean_ui(); }};
  ZealSetting<float> setting_shadow_offset_factor = {BitmapFontBase::kDefaultShadowOffsetFactor, "Zeal",
                                                     "NamePlateShadowOffsetFactor", false};
  ZealSetting<std::string> setting_fontname = {std::string(BitmapFont::kDefaultFontName), "Zeal", "NamePlateFontname",
                                               false, [this](std::string val) { clean_ui(); }};

  std::vector<std::string> get_available_fonts() const;

  // Internal use only (public for use by callbacks).
  bool handle_SetNameSpriteTint(Zeal::GameStructures::Entity *entity);
  bool handle_SetNameSpriteState(void *this_display, Zeal::GameStructures::Entity *entity, int show);
  void handle_targetwnd_postdraw(Zeal::GameUI::SidlWnd *wnd) const;
  void handle_entity_destructor(Zeal::GameStructures::Entity *entity);

 private:
  struct NamePlateInfo {
    std::string text;
    std::string tag_text;
    DWORD color;
    DWORD tag_color;
  };

  struct RenderInfo {
    NamePlateInfo *info;
    float distance;
    Vec2 screen_xy;
  };

  void parse_args(const std::vector<std::string> &args);
  void dump() const;
  ColorIndex get_color_index(const Zeal::GameStructures::Entity &entity);
  ColorIndex get_player_color_index(const Zeal::GameStructures::Entity &entity) const;
  ColorIndex get_pet_color_index(const Zeal::GameStructures::Entity &entity) const;
  std::string generate_nameplate_text(const Zeal::GameStructures::Entity &entity, int show) const;
  std::string generate_target_postamble(const Zeal::GameStructures::Entity &entity) const;
  bool is_nameplate_hidden_by_race(const Zeal::GameStructures::Entity &entity) const;
  bool is_group_member(const Zeal::GameStructures::Entity &entity) const;
  bool is_raid_member(const Zeal::GameStructures::Entity &entity) const;
  bool is_hp_updated(const Zeal::GameStructures::Entity *entity) const;
  bool handle_shownames_command(const std::vector<std::string> &args);
  void handle_tag_command(const std::vector<std::string> &args);
  bool handle_tag_target(const std::string &target_text);
  bool handle_zeal_spam_filter(short &channel, std::string &msg);
  void enable_tags(bool enable);
  void clear_tags();
  bool handle_tag_message(const char *message, bool apply = true);
  void handle_tag_set_channel_message(const std::string &message);
  void broadcast_tag_set_channel(const std::string &channel);
  bool join_tag_channel(const std::string &channel, bool apply = true);
  void send_tag_message_to_channel(const std::string &message);
  bool check_for_tag_channel_message(const char *message, int color_index);
  void synchronize_pretty_print() const;

  void clean_ui();
  void render_ui();
  void load_sprite_font();

  std::unique_ptr<SpriteFont> sprite_font;
  std::unique_ptr<class TagArrows> tag_arrows;
  std::unordered_map<struct Zeal::GameStructures::Entity *, NamePlateInfo> nameplate_info_map;
  std::function<void()> update_options_ui_callback;
  std::function<unsigned int(int)> get_color_callback;
  int tag_channel_number = -1;
};

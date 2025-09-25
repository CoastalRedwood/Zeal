#pragma once
#include <Windows.h>

#include <string>
#include <unordered_map>
#include <vector>

#include "bitmap_font.h"
#include "d3dx8/d3d8.h"
#include "game_packets.h"
#include "game_structures.h"
#include "zeal_settings.h"

struct DamageData {
  ULONGLONG start_time;
  ULONGLONG last_tick;
  float y_offset;
  float x_offset;
  bool needs_removed;
  float opacity;
  void tick(int active_number_count);
  std::string str_dmg;
  Zeal::GameStructures::SPELL *spell;
  D3DCOLOR color;
  bool highlight;

  // Damage is positive, heals are negative. Percent adds a % suffix.
  DamageData(int dmg, bool percent, Zeal::GameStructures::SPELL *spell, D3DCOLOR color, bool highlight);
};

class FloatingDamage {
 public:
  static constexpr char kUseClientFontString[] = "Use client /fcd #";

  void add_damage(Zeal::GameStructures::Entity *source, Zeal::GameStructures::Entity *target, WORD type, short spell_id,
                  short damage, char output_text);
  void handle_hp_update_packet(const Zeal::Packets::SpawnHPUpdate_Struct *packet);
  void callback_deferred();
  void callback_render();
  ZealSetting<bool> enabled = {false, "FloatingDamage", "Enabled", true};
  ZealSetting<bool> hide_with_gui = {false, "FloatingDamage", "HideWithGui", true};
  ZealSetting<bool> spell_icons = {true, "FloatingDamage", "Icons", true};
  ZealSetting<bool> show_spells = {true, "FloatingDamage", "Spells", true};
  ZealSetting<bool> show_melee = {true, "FloatingDamage", "Melee", true};
  ZealSetting<bool> show_self = {true, "FloatingDamage", "Self", true};
  ZealSetting<bool> show_pets = {true, "FloatingDamage", "Pets", true};
  ZealSetting<bool> show_others = {true, "FloatingDamage", "Others", true};
  ZealSetting<bool> show_npcs = {true, "FloatingDamage", "Npcs", true};
  ZealSetting<bool> show_hp_updates = {true, "FloatingDamage", "ShowHpUpdates", true};
  ZealSetting<int> big_hit_threshold = {100, "FloatingDamage", "BigHitThreshold", true};
  ZealSetting<std::string> bitmap_font_filename = {std::string(kUseClientFontString), "FloatingDamage", "Font", true,
                                                   [this](std::string val) { bitmap_font.reset(); }};
  std::vector<std::string> get_available_fonts() const;
  void init_ui();
  void clean_ui();
  void draw_icon(int index, float x, float y, float opacity);
  int get_active_damage_count(Zeal::GameStructures::Entity *ent);

  void add_options_callback(std::function<void()> callback) { update_options_ui_callback = callback; };

  void add_get_color_callback(std::function<unsigned int(int index)> callback) { get_color_callback = callback; };

  FloatingDamage(class ZealService *zeal);
  ~FloatingDamage();

 private:
  D3DCOLOR get_color(bool is_my_damage, bool is_damage_to_me, bool is_damage_to_player, bool is_spell, bool highlight);
  bool is_visible() const;
  bool add_texture(std::string path);
  std::vector<IDirect3DTexture8 *> textures;
  IDirect3DTexture8 *load_texture(std::string path);
  void load_bitmap_font();
  void render_spells();
  void render_text();  // Called in "render" for bitmap_font or "deferred" for CTextureFont.
  std::unique_ptr<BitmapFont> bitmap_font = nullptr;
  int font_size = 5;
  std::unordered_map<Zeal::GameStructures::Entity *, std::vector<DamageData>> damage_numbers;
  std::function<void()> update_options_ui_callback;
  std::function<unsigned int(int)> get_color_callback;
};

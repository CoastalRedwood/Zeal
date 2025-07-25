#include "floating_damage.h"

#include <cstdint>
#include <random>

#include "game_addresses.h"
#include "game_packets.h"
#include "string_util.h"
#include "zeal.h"

#if 0  // Not currently used.

template <typename VertexType>
LPDIRECT3DVERTEXBUFFER8 CreateVertexBuffer(LPDIRECT3DDEVICE8 d3dDevice, VertexType* vertices, int vertexCount, DWORD fvf)
{
	LPDIRECT3DVERTEXBUFFER8 vertexBuffer = nullptr;

	// Create the vertex buffer
	HRESULT result = d3dDevice->CreateVertexBuffer(
		vertexCount * sizeof(VertexType),   // Size of the buffer
		D3DUSAGE_WRITEONLY,                 // Usage flags
		fvf,                                // Flexible Vertex Format (FVF)
		D3DPOOL_DEFAULT,                    // Memory pool to use
		&vertexBuffer                       // Output vertex buffer
	);

	if (FAILED(result)) {
		// Handle error
		return nullptr;
	}

	// Lock the vertex buffer and copy the vertex data into it
	void* pVertices;
	vertexBuffer->Lock(0, vertexCount * sizeof(VertexType), (BYTE**)&pVertices, 0);
	memcpy(pVertices, vertices, vertexCount * sizeof(VertexType));
	vertexBuffer->Unlock();

	return vertexBuffer;
}
#endif

int getRandomIntBetween(int min, int max) {
  // Create a random device and a random engine
  std::random_device rd;
  static std::mt19937 gen(rd());

  // Define the distribution range
  std::uniform_int_distribution<> dis(min, max);

  // Generate and return the random integer
  return dis(gen);
}

void DamageData::tick(int active_number_count) {
  unsigned int duration = highlight ? 3000 : 2500;

  if (GetTickCount64() - start_time > duration) {
    needs_removed = true;
    return;
  }
  if (GetTickCount64() - last_tick >= 25) {
    float speed_offset = (float)active_number_count / 20.f;
    float opacity_offset = (float)active_number_count / 200.f;
    y_offset -= 2 + speed_offset;
    if (highlight)
      opacity -= 0.005f;  // Fade half as fast, no congestion fading.
    else
      opacity -= 0.01f + opacity_offset;
    if (opacity < 0) needs_removed = true;
    last_tick = GetTickCount64();
  }
}

DamageData::DamageData(int dmg, bool percent, Zeal::GameStructures::SPELL *_spell, D3DCOLOR _color, bool _highlight) {
  if (dmg < 0)
    str_dmg = "+" + std::to_string(-dmg);
  else
    str_dmg = std::to_string(dmg);

  if (percent) str_dmg += "%";

  opacity = 1.0f;
  y_offset = static_cast<float>(getRandomIntBetween(-20, 20));
  x_offset = static_cast<float>(getRandomIntBetween(-40, 40));
  needs_removed = false;
  last_tick = GetTickCount64();
  start_time = GetTickCount64();
  spell = _spell;
  color = _color;
  highlight = _highlight;
}

long FloatRGBAtoLong(float r, float g, float b, float a) {
  // Clamp values between 0 and 1
  if (r < 0.0f)
    r = 0.0f;
  else if (r > 1.0f)
    r = 1.0f;
  if (g < 0.0f)
    g = 0.0f;
  else if (g > 1.0f)
    g = 1.0f;
  if (b < 0.0f)
    b = 0.0f;
  else if (b > 1.0f)
    b = 1.0f;
  if (a < 0.0f)
    a = 0.0f;
  else if (a > 1.0f)
    a = 1.0f;

  // Convert float values to 8-bit unsigned integers (0-255 range)
  std::uint8_t red = static_cast<std::uint8_t>(r * 255);
  std::uint8_t green = static_cast<std::uint8_t>(g * 255);
  std::uint8_t blue = static_cast<std::uint8_t>(b * 255);
  std::uint8_t alpha = static_cast<std::uint8_t>(a * 255);

  // Pack RGBA values into a long integer (32-bit)
  return (static_cast<std::uint32_t>(alpha) << 24) | (static_cast<std::uint32_t>(red) << 16) |
         (static_cast<std::uint32_t>(green) << 8) | static_cast<std::uint32_t>(blue);
}

long ModifyAlpha(long rgba, float newAlpha) {
  // Clamp the alpha value between 0.0 and 1.0
  if (newAlpha < 0.0f)
    newAlpha = 0.0f;
  else if (newAlpha > 1.0f)
    newAlpha = 1.0f;

  // Convert the new alpha float value to an 8-bit integer (0-255)
  std::uint8_t alpha = static_cast<std::uint8_t>(newAlpha * 255);

  // Mask out the old alpha value and set the new alpha value
  rgba = (rgba & 0x00FFFFFF) | (alpha << 24);

  return rgba;
}

int FloatingDamage::get_active_damage_count(Zeal::GameStructures::Entity *ent) {
  if (!damage_numbers.count(ent)) return 0;
  return damage_numbers[ent].size();
}

bool FloatingDamage::is_visible() const {
  // In default text mode FCD visibility follows the GUI windows since the text uses that
  // rendering path while in ZealFonts text mode it is visible unless explicitly told to hide.
  return ((bitmap_font && !hide_with_gui.get()) || Zeal::Game::is_gui_visible());
}

void FloatingDamage::callback_render() {
  if (!enabled.get() || !Zeal::Game::is_in_game() || !is_visible()) return;

  load_bitmap_font();
  if (bitmap_font) render_text();

  render_spells();
}

void FloatingDamage::render_spells() {
  if (!spell_icons.get()) return;
  std::vector<Zeal::GameStructures::Entity *> visible_ents = Zeal::Game::get_world_visible_actor_list(250, false);
  Vec2 screen_size = ZealService::get_instance()->dx->GetScreenRect();
  for (auto &[target, dmg_vec] : damage_numbers) {
    for (auto &dmg : dmg_vec) {
      if (dmg.spell) {
        if (std::find(visible_ents.begin(), visible_ents.end(), target) != visible_ents.end() ||
            target == Zeal::Game::get_self()) {
          Vec2 screen_pos = {0, 0};
          if (ZealService::get_instance()->dx->WorldToScreen(
                  {target->Position.x, target->Position.y, target->Position.z}, screen_pos)) {
            draw_icon(dmg.spell->Icon, screen_pos.x + dmg.y_offset - 5, screen_pos.y + dmg.x_offset - 28, dmg.opacity);
          }
        }
      }
    }
  }
}

void FloatingDamage::callback_deferred() {
  // This callback only happens if is_gui_visible().
  if (enabled.get() && Zeal::Game::is_in_game() && bitmap_font == nullptr)
    render_text();  // Bitmap fonts were disabled, so use the client CTextureFont.
}

void FloatingDamage::render_text() {
  if (bitmap_font || Zeal::Game::get_wnd_manager()) {
    Zeal::GameUI::CTextureFont *fnt = bitmap_font ? nullptr : Zeal::Game::get_wnd_manager()->GetFont(font_size);
    if (bitmap_font || fnt) {
      std::vector<Zeal::GameStructures::Entity *> visible_ents = Zeal::Game::get_world_visible_actor_list(250, false);
      Vec2 screen_size = ZealService::get_instance()->dx->GetScreenRect();
      for (auto &[target, dmg_vec] : damage_numbers) {
        for (auto &dmg : dmg_vec) {
          dmg.tick(get_active_damage_count(target));
          if (!dmg.needs_removed &&
              (std::find(visible_ents.begin(), visible_ents.end(), target) != visible_ents.end() ||
               target == Zeal::Game::get_self())) {
            Vec2 screen_pos = {0, 0};
            if (ZealService::get_instance()->dx->WorldToScreen(
                    {target->Position.x, target->Position.y, target->Position.z}, screen_pos)) {
              long color = ModifyAlpha(dmg.color, dmg.opacity);
              if (bitmap_font)
                bitmap_font->queue_string(dmg.str_dmg.c_str(),
                                          Vec3(screen_pos.y + dmg.x_offset, screen_pos.x + dmg.y_offset, 0), false,
                                          color);
              else
                fnt->DrawWrappedText(
                    dmg.str_dmg.c_str(),
                    Zeal::GameUI::CXRect((int)(screen_pos.x + dmg.y_offset), (int)(screen_pos.y + dmg.x_offset),
                                         (int)(screen_pos.x + 150), (int)(screen_pos.y + 150)),
                    Zeal::GameUI::CXRect(0, 0, (int)(screen_size.x * 2), (int)(screen_size.y * 2)), color, 1, 0);
            }
          }
        }
        dmg_vec.erase(
            std::remove_if(dmg_vec.begin(), dmg_vec.end(), [](const DamageData &d) { return d.needs_removed; }),
            dmg_vec.end());
      }
      if (bitmap_font) bitmap_font->flush_queue_to_screen();
    }
    for (auto it = damage_numbers.begin(); it != damage_numbers.end();) {
      if (it->second.empty()) {
        it = damage_numbers.erase(it);
      } else {
        ++it;
      }
    }
  }
}

static D3DCOLOR get_color(bool is_my_damage, bool is_damage_to_me, bool is_damage_to_player, bool is_spell) {
  int color_index = 0;
  if (is_my_damage)
    color_index = is_spell ? 33 : 32;  // Me creating damage.
  else if (is_damage_to_me)
    color_index = is_spell ? 35 : 34;  // Me getting hit.
  else if (is_damage_to_player)
    color_index = is_spell ? 37 : 36;  // Player being hit.
  else
    color_index = is_spell ? 39 : 38;  // NPC being hit.

  return ZealService::get_instance()->ui->options->GetColor(static_cast<int>(color_index));
}

void FloatingDamage::handle_hp_update_packet(const Zeal::Packets::SpawnHPUpdate_Struct *packet) {
  if (!packet || !show_hp_updates.get()) return;
  auto entity = Zeal::Game::get_entity_by_id(packet->spawn_id);
  if (!entity) return;

  auto color = D3DCOLOR_XRGB(0x00, 0xff, 0x00);
  // Self hp updates are in hitpoints.
  if (entity == Zeal::Game::get_self()) {
    // Disabling since the cur_hp does not include various HP modifications (like armor)
    // and the client updates itself from spells and buffs.
    // int delta_hps = packet->cur_hp - entity->HpCurrent;
    // if (delta_hps > 0)
    //	damage_numbers[entity].push_back(DamageData(-delta_hps, false, nullptr, color, false));
    return;
  }

  // NPCs are sent in percent.
  const int kMinPercent = 5;  // Do not report HP updates < 5% since those can be normal tick regens.
  if (entity->Type == Zeal::GameEnums::NPC) {
    if (packet->cur_hp <= 100 && packet->max_hp == 100) {
      int delta_percent = packet->cur_hp - entity->HpCurrent;
      int min_limit = (entity->PetOwnerSpawnId == 0) ? 0 : kMinPercent;  // Don't show pet regen ticks.
      if (delta_percent > min_limit && packet->cur_hp > 0)
        damage_numbers[entity].push_back(DamageData(-delta_percent, true, nullptr, color, false));
    }
    return;
  }

  if (entity->Type != Zeal::GameEnums::Player) return;  // corpses

  // The server should be sending percents for other players based on the translation of
  // the OP_MobHealth packets, but we normalize it since that's what the client does.
  if (!packet->max_hp || !entity->HpMax) return;

  int current_percent = entity->HpCurrent * 100 / entity->HpMax;
  int new_percent = packet->cur_hp * 100 / packet->max_hp;
  int delta_percent = new_percent - current_percent;
  if (new_percent > 0 && delta_percent >= kMinPercent)
    damage_numbers[entity].push_back(DamageData(-delta_percent, true, nullptr, color, false));
}

void FloatingDamage::add_damage(Zeal::GameStructures::Entity *source, Zeal::GameStructures::Entity *target, WORD type,
                                short spell_id, short damage, char output_text) {
  if (!enabled.get() || !is_visible()) return;

  if (!target || damage <= 0) return;

  bool is_player_damage = (source && source->Type == Zeal::GameEnums::Player);

  bool is_spell = (spell_id > 0);
  if (is_player_damage && is_spell && !show_spells.get()) return;

  if (is_player_damage && !is_spell && !show_melee.get()) return;

  auto self = Zeal::Game::get_controlled();
  bool is_my_pet = (source && self && source->PetOwnerSpawnId == self->SpawnId);
  bool is_my_damage = is_my_pet || (source && self && source->SpawnId == self->SpawnId);
  if (is_my_damage && !show_self.get()) return;

  bool is_pet = (source && source->PetOwnerSpawnId != 0);  // Just filter all pet damage (except my pet).
  if (is_pet && !is_my_pet && !show_pets.get()) return;

  bool is_others = !is_my_damage;
  if (is_player_damage && is_others && !show_others.get()) return;

  bool is_npc_damage = (source && source->Type == Zeal::GameEnums::NPC);
  if (is_npc_damage && !is_my_pet && !show_npcs.get()) return;

  bool is_damage_to_me = (target == Zeal::Game::get_controlled());
  bool is_damage_to_player = (target->Type == Zeal::GameEnums::Player);
  auto color = get_color(is_my_damage, is_damage_to_me, is_damage_to_player, is_spell);
  auto sp_data = is_spell ? Zeal::Game::get_spell_mgr()->Spells[spell_id] : nullptr;
  bool highlight = (damage >= big_hit_threshold.get()) || (type == 8);  // Backstab as starting point.
  damage_numbers[target].push_back(DamageData(damage, false, sp_data, color, highlight));
}

void FloatingDamage::draw_icon(int texture_index, float y, float x, float opacity) {
  IDirect3DDevice8 *device = ZealService::get_instance()->dx->GetDevice();
  DWORD sheet_index = texture_index / 100;
  if (sheet_index > textures.size()) return;
  IDirect3DTexture8 *texture = textures.at(sheet_index);
  texture_index = texture_index % 99;
  const int image_size = 24;         // Width and height of each image in pixels
  const int texture_grid_size = 10;  // 10x10 grid

  // Calculate row and column based on index
  int row = texture_index / texture_grid_size;
  int column = texture_index % texture_grid_size;

  // Calculate UV coordinates
  float u_start = column * (image_size / 256.0f);  // 240 is total width of texture
  float v_start = row * (image_size / 256.0f);     // 240 is total height of texture
  float u_end = u_start + (image_size / 256.0f);
  float v_end = v_start + (image_size / 256.0f);

  // Define the vertices for a textured quad
  struct Vertex {
    float x, y, z, rhw;
    DWORD color;
    float u, v;
  };

  DWORD color = D3DCOLOR_ARGB((int)(255.f * opacity), 255, 255, 255);

  Vertex vertices[] = {{x, y, 0.0f, 1.0f, color, u_start, v_start},
                       {x + image_size, y, 0.0f, 1.0f, color, u_end, v_start},
                       {x, y + image_size, 0.0f, 1.0f, color, u_start, v_end},
                       {x + image_size, y + image_size, 0.0f, 1.0f, color, u_end, v_end}};
  ZealService::get_instance()->target_ring->setup_render_states();
  device->SetTexture(0, texture);
  device->SetVertexShader(D3DFVF_XYZRHW | D3DFVF_TEX1 | D3DFVF_DIFFUSE);
  device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(Vertex));
  device->SetTexture(0, NULL);  // Release reference to texture.
  ZealService::get_instance()->target_ring->reset_render_states();
}

IDirect3DTexture8 *FloatingDamage::load_texture(std::string path) {
  IDirect3DTexture8 *texture = nullptr;
  // Get the Direct3D device
  IDirect3DDevice8 *device = ZealService::get_instance()->dx->GetDevice();
  if (!device) {
    Zeal::Game::print_chat("Error: Failed to get Direct3D device.");
    return nullptr;
  }

  // Create texture from file
  HRESULT result = D3DXCreateTextureFromFileA(device, path.c_str(), &texture);
  if (FAILED(result)) {
    Zeal::Game::print_chat("Error: Failed to load texture file: " + path);
    return nullptr;
  }
  return texture;
}

bool FloatingDamage::add_texture(std::string path) {
  if (std::filesystem::exists(path)) {
    // Zeal::Game::print_chat("Added texture: %s", path.c_str());
    textures.push_back(load_texture(path));
    return true;
  }
  return false;
}

void FloatingDamage::init_ui() {
  clean_ui();  // Just in case releases all resources and clears textures.
  std::string current_ui = (char *)0x63D3C0;
  std::string path = current_ui;
  std::string default_path = "./uifiles/default/";
  for (int i = 1; i <= 3; i++) {
    std::stringstream filepath;
    filepath << path << "gemicons0" << i << ".tga";
    if (add_texture(filepath.str())) continue;
    filepath.str("");
    filepath << default_path << "gemicons0" << i << ".tga";
    if (!add_texture(filepath.str())) Zeal::Game::print_chat("Texture not found: %s", filepath.str().c_str());
  }
}

void FloatingDamage::clean_ui() {
  for (auto &texture : textures)
    if (texture) texture->Release();
  textures.clear();
  bitmap_font.reset();
}

std::vector<std::string> FloatingDamage::get_available_fonts() const {
  auto fonts = BitmapFont::get_available_fonts();
  if (!fonts.empty() && fonts[0] == BitmapFont::kDefaultFontName)  // Default is too small.
    fonts.erase(fonts.begin());
  fonts.insert(fonts.begin(), kUseClientFontString);
  return fonts;
}

// Loads the bitmap font for real-time text rendering to screen.
void FloatingDamage::load_bitmap_font() {
  if (bitmap_font || bitmap_font_filename.get().empty() || bitmap_font_filename.get() == kUseClientFontString) return;

  IDirect3DDevice8 *device = ZealService::get_instance()->dx->GetDevice();
  if (device != nullptr) bitmap_font = BitmapFont::create_bitmap_font(*device, bitmap_font_filename.get());
  if (!bitmap_font) {
    Zeal::Game::print_chat("Failed to load font: %s", bitmap_font_filename.get().c_str());
    bitmap_font_filename.set(kUseClientFontString);  // Disable attempts and use CTexture font.
  } else {
    bitmap_font->set_drop_shadow(true);
  }
}

FloatingDamage::FloatingDamage(ZealService *zeal) {
  // mem::write<BYTE>(0x4A594B, 0x14);
  zeal->callbacks->AddGeneric([this]() { callback_deferred(); }, callback_type::DrawWindows);
  zeal->callbacks->AddGeneric([this]() { callback_render(); }, callback_type::RenderUI);
  zeal->callbacks->AddReportSuccessfulHit(
      [this](Zeal::GameStructures::Entity *source, Zeal::GameStructures::Entity *target, WORD type, short spell_id,
             short damage, char out_text) { add_damage(source, target, type, spell_id, damage, out_text); });
  zeal->callbacks->AddGeneric([this]() { init_ui(); }, callback_type::InitUI);
  zeal->callbacks->AddGeneric([this]() { clean_ui(); }, callback_type::CleanUI);
  zeal->callbacks->AddGeneric([this]() { clean_ui(); }, callback_type::DXReset);  // Just release all resources.

  zeal->callbacks->AddPacket(
      [this](UINT opcode, char *buffer, UINT len) {
        if (opcode == Zeal::Packets::HPUpdate && len >= sizeof(Zeal::Packets::SpawnHPUpdate_Struct))
          handle_hp_update_packet(reinterpret_cast<Zeal::Packets::SpawnHPUpdate_Struct *>(buffer));
        return false;  // continue processing
      },
      callback_type::WorldMessage);

  zeal->commands_hook->Add(
      "/fcd", {}, "Toggles floating combat text or adjusts the fonts with arguments",
      [this](std::vector<std::string> &args) {
        int new_size = 5;
        if (args.size() == 3 && args[1] == "font") {
          bitmap_font_filename.set(args[2]);  // Releases to force a reload.
          Zeal::Game::print_chat("Floating combat font set to %s", bitmap_font_filename.get().c_str());
        } else if (args.size() == 2 && args[1] == "font") {
          auto fonts = BitmapFont::get_available_fonts();
          Zeal::Game::print_chat("Usage: `/fcd font <fontname>` selects the zeal font <fontname>");
          Zeal::Game::print_chat("Available fonts:");
          for (const auto &font : fonts) Zeal::Game::print_chat("  %s", font.c_str());
        } else if (args.size() == 3 && args[1] == "bighit" && Zeal::String::tryParse(args[2], &new_size) &&
                   new_size > 0) {
          big_hit_threshold.set(new_size);
          Zeal::Game::print_chat("Floating combat big hit threshold is now %i", new_size);
        } else if (args.size() == 2 && Zeal::String::tryParse(args[1], &new_size)) {
          font_size = new_size;
          Zeal::Game::print_chat("Floating combat font size is now %i", font_size);
          bitmap_font_filename.set(kUseClientFontString);  // Releases and disables bitmap font path.
        } else if (args.size() == 1) {
          enabled.toggle();
          Zeal::Game::print_chat("Floating combat text is %s", enabled.get() ? "Enabled" : "Disabled");
        } else {
          Zeal::Game::print_chat("Usage: `/fcd` toggles the enable on and off");
          Zeal::Game::print_chat("Usage: `/fcd <#>` selects the client font size (1 to 6)");
          Zeal::Game::print_chat("Usage: `/fcd font` prints the available fonts");
          Zeal::Game::print_chat("Usage: `/fcd font <fontname>` selects the zeal font <fontname>");
          Zeal::Game::print_chat("Usage: `/fcd bighit <threshold>` sets the big hit threshold");
        }

        return true;
      });
}

FloatingDamage::~FloatingDamage() { clean_ui(); }
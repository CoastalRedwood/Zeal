#define NOMINMAX
#include "target_ring.h"

#include "callbacks.h"
#include "commands.h"
#include "hook_wrapper.h"
#include "string_util.h"
#include "ui_skin.h"
#include "zeal.h"

static constexpr char const *kTextureSubDirectoryPath = "targetrings";

IDirect3DDevice8 *TargetRing::UpdateDevice() {
  device = ZealService::get_instance()->dx->GetDevice();
  return device;
}

void TargetRing::LoadTexture() {
  if (!device) return;

  if (texture_buffer) {
    texture_buffer->Release();
    texture_buffer = nullptr;
  }

  if (texture_name.get().empty() || texture_name.get() == "None" || Zeal::Game::get_gamestate() == GAMESTATE_ENTERWORLD)
    return;

  try {
    // Full texture path
    std::filesystem::path texturePath = UISkin::get_zeal_resources_path() /
                                        std::filesystem::path(kTextureSubDirectoryPath) /
                                        std::filesystem::path(texture_name.get() + ".tga");

    // Create texture from file
    HRESULT result = D3DXCreateTextureFromFileA(device, texturePath.string().c_str(), &texture_buffer);
    if (FAILED(result)) {
      texture_buffer = nullptr;
      Zeal::Game::print_chat("Error: Failed to load texture file: %s", texturePath.string().c_str());
      texture_name.set("None");  // Clear to disable future attempts.
      return;
    }
  } catch (const std::exception &ex) {
    Zeal::Game::print_chat("Exception: " + std::string(ex.what()));
    texture_name.set("None");  // Clear to disable future attempts.
  } catch (...) {
    Zeal::Game::print_chat("An unknown error occurred while setting the texture.");
    texture_name.set("None");  // Clear to disable future attempts.
  }
}

void TargetRing::Render() {
  if (!enabled.get() || !Zeal::Game::is_in_game() || (hide_with_gui.get() && !Zeal::Game::is_gui_visible())) return;
  Zeal::GameStructures::Entity *target = Zeal::Game::get_target();
  if (!target || !target->ActorInfo || !target->ActorInfo->ViewActor_) return;
  if (disable_for_self.get() && (target == Zeal::Game::get_self())) return;

  // Looks like we should render a ring, so first acquire and configure all of the fixed resources.
  if (!UpdateDevice()) return;

  if (!texture_buffer) LoadTexture();  // Loads texture if active and needed.

  if (!vertex_buffer && !CreateVertexBuffers()) {
    Release(false);  // Do a full cleanup except texture.
    return;
  }

  if (!index_buffer && !CreateIndexBuffer()) {
    Release(false);  // Do a full cleanup except texture.
    return;
  }

  RenderRing(target);
}

void TargetRing::RenderRing(const Zeal::GameStructures::Entity *target) {
  // Future optimization: Might be able to use material properties to adjust the alpha and
  // color and thus avoid most vertex buffer updates.
  if (!target || !UpdateVertexBuffer(GetColor(target)) || (num_indices <= 2)) return;

  // Configure with our rendering settings while preserving state to restore.
  D3DRenderStateStash render_state(*device);
  render_state.store_and_modify({D3DRS_CULLMODE, D3DCULL_NONE});  // Future optimization.
  render_state.store_and_modify({D3DRS_ALPHABLENDENABLE, TRUE});
  render_state.store_and_modify({D3DRS_SRCBLEND, D3DBLEND_SRCALPHA});
  render_state.store_and_modify({D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA});
  render_state.store_and_modify({D3DRS_BLENDOP, D3DBLENDOP_ADD});
  render_state.store_and_modify({D3DRS_ZENABLE, TRUE});
  render_state.store_and_modify({D3DRS_ZWRITEENABLE, TRUE});
  render_state.store_and_modify({D3DRS_LIGHTING, FALSE});

  D3DTextureStateStash texture_state(*device);
  texture_state.store_and_modify({D3DTSS_COLOROP, D3DTOP_MODULATE});  // Mix color with white font.
  texture_state.store_and_modify({D3DTSS_COLORARG1, D3DTA_TEXTURE});
  texture_state.store_and_modify({D3DTSS_COLORARG2, D3DTA_DIFFUSE});
  texture_state.store_and_modify({D3DTSS_ALPHAOP, D3DTOP_MODULATE});  // Support color alpha.
  texture_state.store_and_modify({D3DTSS_ALPHAARG1, D3DTA_TEXTURE});
  texture_state.store_and_modify({D3DTSS_ALPHAARG2, D3DTA_DIFFUSE});

  D3DXMATRIX originalWorldMatrix;
  device->GetTransform(D3DTS_WORLD, &originalWorldMatrix);  // Stashing this for restoration.

  D3DXMATRIX rotation_matrix;
  float rotation_angle = GetRotationAngle(target);
  D3DXMatrixRotationZ(&rotation_matrix, rotation_angle);
  D3DXMATRIX translation_matrix;
  D3DXMatrixTranslation(&translation_matrix, target->Position.x, target->Position.y, target->ActorInfo->Z);
  D3DXMATRIX world_matrix = rotation_matrix * translation_matrix;
  device->SetTransform(D3DTS_WORLD, &world_matrix);

  // Note: Not preserving shader, texture, source, or indices to avoid reference counting.
  device->SetVertexShader(DiffuseVertex::kFvfCode);
  device->SetTexture(0, NULL);  // Ensure no texture is bound for drawing solid cylinder.
  device->SetStreamSource(0, vertex_buffer, sizeof(DiffuseVertex));

  device->SetIndices(index_buffer, 0);
  int end_indices = use_cone.get() ? num_indices - 2 : top_ring_num_indices - 2;
  device->DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP, 0, 4 * num_per_circle, 0, end_indices);

  if (texture_buffer && texture_vertex_buffer) {
    device->SetVertexShader(TextureVertex::kFvfCode);
    device->SetTexture(0, texture_buffer);
    device->SetStreamSource(0, texture_vertex_buffer, sizeof(TextureVertex));
    device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, texture_vertices.size() - 2);
  }

  // Restore D3D state.
  device->SetTexture(0, NULL);          // Unbind any textures.
  device->SetStreamSource(0, NULL, 0);  // Unbind vertex buffer.
  device->SetIndices(NULL, 0);          // Unbind index buffer.
  device->SetTransform(D3DTS_WORLD, &originalWorldMatrix);
  texture_state.restore_state();
  render_state.restore_state();
}

D3DCOLOR TargetRing::GetTargetIndexColor() const {
  const int kTargetColorIndex = 18;  // NamePlate::ColorIndex::Target
  if (get_color_callback) return get_color_callback(kTargetColorIndex);
  return 0xFFFFFFFF;  // Default to solid white.
}

D3DCOLOR TargetRing::GetColor(const Zeal::GameStructures::Entity *target) const {
  // The color tinting is either the assigned target color or the level consider color.
  D3DCOLOR color = target_color.get() ? GetTargetIndexColor() : Zeal::Game::GetLevelCon(target);

  // Add in user selected alpha.
  float trp = 1.0f - transparency.get();
  DWORD alpha = static_cast<BYTE>(std::max(0.01f, std::min(1.0f, trp)) * 255);
  color = (alpha << 24) | (color & 0x00ffffff);

  // Modulate (fading) the color if autoattack is active and the blinking indicator is enabled.
  if (attack_indicator.get()) {
    // The fade_factor will return 1.0f if autoattack is disabled.
    float fade_factor = Zeal::Game::get_target_blink_fade_factor(flash_speed.get(), true);
    if (fade_factor < 1.0f) {
      BYTE faded_red = static_cast<BYTE>(((color >> 16) & 0xFF) * fade_factor);
      BYTE faded_green = static_cast<BYTE>(((color >> 8) & 0xFF) * fade_factor);
      BYTE faded_blue = static_cast<BYTE>((color & 0xFF) * fade_factor);
      color = D3DCOLOR_ARGB(alpha, faded_red, faded_green, faded_blue);
    }
  }

  return color;
}

float TargetRing::GetRotationAngle(const Zeal::GameStructures::Entity *target) const {
  if (!target || !texture_buffer) return 0;

  static constexpr float kMathPi = static_cast<float>(M_PI);
  if (rotate_match_heading.get()) {
    float rotation_angle = static_cast<float>(target->Heading * kMathPi / 256);
    return rotation_angle;
  }

  // Base rotation period = full rotation every 6 seconds.
  static constexpr float base_rotation_period_ms = 6.0f * 1000.f;
  float rotation_scale_factor = rotation_speed.get();
  if (rotation_scale_factor == 0) return 0;
  float rotation_period_ms =
      std::max(-3600 * 1000.f, std::min(3600 * 1000.f, base_rotation_period_ms / rotation_scale_factor));

  // Use integer math for a precise modulo result even with large 64-bit numbers.
  unsigned int rotation_fraction_ms = static_cast<long long>(GetTickCount64()) % static_cast<int>(rotation_period_ms);
  float rotation_angle = 2 * kMathPi * rotation_fraction_ms / rotation_period_ms;
  return rotation_angle;
}

bool TargetRing::CreateVertexBuffers() {
  num_per_circle = std::max(32, num_segments.get());
  int num_ring_vertices = 4 * num_per_circle;  // Outer and inner times upper and lower.

  // The rings consists of the top and bottom rings which have inner and outer circles.
  const auto color = D3DCOLOR_ARGB(0, 0, 0, 0);  // Set to transparent black for change detect.
  const float radius = outer_size.get();
  const float inner_radius = std::clamp(radius - (radius * inner_percent.get()), 1.f, 100.f);
  const float outer_radius = radius;

  const float top_height = 0.3f;
  const float bottom_height = 0;
  const float angle_step = 2.0f * static_cast<float>(M_PI) / num_per_circle;
  vertices.clear();
  vertices.resize(4 * num_per_circle);
  for (int i = 0; i < num_per_circle; ++i) {
    float angle = (i * angle_step);
    float cos_angle = cosf(angle);
    float sin_angle = sinf(angle);
    float v = 1.0f - static_cast<float>(i) / num_per_circle;

    // Inner circles.
    vertices[i + 0 * num_per_circle] = {
        .x = inner_radius * cos_angle, .y = inner_radius * sin_angle, .z = top_height, .color = color};
    vertices[i + 1 * num_per_circle] = vertices[i + 0 * num_per_circle];
    vertices[i + 1 * num_per_circle].z = bottom_height;

    // Outer circles.
    vertices[i + 2 * num_per_circle] = {
        .x = outer_radius * cos_angle, .y = outer_radius * sin_angle, .z = top_height, .color = color};
    vertices[i + 3 * num_per_circle] = vertices[i + 2 * num_per_circle];
    vertices[i + 3 * num_per_circle].z = bottom_height;
  }

  int buffer_size = vertices.size() * sizeof(vertices[0]);
  if (FAILED(device->CreateVertexBuffer(buffer_size, D3DUSAGE_WRITEONLY, DiffuseVertex::kFvfCode, D3DPOOL_DEFAULT,
                                        &vertex_buffer))) {
    vertex_buffer = nullptr;  // Ensure nullptr.
    return false;
  }

  texture_vertices.clear();
  if (texture_buffer) {
    for (int i = 0; i < num_per_circle; ++i) {
      float v = 1.0f - static_cast<float>(i) / num_per_circle;
      texture_vertices.emplace_back(TextureVertex(vertices[i], 0.0f, v));                       // Inner circle.
      texture_vertices.emplace_back(TextureVertex(vertices[i + 2 * num_per_circle], 1.0f, v));  // Outer circle.
    }
    texture_vertices.emplace_back(TextureVertex(vertices[0], 0.0f, 0.0f));  // Close out the strip.
    texture_vertices.emplace_back(TextureVertex(vertices[2 * num_per_circle], 1.0f, 0.0f));

    int buffer_size = texture_vertices.size() * sizeof(texture_vertices[0]);
    if (FAILED(device->CreateVertexBuffer(buffer_size, D3DUSAGE_WRITEONLY, TextureVertex::kFvfCode, D3DPOOL_DEFAULT,
                                          &texture_vertex_buffer))) {
      texture_vertex_buffer = nullptr;  // Ensure nullptr.
      return false;
    }

    // The texture buffer is not dependent on color, so it can be a static, fixed vertex buffer. Copy over now.
    uint8_t *locked_buffer = nullptr;
    if (FAILED(texture_vertex_buffer->Lock(0, 0, &locked_buffer, D3DLOCK_DISCARD))) {
      return false;
    }

    memcpy(locked_buffer, texture_vertices.data(), buffer_size);
    texture_vertex_buffer->Unlock();
  }

  return UpdateVertexBuffer(D3DCOLOR_XRGB(0xff, 0xff, 0xff));  // Set to solid white to trigger update w/copy.
}

// Copies over the pre-calculated vertices with updated color and transparency to the vertex_buffer.
bool TargetRing::UpdateVertexBuffer(D3DCOLOR color) {
  if (vertices.empty() || !vertex_buffer) return false;

  if (vertices[0].color == color) return true;  // Skip updating if the color already matches.
  for (auto &vertex : vertices) {
    vertex.color = color;
  }

  uint8_t *locked_buffer = nullptr;
  if (FAILED(vertex_buffer->Lock(0, 0, &locked_buffer, D3DLOCK_DISCARD))) {
    return false;
  }

  int buffer_size = vertices.size() * sizeof(vertices[0]);
  memcpy(locked_buffer, vertices.data(), buffer_size);
  vertex_buffer->Unlock();

  return true;
}

// The diffuse vertices use an index buffer to re-use the vertices.
bool TargetRing::CreateIndexBuffer() {
  if (index_buffer) return true;

  // Calculate the fixed pattern that maps the pre-calculated vertices to the cylinder.
  std::vector<int16_t> indices;

  // Top filled circle meshing inner and outer radius.
  for (size_t i = 0; i < num_per_circle; ++i) {
    indices.push_back(2 * num_per_circle + i);  // Top outer radius
    indices.push_back(0 * num_per_circle + i);  // Top inner radius.
  }
  indices.push_back(2 * num_per_circle);      // Close out top circle.
  indices.push_back(0 * num_per_circle);      // Repeated below to close out.
  top_ring_num_indices = indices.size() + 1;  // Include the repeat below.

  // Draw the inner cylinder walls.
  for (size_t i = 0; i < num_per_circle; ++i) {
    indices.push_back(0 * num_per_circle + i);  // Top inner radius
    indices.push_back(1 * num_per_circle + i);  // Bottom innner radius.
  }
  indices.push_back(0 * num_per_circle);  // Close out inner wall.
  indices.push_back(1 * num_per_circle);  // Repeated below to close out.

  // Bottom filled circle meshing inner and outer radius.
  for (size_t i = 0; i < num_per_circle; ++i) {
    indices.push_back(i + 1 * num_per_circle);  // Bottom inner radius
    indices.push_back(i + 3 * num_per_circle);  // Bottom outer radius.
  }
  indices.push_back(1 * num_per_circle);  // Close out bottom circle.
  indices.push_back(3 * num_per_circle);  // Repeated below to close out.

  // Draw the outer cylinder walls.
  for (size_t i = 0; i < num_per_circle; ++i) {
    indices.push_back(3 * num_per_circle + i);  // Bottom outer radius.
    indices.push_back(2 * num_per_circle + i);  // Top outer radius.
  }
  indices.push_back(3 * num_per_circle);  // Close out outer wall.
  indices.push_back(2 * num_per_circle);  // Repeated below to close out.
  indices.push_back(2 * num_per_circle);

  int buffer_size = indices.size() * sizeof(indices[0]);
  if (FAILED(device->CreateIndexBuffer(buffer_size, 0, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &index_buffer))) {
    index_buffer = nullptr;  // Ensure nullptr.
    return false;
  }

  uint8_t *locked_buffer = nullptr;
  if (FAILED(index_buffer->Lock(0, 0, &locked_buffer, D3DLOCK_DISCARD))) {
    return false;
  }

  memcpy(locked_buffer, indices.data(), buffer_size);
  index_buffer->Unlock();
  num_indices = indices.size();
  return true;
}

static std::vector<std::string> GetTGAFiles(const std::filesystem::path &directoryPath) {
  std::vector<std::string> tgaFiles;

  // Iterate over the directory
  for (const auto &entry : std::filesystem::directory_iterator(directoryPath)) {
    // Check if it's a file and has a .tga extension
    if (entry.is_regular_file() && entry.path().extension() == ".tga") {
      // Get the filename without extension and add to the vector
      tgaFiles.push_back(entry.path().stem().string());
    }
  }

  return tgaFiles;
}

std::vector<std::string> TargetRing::get_available_textures() const {
  std::filesystem::path texturePath =
      UISkin::get_zeal_resources_path() / std::filesystem::path(kTextureSubDirectoryPath);
  std::vector<std::string> tgas = GetTGAFiles(texturePath);
  if (tgas.empty()) Zeal::Game::print_chat("Warning: no texture files found at: %s", texturePath.c_str());
  tgas.insert(tgas.begin(), "None");
  return tgas;
}

void TargetRing::Release(bool release_texture) {
  if (vertex_buffer) vertex_buffer->Release();
  vertex_buffer = nullptr;
  if (index_buffer) index_buffer->Release();
  index_buffer = nullptr;
  num_indices = 0;
  top_ring_num_indices = 0;
  vertices.clear();

  if (texture_vertex_buffer) texture_vertex_buffer->Release();
  texture_vertex_buffer = nullptr;
  texture_vertices.clear();

  device = nullptr;

  num_per_circle = 0;

  if (release_texture) {
    if (texture_buffer) texture_buffer->Release();
    texture_buffer = nullptr;
  }
}

TargetRing::TargetRing(ZealService *zeal) {
  zeal->callbacks->AddGeneric([this]() { Render(); }, callback_type::RenderUI);
  zeal->callbacks->AddGeneric([this]() { Release(); }, callback_type::CleanUI);
  zeal->callbacks->AddGeneric([this]() { Release(); }, callback_type::DXReset);  // Just release all resources.
  zeal->callbacks->AddGeneric([this]() { Release(); }, callback_type::DXCleanDevice);

  zeal->commands_hook->Add("/targetring", {}, "Toggles target ring", [this](std::vector<std::string> &args) {
    if (args.size() == 2) {
      if (args[1] == "indicator") {
        attack_indicator.toggle();
      } else {
        float pct = 0;
        if (Zeal::String::tryParse(args[1], &pct)) inner_percent.set(pct);
      }
    } else {
      enabled.toggle();
      Zeal::Game::print_chat("Target ring is %s", enabled.get() ? "Enabled" : "Disabled");
    }
    if (update_options_ui_callback) update_options_ui_callback();
    return true;
  });
}

TargetRing::~TargetRing() { Release(); }

#pragma once
#include <Windows.h>

#include "directx.h"
#include "game_functions.h"
#include "zeal_settings.h"

class TargetRing {
 public:
  TargetRing(class ZealService *zeal);
  ~TargetRing();

  // Returns a list of available textures for mapping onto the ring.
  std::vector<std::string> get_available_textures() const;

  // Registers a callback that executes when the command line interface makes a setting change.
  void add_options_callback(std::function<void()> callback) { update_options_ui_callback = callback; };

  // Registers a callback used to translate a specific color index into a D3DCOLOR value.
  void add_get_color_callback(std::function<unsigned int(int index)> callback) { get_color_callback = callback; };

  ZealSetting<bool> enabled = {false, "TargetRing", "Enabled", true};
  ZealSetting<bool> hide_with_gui = {false, "TargetRing", "HideWithGui", true};
  ZealSetting<bool> disable_for_self = {false, "TargetRing", "DisableForSelf", true};
  ZealSetting<bool> attack_indicator = {false, "TargetRing", "AttackIndicator", true};
  ZealSetting<bool> rotate_match_heading = {false, "TargetRing", "MatchHeading", true};
  ZealSetting<bool> use_cone = {false, "TargetRing", "Cone", true};
  ZealSetting<bool> target_color = {false, "TargetRing", "TargetColor", true};
  ZealSetting<float> inner_percent = {0.50f, "TargetRing", "InnerSize", true, [this](float) { Release(false); }};
  ZealSetting<float> outer_size = {10.0f, "TargetRing", "Size", true, [this](float) { Release(false); }};
  ZealSetting<float> rotation_speed = {1.0f, "TargetRing", "RotateSpeed", true};
  ZealSetting<float> flash_speed = {1.0f, "TargetRing", "FlashSpeed", true};
  ZealSetting<float> transparency = {0.7f, "TargetRing", "Transparency", true};
  ZealSetting<int> num_segments = {128, "TargetRing", "Segments", true, [this](int) { Release(false); }};
  ZealSetting<std::string> texture_name = {"None", "TargetRing", "Texture", true,
                                           [this](std::string name) { Release(); }};

 private:
  struct DiffuseVertex {
    static constexpr DWORD kFvfCode = (D3DFVF_XYZ | D3DFVF_DIFFUSE);

    float x, y, z;   // Position coordinates
    D3DCOLOR color;  // Color for solid vertices
  };

  struct TextureVertex {
    static constexpr DWORD kFvfCode = (D3DFVF_XYZ | D3DFVF_TEX1);

    float x, y, z;  // Position coordinates
    float u, v;     // Texture coordinates

    explicit TextureVertex(const DiffuseVertex &dv, float u, float v) : x(dv.x), y(dv.y), z(dv.z + 0.01f), u(u), v(v){};
  };

  void Release(bool release_texture = true);
  void Render();
  void RenderRing(const Zeal::GameStructures::Entity *target);
  IDirect3DDevice8 *UpdateDevice();
  bool CreateVertexBuffers();
  bool CreateIndexBuffer();
  bool UpdateVertexBuffer(D3DCOLOR color);
  void LoadTexture();

  float GetRotationAngle(const Zeal::GameStructures::Entity *target) const;
  D3DCOLOR GetColor(const Zeal::GameStructures::Entity *target) const;
  D3DCOLOR GetTargetIndexColor() const;

  std::vector<DiffuseVertex> vertices;                      // CPU cache of calculated vertices.
  std::vector<TextureVertex> texture_vertices;              // CPU cache of calculated vertices.
  IDirect3DDevice8 *device = nullptr;                       // Local copy of D3D device.
  IDirect3DVertexBuffer8 *vertex_buffer = nullptr;          // Primary color changing cylinder vertices.
  IDirect3DIndexBuffer8 *index_buffer = nullptr;            // Triangle strip indicing of cylinder.
  IDirect3DTexture8 *texture_buffer = nullptr;              // Loaded texture resource.
  IDirect3DVertexBuffer8 *texture_vertex_buffer = nullptr;  // Fixed texture to top ring mapping vertices.

  int num_per_circle = 0;        // Clamped value of num_segments.
  int num_indices = 0;           // Number of indices in triangle strip index buffer.
  int top_ring_num_indices = 0;  // Top ring is at start of index buffer.

  std::function<void()> update_options_ui_callback;
  std::function<unsigned int(int)> get_color_callback;
};

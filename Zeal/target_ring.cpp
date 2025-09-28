#define NOMINMAX
#include "target_ring.h"

#include "callbacks.h"
#include "commands.h"
#include "hook_wrapper.h"
#include "string_util.h"
#include "ui_skin.h"
#include "zeal.h"

#define NUM_VERTICES 4

static constexpr char const *kTextureSubDirectoryPath = "targetrings";

D3DCOLOR TargetRing::get_target_color() const {
  const int kTargetColorIndex = 18;  // NamePlate::ColorIndex::Target
  if (get_color_callback) return get_color_callback(kTargetColorIndex);
  return 0xFFFFFFFF;  // Default to solid white.
}

RenderState::RenderState(IDirect3DDevice8 *device, DWORD state, DxStateType_ type) : state(state), type(type) {
  if (type == DxStateType_::DxStateType_Render) {
    device->GetRenderState((D3DRENDERSTATETYPE)state, &value);
  } else if (type == DxStateType_::DxStateType_Texture) {
    device->GetTextureStageState(0, (D3DTEXTURESTAGESTATETYPE)state, &value);
  }
}

void TargetRing::setup_render_states() {
  IDirect3DDevice8 *device = ZealService::get_instance()->dx->GetDevice();
  if (!device) return;
  render_states.clear();
  render_states.push_back({device, (DWORD)D3DRS_ALPHABLENDENABLE, DxStateType_Render});
  render_states.push_back({device, (DWORD)D3DRS_SRCBLEND, DxStateType_Render});
  render_states.push_back({device, (DWORD)D3DRS_DESTBLEND, DxStateType_Render});
  render_states.push_back({device, (DWORD)D3DRS_CULLMODE, DxStateType_Render});
  render_states.push_back({device, (DWORD)D3DRS_ZENABLE, DxStateType_Render});
  render_states.push_back({device, (DWORD)D3DRS_ZWRITEENABLE, DxStateType_Render});
  render_states.push_back({device, (DWORD)D3DRS_LIGHTING, DxStateType_Render});
  render_states.push_back({device, (DWORD)D3DTSS_COLOROP, DxStateType_Texture});
  render_states.push_back({device, (DWORD)D3DTSS_COLORARG1, DxStateType_Texture});
  render_states.push_back({device, (DWORD)D3DTSS_ALPHAOP, DxStateType_Texture});
  render_states.push_back({device, (DWORD)D3DTSS_ALPHAARG1, DxStateType_Texture});
  render_states.push_back({device, (DWORD)D3DRS_BLENDOP, DxStateType_Render});
  render_states.push_back({device, (DWORD)D3DRS_TEXTUREFACTOR, DxStateType_Render});
  device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
  device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
  device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
  device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
  device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
  device->SetRenderState(D3DRS_ZENABLE, TRUE);
  device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
  device->SetRenderState(D3DRS_LIGHTING, FALSE);

  // Set texture stage states
  device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
  device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
  device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
  device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
  device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
  device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
}

void TargetRing::reset_render_states() {
  IDirect3DDevice8 *device = ZealService::get_instance()->dx->GetDevice();
  for (auto &state : render_states) {
    if (state.type == DxStateType_Render)
      device->SetRenderState((D3DRENDERSTATETYPE)state.state, state.value);
    else if (state.type == DxStateType_Texture)
      device->SetTextureStageState(0, (D3DTEXTURESTAGESTATETYPE)state.state, state.value);
  }
}

void TargetRing::load_texture(const std::string &filename) {
  try {
    if (targetRingTexture) targetRingTexture->Release();

    targetRingTexture = nullptr;

    if (!filename.length() || filename == "None" || Zeal::Game::get_gamestate() == GAMESTATE_ENTERWORLD) return;

    // Full texture path
    std::filesystem::path texturePath = UISkin::get_zeal_resources_path() /
                                        std::filesystem::path(kTextureSubDirectoryPath) /
                                        std::filesystem::path(filename + ".tga");
    // Get the Direct3D device
    IDirect3DDevice8 *device = ZealService::get_instance()->dx->GetDevice();
    if (!device) {
      Zeal::Game::print_chat("Error: Failed to get Direct3D device.");
      return;
    }

    // Create texture from file
    HRESULT result = D3DXCreateTextureFromFileA(device, texturePath.string().c_str(), &targetRingTexture);
    if (FAILED(result)) {
      targetRingTexture = nullptr;
      Zeal::Game::print_chat("Error: Failed to load texture file: %s", texturePath.string().c_str());
      return;
    }
  } catch (const std::exception &ex) {
    Zeal::Game::print_chat("Exception: " + std::string(ex.what()));
  } catch (...) {
    Zeal::Game::print_chat("An unknown error occurred while setting the texture.");
  }
}

template <typename VertexType>
LPDIRECT3DVERTEXBUFFER8 CreateVertexBuffer(LPDIRECT3DDEVICE8 d3dDevice, VertexType *vertices, int vertexCount,
                                           DWORD fvf) {
  LPDIRECT3DVERTEXBUFFER8 vertexBuffer = nullptr;

  // Create the vertex buffer
  HRESULT result = d3dDevice->CreateVertexBuffer(vertexCount * sizeof(VertexType),  // Size of the buffer
                                                 D3DUSAGE_WRITEONLY,                // Usage flags
                                                 fvf,                               // Flexible Vertex Format (FVF)
                                                 D3DPOOL_DEFAULT,                   // Memory pool to use
                                                 &vertexBuffer                      // Output vertex buffer
  );

  if (FAILED(result)) {
    // Handle error
    return nullptr;
  }

  // Lock the vertex buffer and copy the vertex data into it
  void *pVertices;
  vertexBuffer->Lock(0, vertexCount * sizeof(VertexType), (BYTE **)&pVertices, 0);
  memcpy(pVertices, vertices, vertexCount * sizeof(VertexType));
  vertexBuffer->Unlock();

  return vertexBuffer;
}

//// Collide with world to get terrain height (this looks great on slopes but steep drop offs and walls are terrible)
// Zeal::Game::collide_with_world({ pos.x + outerRadiusVertex.x, pos.y + outerRadiusVertex.y, pos.z + (size * 2) },
//	{ pos.x + outerRadiusVertex.x, pos.y + outerRadiusVertex.y, pos.z - (size * 2) },
//	outerTerrainHeight);
// Zeal::Game::collide_with_world({ pos.x + innerRadiusVertex.x, pos.y + innerRadiusVertex.y, pos.z + (size * 2) },
//	{ pos.x + innerRadiusVertex.x, pos.y + innerRadiusVertex.y, pos.z - (size * 2) },
//	innerTerrainHeight);

// void TargetRing::render_ring_section(IDirect3DDevice8* device, TextureVertex* vertices, int vertexCount,
// IDirect3DTexture8* texture) { 			D3DXMatrixTranslation(&worldMatrix, pos.x, pos.y, pos.z +
// zOffset);
//			device->SetTransform(D3DTS_WORLD, &worldMatrix);
//
//			device->SetTexture(0, 0);
//			device->SetStreamSource(0, solidVertexBuffer, sizeof(Vertex));
//			device->SetVertexShader(D3DFVF_XYZ | D3DFVF_DIFFUSE);
//			device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, vertex_count - 2);
//
//			if (texture) {
//				// Render the texture
//				LPDIRECT3DVERTEXBUFFER8 texturedVertexBuffer = CreateVertexBuffer(device,
// texture_vertices, vertex_count, D3DFVF_XYZ | D3DFVF_TEX1); 				device->SetTexture(0, texture);
// device->SetStreamSource(0, texturedVertexBuffer, sizeof(TextureVertex));
// device->SetVertexShader(D3DFVF_XYZ | D3DFVF_TEX1);
// device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, vertex_count - 2);
//				texturedVertexBuffer->Release();
//			}
// }
// void TargetRing::render_cylinder(IDirect3DDevice8* device, Vec3 pos, float radius, float topZ, float bottomZ, DWORD
// color) {
//	// Create cylinder vertices based on the radius and Z positions
//	const int cylinderSegments = num_segments; // You can adjust the number of segments as needed
//	Vertex* cylinderVertices = new Vertex[cylinderSegments * 2 + 2]; // Top and bottom
//
//	for (int i = 0; i <= cylinderSegments; ++i) {
//		float angle = (i * 2.0f * D3DX_PI) / cylinderSegments;
//
//		// Top circle vertices
//		cylinderVertices[i].x = radius * cosf(angle);
//		cylinderVertices[i].y = radius * sinf(angle);
//		cylinderVertices[i].z = topZ; // Position at top
//
//		// Bottom circle vertices
//		cylinderVertices[i + cylinderSegments + 1].x = radius * cosf(angle);
//		cylinderVertices[i + cylinderSegments + 1].y = radius * sinf(angle);
//		cylinderVertices[i + cylinderSegments + 1].z = bottomZ; // Position at bottom
//
//		// Set color
//		cylinderVertices[i].color = color;
//		cylinderVertices[i + cylinderSegments + 1].color = color;
//	}
//
//	// Render cylinder using triangle strips
//	// Use a similar approach as the render_ring_section function to create a vertex buffer and draw the cylinder
//	// ...
//
//	delete[] cylinderVertices;
// }

// void TargetRing::render_ring(Vec3 pos, float size, DWORD color, IDirect3DTexture8* texture, float rotationAngle) {
//	IDirect3DDevice8* device = ZealService::get_instance()->dx->GetDevice();
//	if (!device)
//		return;
//
//	const float innerRadius = std::clamp(size - (size * inner_percent), 0.f, 100.f);
//	const float outerRadius = size;
//	const float angleStep = 2.0f * static_cast<float>(M_PI) / num_segments;  // Angle step for ring segments
//
//	// Create vertices for the upper and lower rings
//	TextureVertex* upperRingVertices = new TextureVertex[num_segments + 1];
//	TextureVertex* lowerRingVertices = new TextureVertex[num_segments + 1];
//	float ringThickness = 0.5f;
//	for (int i = 0; i <= num_segments; ++i) {
//		float angle = (i * angleStep) + rotationAngle;
//
//		// Upper ring vertices
//		upperRingVertices[i].x = outerRadius * cosf(angle);
//		upperRingVertices[i].y = outerRadius * sinf(angle);
//		upperRingVertices[i].z = pos.z + ringThickness; // Position the upper ring above
//		upperRingVertices[i].color = color;
//		upperRingVertices[i].u = 1.0f; // Set texture coordinates as needed
//		upperRingVertices[i].v = 1.0f - (float)i / (float)num_segments;
//
//		// Lower ring vertices
//		lowerRingVertices[i].x = outerRadius * cosf(angle);
//		lowerRingVertices[i].y = outerRadius * sinf(angle);
//		lowerRingVertices[i].z = pos.z - ringThickness; // Position the lower ring below
//		lowerRingVertices[i].color = color;
//		lowerRingVertices[i].u = 1.0f; // Set texture coordinates as needed
//		lowerRingVertices[i].v = 1.0f - (float)i / (float)num_segments;
//	}
//	setup_render_states();
//
//	// Render the upper ring
//	render_ring_section(device, upperRingVertices, num_segments + 1, texture);
//
//	// Render the lower ring
//	render_ring_section(device, lowerRingVertices, num_segments + 1, texture);
//
//	// Render the outer cylinder
//	render_cylinder(device, pos, outerRadius, pos.z + ringThickness, pos.z - ringThickness, color);
//
//	// Render the inner cylinder
//	render_cylinder(device, pos, innerRadius, pos.z + ringThickness, pos.z - ringThickness, color);
//
//	// Clean up
//	delete[] upperRingVertices;
//	delete[] lowerRingVertices;
// }
void TargetRing::drawVertices(Vec3 pos, DWORD vertex_count, IDirect3DTexture8 *texture, D3DXMATRIX worldMatrix,
                              SolidVertex *solid_vertices, TexturedVertex *texture_vertices) {
  IDirect3DDevice8 *device = ZealService::get_instance()->dx->GetDevice();
  if (!device) return;
  LPDIRECT3DVERTEXBUFFER8 solidVertexBuffer =
      CreateVertexBuffer(device, solid_vertices, vertex_count, D3DFVF_XYZ | D3DFVF_DIFFUSE);
  D3DXMatrixTranslation(&worldMatrix, pos.x, pos.y, pos.z);
  device->SetTransform(D3DTS_WORLD, &worldMatrix);

  device->SetTexture(0, 0);
  device->SetStreamSource(0, solidVertexBuffer, sizeof(SolidVertex));
  device->SetVertexShader(D3DFVF_XYZ | D3DFVF_DIFFUSE);
  device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, vertex_count - 2);
  device->SetStreamSource(0, NULL, 0);  // Unbind vertex buffer before Release.
  solidVertexBuffer->Release();
  if (texture) {
    LPDIRECT3DVERTEXBUFFER8 texturedVertexBuffer =
        CreateVertexBuffer(device, texture_vertices, vertex_count, D3DFVF_XYZ | D3DFVF_TEX1);
    device->SetTexture(0, texture);
    device->SetStreamSource(0, texturedVertexBuffer, sizeof(TexturedVertex));
    device->SetVertexShader(D3DFVF_XYZ | D3DFVF_TEX1);
    device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, vertex_count - 2);
    device->SetTexture(0, NULL);          // Release reference to texture.
    device->SetStreamSource(0, NULL, 0);  // Unbind vertex buffer before Release.
    texturedVertexBuffer->Release();
  }
}

void generate_vertices(SolidVertex *ringVertices, SolidVertex *outer_cylinderVertices,
                       SolidVertex *inner_cylinderVertices, TexturedVertex *textureVertices, int num_segments,
                       float radius, float inner_percent, float rotationAngle, DWORD color, float height) {
  float trp = 1.0f - ZealService::get_instance()->target_ring->transparency.get();  // 50% transparency
  trp = std::max(0.01f, std::min(1.0f, trp));
  BYTE alpha = static_cast<BYTE>(trp * 255);              // Convert to alpha byte
  DWORD newColor = (color & 0x00FFFFFF) | (alpha << 24);  // Apply alpha

  const float innerRadius = std::clamp(radius - (radius * inner_percent), 0.f, 100.f);
  const float outerRadius = radius;
  const float angleStep = 2.0f * static_cast<float>(M_PI) / num_segments;  // Fixed truncation warning
  int vertexIndex = 0;
  int cylinderIndex = 0;
  int cylinderIndex2 = 0;
  for (int i = 0; i <= num_segments; ++i) {
    float angle = (i * angleStep) + rotationAngle;
    Vec2 outerRadiusVertex = {outerRadius * cosf(angle), outerRadius * sinf(angle)};
    Vec2 innerRadiusVertex = {innerRadius * cosf(angle), innerRadius * sinf(angle)};

    // Outer circle vertices
    textureVertices[vertexIndex].x = outerRadiusVertex.x;
    textureVertices[vertexIndex].y = outerRadiusVertex.y;
    textureVertices[vertexIndex].z = 0.f;  // outerTerrainHeight.z - pos.z;
    textureVertices[vertexIndex].color = newColor;
    textureVertices[vertexIndex].u = 1.0f;
    textureVertices[vertexIndex].v = 1.0f - (float)i / (float)num_segments;
    ringVertices[vertexIndex] = SolidVertex(textureVertices[vertexIndex]);
    outer_cylinderVertices[cylinderIndex++] = SolidVertex(textureVertices[vertexIndex], height);
    outer_cylinderVertices[cylinderIndex++] = SolidVertex(textureVertices[vertexIndex], 0);
    vertexIndex++;

    // Inner circle vertices
    textureVertices[vertexIndex].x = innerRadiusVertex.x;
    textureVertices[vertexIndex].y = innerRadiusVertex.y;
    textureVertices[vertexIndex].z = 0.f;  // innerTerrainHeight.z - pos.z;
    textureVertices[vertexIndex].color = newColor;
    textureVertices[vertexIndex].u = 0.0f;
    textureVertices[vertexIndex].v = 1.0f - (float)i / (float)num_segments;
    ringVertices[vertexIndex] = SolidVertex(textureVertices[vertexIndex]);
    inner_cylinderVertices[cylinderIndex2++] = SolidVertex(textureVertices[vertexIndex], height);
    inner_cylinderVertices[cylinderIndex2++] = SolidVertex(textureVertices[vertexIndex], 0);
    vertexIndex++;
  }
}

void generate_cylinder_vertices(SolidVertex *solidVertices, int num_segments, float radius, float height, DWORD color) {
  const float angleStep = 2.0f * D3DX_PI / num_segments;
  int vertexIndex = 0;

  for (int i = 0; i <= num_segments; ++i) {
    float angle = i * angleStep;

    // Calculate positions for the top and bottom vertices
    float x = radius * cosf(angle);
    float y = radius * sinf(angle);

    // Top vertex
    solidVertices[vertexIndex].x = x;
    solidVertices[vertexIndex].y = y;
    solidVertices[vertexIndex].z = height;  // Top
    solidVertices[vertexIndex].color = color;
    vertexIndex++;

    // Bottom vertex
    solidVertices[vertexIndex].x = x;
    solidVertices[vertexIndex].y = y;
    solidVertices[vertexIndex].z = 0.0f;  // Bottom
    solidVertices[vertexIndex].color = color;
    vertexIndex++;
  }
}

void TargetRing::render_ring(Vec3 pos, float radius, DWORD color, IDirect3DTexture8 *texture, float rotationAngle) {
  IDirect3DDevice8 *device = ZealService::get_instance()->dx->GetDevice();
  if (!device) return;
  D3DXMATRIX worldMatrix, originalWorldMatrix, rotationMatrix;
  const float height = 0.3f;
  const DWORD ring_vertex_count = num_segments.get() * 2 + 2;
  const DWORD cylinder_vertex_count = num_segments.get() * 2 + 2;
  TexturedVertex *texture_vertices = new TexturedVertex[ring_vertex_count];
  SolidVertex *solid_vertices = new SolidVertex[ring_vertex_count];
  SolidVertex *outer_cylinder_vertices = new SolidVertex[cylinder_vertex_count];
  SolidVertex *inner_cylinder_vertices = new SolidVertex[cylinder_vertex_count];
  generate_vertices(solid_vertices, outer_cylinder_vertices, inner_cylinder_vertices, texture_vertices,
                    num_segments.get(), radius, inner_percent.get(), rotationAngle, color, height);
  // generate_cylinder_vertices(solid_vertices, num_segments, size, height, color);

  setup_render_states();
  device->GetTransform(D3DTS_WORLD, &originalWorldMatrix);
  drawVertices({pos.x, pos.y, pos.z + height}, ring_vertex_count, texture, worldMatrix, solid_vertices,
               texture_vertices);
  if (use_cone.get()) {
    drawVertices({pos.x, pos.y, pos.z}, ring_vertex_count, nullptr, worldMatrix, solid_vertices, nullptr);
    drawVertices({pos.x, pos.y, pos.z}, cylinder_vertex_count, nullptr, worldMatrix, inner_cylinder_vertices, nullptr);
    drawVertices({pos.x, pos.y, pos.z}, cylinder_vertex_count, nullptr, worldMatrix, outer_cylinder_vertices, nullptr);
  }
  device->SetTransform(D3DTS_WORLD, &originalWorldMatrix);
  reset_render_states();
  // delete[] texture_vertices;
  delete[] solid_vertices;
  delete[] outer_cylinder_vertices;
  delete[] inner_cylinder_vertices;
  delete[] texture_vertices;
}

void TargetRing::callback_render() {
  if (!enabled.get() || !Zeal::Game::is_in_game() || (hide_with_gui.get() && !Zeal::Game::is_gui_visible())) return;
  Zeal::GameStructures::Entity *target = Zeal::Game::get_target();
  if (!target || !target->ActorInfo || !target->ActorInfo->ViewActor_) return;
  if (disable_for_self.get() && (target == Zeal::Game::get_self())) return;
  float radius = 10.f;  // Zeal::Game::CalcCombatRange(Zeal::Game::get_self(), target);

  // ### Target Ring Color ###
  DWORD originalColor = target_color.get() ? get_target_color() : Zeal::Game::GetLevelCon(target);

  // Max Red, Green, and Blue by default
  DWORD Color = originalColor;

  // ### Auto Attack Indicator (fade/unfade target's color while autoattack turned on)###
  if (attack_indicator.get())  // auto attack is enabled
  {
    float fadeFactor = Zeal::Game::get_target_blink_fade_factor(flash_speed.get(), true);
    if (fadeFactor < 1.0f) {
      // Extract the ARGB components from the original color
      BYTE originalA = (originalColor >> 24) & 0xFF;
      BYTE originalR = (originalColor >> 16) & 0xFF;
      BYTE originalG = (originalColor >> 8) & 0xFF;
      BYTE originalB = originalColor & 0xFF;

      // Calculate the faded color components
      BYTE fadedA = originalA;  // Keep the original alpha value
      BYTE fadedR = (BYTE)((1.0f - fadeFactor) * 0x00 + fadeFactor * originalR);
      BYTE fadedG = (BYTE)((1.0f - fadeFactor) * 0x00 + fadeFactor * originalG);
      BYTE fadedB = (BYTE)((1.0f - fadeFactor) * 0x00 + fadeFactor * originalB);

      // Set the color with the faded components
      Color = D3DCOLOR_ARGB(fadedA, fadedR, fadedG, fadedB);
    }
  }

  if (targetRingTexture) {
    // ### Rotate Target Ring ###
    static constexpr float kMathPi = static_cast<float>(M_PI);
    static float rotationAngle = 0.0f;
    // Calculate the increment for a full rotation every 6 seconds
    static const float rotationIncrement = (2.0f * kMathPi) / (6.0f * 1000.0f);  // radians per millisecond
    // Update the rotation angle based on the elapsed time
    static ULONGLONG lastRotationTime = GetTickCount64();
    ULONGLONG currentRotationTime = GetTickCount64();
    ULONGLONG elapsedRotationTime = currentRotationTime - lastRotationTime;
    rotationAngle += (rotationIncrement * elapsedRotationTime) * rotation_speed.get();
    lastRotationTime = currentRotationTime;
    // Reset the rotation angle after a full rotation
    if (rotationAngle >= 2.0f * kMathPi) {
      rotationAngle -= 2.0f * kMathPi;
    }
    float direction = static_cast<float>(target->Heading * kMathPi / 256);
    // ### Render Target Ring ###
    render_ring({target->Position.x, target->Position.y, target->ActorInfo->Z}, outer_size.get(), Color,
                targetRingTexture, rotate_match_heading.get() ? direction : rotationAngle);
  } else {
    render_ring({target->Position.x, target->Position.y, target->ActorInfo->Z}, outer_size.get(), Color, nullptr,
                rotate_match_heading.get() ? target->Heading : 0);
  }
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

void TargetRing::callback_initui() { load_texture(texture_name.get()); }

void TargetRing::callback_cleanui() {
  if (targetRingTexture) targetRingTexture->Release();
  targetRingTexture = nullptr;
}

TargetRing::TargetRing(ZealService *zeal) {
  zeal->callbacks->AddGeneric([this]() { callback_render(); }, callback_type::RenderUI);
  zeal->callbacks->AddGeneric([this]() { callback_initui(); }, callback_type::InitUI);
  zeal->callbacks->AddGeneric([this]() { callback_cleanui(); }, callback_type::CleanUI);
  zeal->callbacks->AddGeneric([this]() { callback_cleanui(); }, callback_type::DXReset);  // Just release all resources.
  zeal->callbacks->AddGeneric([this]() { callback_cleanui(); }, callback_type::DXCleanDevice);

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

TargetRing::~TargetRing() { callback_cleanui(); }

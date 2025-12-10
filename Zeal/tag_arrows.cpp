#include "tag_arrows.h"

#include "game_functions.h"

TagArrows::TagArrows(IDirect3DDevice8 &device) : device(device) {
  CalculateVertices();  // Precalculate the arrow shape.
}

TagArrows::~TagArrows() { Release(); }

// DirectX resources need to be manually released at Reset or other cleanup.
void TagArrows::Release() {
  if (vertex_buffer) vertex_buffer->Release();
  vertex_buffer = nullptr;
  vertex_buffer_wr_index = 0;
  if (index_buffer) index_buffer->Release();
  index_buffer = nullptr;
  num_indices = 0;
  arrow_queue.clear();
  memset(vertices, 0, sizeof(vertices));
}

void TagArrows::Dump() const {
  Zeal::Game::print_chat("vertex: %d, index: %d, num_indices: %d, vertex_buffer_wr_index: %d", vertex_buffer != nullptr,
                         index_buffer != nullptr, num_indices, vertex_buffer_wr_index);
}

void TagArrows::QueueArrow(const Vec3 &position, const D3DCOLOR color) {
  arrow_queue.emplace_back(Arrow{.position = position, .color = color});
}

void TagArrows::FlushQueueToScreen() {
  if (arrow_queue.empty()) return;

  if (!vertex_buffer) {
    vertex_buffer_wr_index = 0;
    if (FAILED(device.CreateVertexBuffer(kVertexBufferMaxArrowsCount * kNumArrowVertices * sizeof(ArrowVertex),
                                         D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC, ArrowVertex::kFvfCode, D3DPOOL_DEFAULT,
                                         &vertex_buffer))) {
      vertex_buffer = nullptr;  // Ensure nullptr.
      Release();                // Do a full cleanup.
      return;
    }
  }

  if (!index_buffer && !CreateIndexBuffer()) {
    Release();  // Do a full cleanup.
    return;
  }

  RenderQueue();
  arrow_queue.clear();
}

// Submits arrows to the D3D renderer one at a time.
void TagArrows::RenderQueue() {
  // Configure for 3D rendering. Could possibly enhance this by enabling light and adding
  // diffuse, specular, and ambient lighting but keep it simple for now.
  D3DRenderStateStash render_state(device);
  render_state.store_and_modify({D3DRS_CULLMODE, D3DCULL_NONE});  // Future optimization.
  render_state.store_and_modify({D3DRS_ALPHABLENDENABLE, FALSE});
  render_state.store_and_modify({D3DRS_ZENABLE, TRUE});
  render_state.store_and_modify({D3DRS_ZWRITEENABLE, TRUE});
  render_state.store_and_modify({D3DRS_LIGHTING, FALSE});

  // Note: Not preserving shader, texture, source, or indices to avoid reference counting.
  device.SetVertexShader(ArrowVertex::kFvfCode);
  device.SetTexture(0, NULL);  // Ensure no texture is bound
  device.SetStreamSource(0, vertex_buffer, sizeof(ArrowVertex));

  D3DXMATRIX worldMatrix, originalWorldMatrix;
  device.GetTransform(D3DTS_WORLD, &originalWorldMatrix);  // Stashing this for restoration.

  D3DXMATRIX translationMatrix;
  for (const auto &entry : arrow_queue) {
    // Per arrow translation from model space to world space.
    D3DXMatrixTranslation(&translationMatrix, entry.position.x, entry.position.y, entry.position.z);
    device.SetTransform(D3DTS_WORLD, &translationMatrix);

    // Retrieve the color dependent set of arrow model vertices.
    int start_vertex_index = GetStartVertexIndex(entry);
    if (start_vertex_index < 0) {
      Release();
      return;
    }

    device.SetIndices(index_buffer, start_vertex_index);
    device.DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP, 0, kNumArrowVertices, 0, num_indices - 2);
  }

  // Restore D3D state.
  device.SetStreamSource(0, NULL, 0);  // Unbind vertex buffer.
  device.SetIndices(NULL, 0);          // Ensure index_buffer is no longer bound.
  device.SetTransform(D3DTS_WORLD, &originalWorldMatrix);
  render_state.restore_state();
}

int TagArrows::GetStartVertexIndex(const Arrow &arrow) {
  // First check for the presence of a matching cached color.
  for (int i = 0; i < vertex_buffer_wr_index; ++i) {
    if (vertex_cache_colors[i] == arrow.color) return i * kNumArrowVertices;
  }

  if (vertex_buffer_wr_index >= kVertexBufferMaxArrowsCount) vertex_buffer_wr_index = 0;
  vertex_cache_colors[vertex_buffer_wr_index] = arrow.color;

  // No cache hit so need to update the vertex colors of our pre-calculated vertex model and then
  // copy it over to the next vertex buffer slot.
  auto color = arrow.color;
  int entry_red = (color >> 16) & 0xFF;
  int entry_green = (color >> 8) & 0xFF;
  int entry_blue = color & 0xFF;
  int grey = 192;
  auto top_color =
      D3DCOLOR_XRGB(grey + (entry_red - grey) / 8, grey + (entry_green - grey) / 8, grey + (entry_blue - grey) / 8);
  auto inner_color = D3DCOLOR_XRGB(entry_red + (grey - entry_red) / 4, entry_green + (grey - entry_green) / 4,
                                   entry_blue + (grey - entry_blue) / 4);
  auto outer_color = D3DCOLOR_XRGB(entry_red + (grey - entry_red) / 2, entry_green + (grey - entry_green) / 2,
                                   entry_blue + (grey - entry_blue) / 2);
  for (int i = 0; i < kNumCircleVertices; ++i) vertices[i].color = top_color;
  for (int i = kNumCircleVertices; i < 2 * kNumCircleVertices; ++i) vertices[i].color = inner_color;
  for (int i = 2 * kNumCircleVertices; i < 3 * kNumCircleVertices; ++i) vertices[i].color = outer_color;
  vertices[3 * kNumCircleVertices].color = arrow.color;
  vertices[3 * kNumCircleVertices + 1].color = arrow.color;

  // The cache is effectively flushed when the wr_index rolls around otherwise we are appending.
  auto lock_type = (vertex_buffer_wr_index == 0) ? D3DLOCK_DISCARD : D3DLOCK_NOOVERWRITE;
  const int start_vertex_index = vertex_buffer_wr_index * kNumArrowVertices;
  const int start_offset_bytes = start_vertex_index * sizeof(ArrowVertex);
  const int copy_size = sizeof(vertices);
  BYTE *buffer = nullptr;
  if (FAILED(vertex_buffer->Lock(start_offset_bytes, copy_size, &buffer, lock_type))) {
    Release();
    return -1;
  }
  memcpy(buffer, vertices, copy_size);
  vertex_buffer->Unlock();
  vertex_buffer_wr_index++;

  return start_vertex_index;
}

// Calculate the fixed relative locations of the vertices with a default color that is updated later.
void TagArrows::CalculateVertices() {
  // The arrow consists of the top circle, the bottom circle base of the cylinder, the wider bottom circle for
  // the arrow head, and then the point at the bottom which is set at the origin.
  const auto color = D3DCOLOR_XRGB(0xff, 0xff, 0xff);
  const float top_height = 4;
  const float mid_height = 2;
  const float inner_radius = 0.75;
  const float outer_radius = 1.5;
  const float angle_step = 2.0f * static_cast<float>(M_PI) / kNumCircleVertices;  // Fixed truncation warning
  for (int i = 0; i < kNumCircleVertices; ++i) {
    float angle = (i * angle_step);
    float cos_angle = cosf(angle);
    float sin_angle = sinf(angle);
    vertices[i] = {.x = inner_radius * cos_angle, .y = inner_radius * sin_angle, .z = top_height, .color = color};
    vertices[i + kNumCircleVertices] = vertices[i];
    vertices[i + kNumCircleVertices].z = mid_height;
    vertices[i + 2 * kNumCircleVertices] = {
        .x = outer_radius * cos_angle, .y = outer_radius * sin_angle, .z = mid_height, .color = color};
  }
  vertices[3 * kNumCircleVertices + 0] = {.x = 0, .y = 0, .z = 0, .color = color};
  vertices[3 * kNumCircleVertices + 1] = {.x = 0, .y = 0, .z = top_height, .color = color};
}

// Define the fixed index lookup map that accesses the Arrow vertices in the required strip order.
bool TagArrows::CreateIndexBuffer() {
  if (index_buffer) return true;

  // Calculate the fixed pattern that maps the pre-calculated vertices to the form the
  // the cylinder and cone shapes.
  std::vector<int16_t> indices;
  const int16_t bottom_center_vertex_index = 3 * kNumCircleVertices;
  const int16_t top_center_vertex_index = 3 * kNumCircleVertices + 1;

  // Draw top filled circle.
  for (size_t i = 0; i < kNumCircleVertices; ++i) {
    indices.push_back(i);                        // Top radius.
    indices.push_back(top_center_vertex_index);  // To center.
  }
  indices.push_back(0);  // Back to starting top radius to close out circle.

  // Draw the cylinder walls (the repeat at 0 breaks the strip).
  for (size_t i = 0; i < kNumCircleVertices; ++i) {
    indices.push_back(i);
    indices.push_back(kNumCircleVertices + i);
  }
  indices.push_back(0);  // Close off with final triangle set.
  indices.push_back(kNumCircleVertices);

  // Draw the ring at bottom (inner to outer, repeat from above breaks strip).
  for (size_t i = 0; i < kNumCircleVertices; ++i) {
    indices.push_back(kNumCircleVertices + i);
    indices.push_back(2 * kNumCircleVertices + i);
  }
  indices.push_back(kNumCircleVertices);  // Repeat to close off final and end up at outside.
  indices.push_back(2 * kNumCircleVertices);

  // Draw the cone (and repeat from above breaks strip).
  for (size_t i = 0; i < kNumCircleVertices; ++i) {
    indices.push_back(2 * kNumCircleVertices + i);
    indices.push_back(bottom_center_vertex_index);  // To center.
  }
  indices.push_back(2 * kNumCircleVertices);  // Back to starting outer radius to close out cone.
  indices.push_back(2 * kNumCircleVertices);  // And repeat to terminate strip.

  int buffer_size = indices.size() * sizeof(indices[0]);
  if (FAILED(device.CreateIndexBuffer(buffer_size, 0, D3DFMT_INDEX16, D3DPOOL_DEFAULT, &index_buffer))) {
    index_buffer = nullptr;  // Ensure nullptr.
    return false;
  }

  uint8_t *locked_buffer = nullptr;
  if (FAILED(index_buffer->Lock(0, 0, &locked_buffer, D3DLOCK_DISCARD))) {
    Release();
    return false;
  }

  memcpy(locked_buffer, indices.data(), buffer_size);
  index_buffer->Unlock();
  num_indices = indices.size();
  return true;
}

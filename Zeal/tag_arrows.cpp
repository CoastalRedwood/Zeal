#include "tag_arrows.h"

#include <array>
#include <span>

#include "game_functions.h"

// Arrow consists of 3 circles with a vertex at the tip.
static constexpr int kNumCircleVertices = 60;                         // Spaced every 6 degrees.
static constexpr int kNumArrowVertices = 3 * kNumCircleVertices + 2;  // Three circles + top and bottom centers.
static constexpr int kVertexBufferMaxArrowsCount = 8;                 // Cache up to 8 arrow colors.

// The octagon consists of two faces with 8 vertices each.
static constexpr int kNumOctagonVertices = 16;

// The paw consists of four toes and a main pad in hard-coded patterns.
namespace PawData {

struct Point {
  float x;
  float y;
};

static constexpr int kNumToePadPoints = 40;
static constexpr int kNumMainPadPoints = 120;
static constexpr int kNumPawVertices = 2 * (4 * kNumToePadPoints + kNumMainPadPoints);

static constexpr std::array<Point, kNumToePadPoints> kToeLeftOuter = {
    {{-34.47f, 50.50f}, {-35.76f, 52.46f}, {-37.26f, 54.25f}, {-38.92f, 55.80f}, {-40.70f, 57.09f}, {-42.57f, 58.07f},
     {-44.47f, 58.74f}, {-46.36f, 59.07f}, {-48.19f, 59.05f}, {-49.92f, 58.69f}, {-51.50f, 57.99f}, {-52.90f, 56.97f},
     {-54.08f, 55.65f}, {-55.01f, 54.08f}, {-55.67f, 52.28f}, {-56.04f, 50.30f}, {-56.12f, 48.19f}, {-55.89f, 46.00f},
     {-55.38f, 43.78f}, {-54.58f, 41.60f}, {-53.53f, 39.50f}, {-52.24f, 37.54f}, {-50.74f, 35.75f}, {-49.08f, 34.20f},
     {-47.30f, 32.91f}, {-45.43f, 31.93f}, {-43.53f, 31.26f}, {-41.64f, 30.93f}, {-39.81f, 30.95f}, {-38.08f, 31.31f},
     {-36.50f, 32.01f}, {-35.10f, 33.03f}, {-33.92f, 34.35f}, {-32.99f, 35.92f}, {-32.33f, 37.72f}, {-31.96f, 39.70f},
     {-31.88f, 41.81f}, {-32.11f, 44.00f}, {-32.62f, 46.22f}, {-33.42f, 48.40f}}};

static constexpr std::array<Point, kNumToePadPoints> kToeRightOuter = {
    {{53.53f, 39.50f}, {54.58f, 41.60f}, {55.38f, 43.78f}, {55.89f, 46.00f}, {56.12f, 48.19f}, {56.04f, 50.30f},
     {55.67f, 52.28f}, {55.01f, 54.08f}, {54.08f, 55.65f}, {52.90f, 56.97f}, {51.50f, 57.99f}, {49.92f, 58.69f},
     {48.19f, 59.05f}, {46.36f, 59.07f}, {44.47f, 58.74f}, {42.57f, 58.07f}, {40.70f, 57.09f}, {38.92f, 55.80f},
     {37.26f, 54.25f}, {35.76f, 52.46f}, {34.47f, 50.50f}, {33.42f, 48.40f}, {32.62f, 46.22f}, {32.11f, 44.00f},
     {31.88f, 41.81f}, {31.96f, 39.70f}, {32.33f, 37.72f}, {32.99f, 35.92f}, {33.92f, 34.35f}, {35.10f, 33.03f},
     {36.50f, 32.01f}, {38.08f, 31.31f}, {39.81f, 30.95f}, {41.64f, 30.93f}, {43.53f, 31.26f}, {45.43f, 31.93f},
     {47.30f, 32.91f}, {49.08f, 34.20f}, {50.74f, 35.75f}, {52.24f, 37.54f}}};

static constexpr std::array<Point, kNumToePadPoints> kToeLeftInner = {
    {{-5.13f, 73.81f},  {-5.68f, 76.58f},  {-6.53f, 79.23f},  {-7.67f, 81.70f},  {-9.06f, 83.94f},  {-10.67f, 85.88f},
     {-12.46f, 87.48f}, {-14.39f, 88.70f}, {-16.40f, 89.51f}, {-18.46f, 89.89f}, {-20.51f, 89.82f}, {-22.49f, 89.32f},
     {-24.36f, 88.39f}, {-26.08f, 87.06f}, {-27.59f, 85.36f}, {-28.87f, 83.32f}, {-29.89f, 81.01f}, {-30.61f, 78.48f},
     {-31.02f, 75.79f}, {-31.11f, 73.00f}, {-30.87f, 70.19f}, {-30.32f, 67.42f}, {-29.47f, 64.77f}, {-28.33f, 62.30f},
     {-26.94f, 60.06f}, {-25.33f, 58.12f}, {-23.54f, 56.52f}, {-21.61f, 55.30f}, {-19.60f, 54.49f}, {-17.54f, 54.11f},
     {-15.49f, 54.18f}, {-13.51f, 54.68f}, {-11.64f, 55.61f}, {-9.92f, 56.94f},  {-8.41f, 58.64f},  {-7.13f, 60.68f},
     {-6.11f, 62.99f},  {-5.39f, 65.52f},  {-4.98f, 68.21f},  {-4.89f, 71.00f}}};

static constexpr std::array<Point, kNumToePadPoints> kToeRightInner = {
    {{30.87f, 70.19f}, {31.11f, 73.00f}, {31.02f, 75.79f}, {30.61f, 78.48f}, {29.89f, 81.01f}, {28.87f, 83.32f},
     {27.59f, 85.36f}, {26.08f, 87.06f}, {24.36f, 88.39f}, {22.49f, 89.32f}, {20.51f, 89.82f}, {18.46f, 89.89f},
     {16.40f, 89.51f}, {14.39f, 88.70f}, {12.46f, 87.48f}, {10.67f, 85.88f}, {9.06f, 83.94f},  {7.67f, 81.70f},
     {6.53f, 79.23f},  {5.68f, 76.58f},  {5.13f, 73.81f},  {4.89f, 71.00f},  {4.98f, 68.21f},  {5.39f, 65.52f},
     {6.11f, 62.99f},  {7.13f, 60.68f},  {8.41f, 58.64f},  {9.92f, 56.94f},  {11.64f, 55.61f}, {13.51f, 54.68f},
     {15.49f, 54.18f}, {17.54f, 54.11f}, {19.60f, 54.49f}, {21.61f, 55.30f}, {23.54f, 56.52f}, {25.33f, 58.12f},
     {26.94f, 60.06f}, {28.33f, 62.30f}, {29.47f, 64.77f}, {30.32f, 67.42f}}};

static constexpr std::array<Point, kNumMainPadPoints> kMainPad = {
    {{0.00f, 2.53f},    {2.20f, 2.51f},    {4.39f, 2.46f},    {6.57f, 2.37f},    {8.73f, 2.24f},    {10.87f, 2.09f},
     {12.98f, 1.91f},   {15.05f, 1.71f},   {17.08f, 1.50f},   {19.07f, 1.27f},   {21.00f, 1.05f},   {22.87f, 0.82f},
     {24.69f, 0.61f},   {26.43f, 0.42f},   {28.10f, 0.25f},   {29.70f, 0.12f},   {31.21f, 0.03f},   {32.64f, 0.00f},
     {33.98f, 0.02f},   {35.22f, 0.12f},   {36.37f, 0.28f},   {37.42f, 0.53f},   {38.37f, 0.86f},   {39.21f, 1.28f},
     {39.94f, 1.80f},   {40.57f, 2.41f},   {41.08f, 3.13f},   {41.48f, 3.95f},   {41.77f, 4.88f},   {41.94f, 5.90f},
     {42.00f, 7.03f},   {41.94f, 8.26f},   {41.77f, 9.58f},   {41.48f, 10.99f},  {41.08f, 12.49f},  {40.57f, 14.06f},
     {39.94f, 15.70f},  {39.21f, 17.41f},  {38.37f, 19.16f},  {37.42f, 20.96f},  {36.37f, 22.78f},  {35.22f, 24.62f},
     {33.98f, 26.47f},  {32.64f, 28.32f},  {31.21f, 30.15f},  {29.70f, 31.94f},  {28.10f, 33.69f},  {26.43f, 35.39f},
     {24.69f, 37.01f},  {22.87f, 38.56f},  {21.00f, 40.02f},  {19.07f, 41.37f},  {17.08f, 42.61f},  {15.05f, 43.72f},
     {12.98f, 44.71f},  {10.87f, 45.56f},  {8.73f, 46.26f},   {6.57f, 46.81f},   {4.39f, 47.21f},   {2.20f, 47.45f},
     {0.00f, 47.53f},   {-2.20f, 47.45f},  {-4.39f, 47.21f},  {-6.57f, 46.81f},  {-8.73f, 46.26f},  {-10.87f, 45.56f},
     {-12.98f, 44.71f}, {-15.05f, 43.72f}, {-17.08f, 42.61f}, {-19.07f, 41.37f}, {-21.00f, 40.02f}, {-22.87f, 38.56f},
     {-24.69f, 37.01f}, {-26.43f, 35.39f}, {-28.10f, 33.69f}, {-29.70f, 31.94f}, {-31.21f, 30.15f}, {-32.64f, 28.32f},
     {-33.98f, 26.47f}, {-35.22f, 24.62f}, {-36.37f, 22.78f}, {-37.42f, 20.96f}, {-38.37f, 19.16f}, {-39.21f, 17.41f},
     {-39.94f, 15.70f}, {-40.57f, 14.06f}, {-41.08f, 12.49f}, {-41.48f, 10.99f}, {-41.77f, 9.58f},  {-41.94f, 8.26f},
     {-42.00f, 7.03f},  {-41.94f, 5.90f},  {-41.77f, 4.88f},  {-41.48f, 3.95f},  {-41.08f, 3.13f},  {-40.57f, 2.41f},
     {-39.94f, 1.80f},  {-39.21f, 1.28f},  {-38.37f, 0.86f},  {-37.42f, 0.53f},  {-36.37f, 0.28f},  {-35.22f, 0.12f},
     {-33.98f, 0.02f},  {-32.64f, 0.00f},  {-31.21f, 0.03f},  {-29.70f, 0.12f},  {-28.10f, 0.25f},  {-26.43f, 0.42f},
     {-24.69f, 0.61f},  {-22.87f, 0.82f},  {-21.00f, 1.05f},  {-19.07f, 1.27f},  {-17.08f, 1.50f},  {-15.05f, 1.71f},
     {-12.98f, 1.91f},  {-10.87f, 2.09f},  {-8.73f, 2.24f},   {-6.57f, 2.37f},   {-4.39f, 2.46f},   {-2.20f, 2.51f}}};
}  // namespace PawData

static constexpr int kBufferVertices =
    kVertexBufferMaxArrowsCount * kNumArrowVertices + PawData::kNumPawVertices + kNumOctagonVertices;

// Helper class for calculating a height dependent gradient.
class Gradient {
 public:
  Gradient(D3DCOLOR color, int grey_in, float min_h_in, float max_h_in, float clamp_low_in = 0.15f,
           float clamp_high_in = 1.0f) {
    grey = max(0, min(255, grey_in));
    base_red = (color >> 16) & 0xFF;
    base_green = (color >> 8) & 0xFF;
    base_blue = color & 0xFF;
    min_h = min_h_in;
    max_h = max(min_h + 1.0f, max_h_in);
    clamp_low = max(0.0f, min(1.0f, clamp_low_in));
    clamp_high = max(clamp_low, min(1.0f, clamp_high_in));
  }

  D3DCOLOR GetColor(float h) {
    float gradient = clamp_low + 0.85f * max(0.0f, min(1.0f, (h - min_h) / (max_h - min_h)));
    return D3DCOLOR_XRGB(grey + static_cast<uint8_t>((base_red - grey) * gradient),
                         grey + static_cast<uint8_t>((base_green - grey) * gradient),
                         grey + static_cast<uint8_t>((base_blue - grey) * gradient));
  }

 private:
  int grey;
  int base_red;
  int base_green;
  int base_blue;
  float clamp_low;
  float clamp_high;
  float min_h;
  float max_h;
};

TagArrows::TagArrows(IDirect3DDevice8 &device) : device(device) {
  CalculateShapes();  // Precalculate the arrow and other shapes.
}

TagArrows::~TagArrows() { Release(); }

// DirectX resources need to be manually released at Reset or other cleanup.
void TagArrows::Release() {
  if (vertex_buffer) vertex_buffer->Release();
  vertex_buffer = nullptr;
  render_infos.clear();

  if (index_buffer) index_buffer->Release();
  index_buffer = nullptr;

  arrow_queue.clear();
}

void TagArrows::Dump() const {
  Zeal::Game::print_chat("vertex: %d, index: %d, infos: %d", vertex_buffer != nullptr, index_buffer != nullptr,
                         render_infos.size());
  for (const auto &info : render_infos) {
    Zeal::Game::print_chat("Info: start_vertex: %d, color: 0x%08x, verts: %d, index: %d, prims: %d",
                           info.start_vertex_index, info.color, info.num_vertices, info.start_index,
                           info.num_primitives);
  }
}

void TagArrows::QueueTagShape(const Vec3 &position, const D3DCOLOR color, Shape shape, float bearing) {
  arrow_queue.emplace_back(Arrow{.position = position, .color = color, .shape = shape, .bearing = bearing});
}

void TagArrows::FlushQueueToScreen() {
  if (arrow_queue.empty()) return;

  if (!vertex_buffer) {
    render_infos.clear();
    if (FAILED(device.CreateVertexBuffer(kBufferVertices * sizeof(ArrowVertex), D3DUSAGE_WRITEONLY | D3DUSAGE_DYNAMIC,
                                         ArrowVertex::kFvfCode, D3DPOOL_DEFAULT, &vertex_buffer))) {
      vertex_buffer = nullptr;  // Ensure nullptr.
      Release();                // Do a full cleanup.
      return;
    }
  }

  // The index buffer is fixed (color independent) and points to all shapes.
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

  D3DXMATRIX originalWorldMatrix;
  device.GetTransform(D3DTS_WORLD, &originalWorldMatrix);  // Stashing this for restoration.

  D3DXMATRIX translationMatrix;
  for (const auto &entry : arrow_queue) {
    // Retrieve the color dependent set of arrow model vertices.
    auto render_info = GetRenderInfo(entry);

    // Per arrow translation from model space to world space.
    D3DXMatrixTranslation(&translationMatrix, entry.position.x, entry.position.y, entry.position.z);

    // And support rotating the model towards the bearing if the value is set.
    if (entry.bearing != 0) {
      D3DXMATRIX rotation_matrix;
      D3DXMatrixRotationZ(&rotation_matrix, -entry.bearing);
      translationMatrix = rotation_matrix * translationMatrix;
    }
    device.SetTransform(D3DTS_WORLD, &translationMatrix);

    if (render_info.start_vertex_index < 0) {
      Release();
      return;
    }

    device.SetIndices(index_buffer, render_info.start_vertex_index);
    device.DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP, 0, render_info.num_vertices, render_info.start_index,
                                render_info.num_primitives);
  }

  // Restore D3D state.
  device.SetStreamSource(0, NULL, 0);  // Unbind vertex buffer.
  device.SetIndices(NULL, 0);          // Ensure index_buffer is no longer bound.
  device.SetTransform(D3DTS_WORLD, &originalWorldMatrix);
  render_state.restore_state();
}

TagArrows::RenderInfo TagArrows::GetRenderInfo(const Arrow &tag) {
  // Go through cache buffer checking if there is a match.
  for (const auto &render_info : render_infos) {
    if (render_info.shape == tag.shape && render_info.color == tag.color) return render_info;
  }

  switch (tag.shape) {
    case Shape::Arrow:
      return AllocateArrow(tag);
    case Shape::Octagon:
      return AllocateOctagon(tag);
    case Shape::Paw:
      return AllocatePaw(tag);
    default:
      return RenderInfo();
  }
}

// Appends the vertices of a new shape. Caller is responsible for updating render_infos.
int TagArrows::AppendVertices(std::vector<ArrowVertex> &vertices) {
  int start_vertex_index = 0;
  if (!render_infos.empty()) {
    start_vertex_index = render_infos.back().start_vertex_index + render_infos.back().num_vertices;
  }

  if (start_vertex_index + vertices.size() > kBufferVertices) {
    start_vertex_index = 0;
    render_infos.clear();  // Flush cache.
  }

  auto lock_type = (start_vertex_index == 0) ? D3DLOCK_DISCARD : D3DLOCK_NOOVERWRITE;
  const int start_offset_bytes = start_vertex_index * sizeof(ArrowVertex);
  const int copy_size = vertices.size() * sizeof(vertices[0]);
  BYTE *buffer = nullptr;
  if (FAILED(vertex_buffer->Lock(start_offset_bytes, copy_size, &buffer, lock_type))) {
    Release();
    return -1;
  }
  memcpy(buffer, vertices.data(), copy_size);
  vertex_buffer->Unlock();

  return start_vertex_index;
}

// Calculate the fixed relative locations of the vertices with a default color that is updated later.
void TagArrows::CalculateShapes() {
  indices.clear();
  CalculateArrowVertices();
  AppendArrowIndices();
  CalculateOctagonVertices();
  AppendOctagonIndices();
  CalculatePawVertices();
  AppendPawIndices();
}

void TagArrows::CalculateArrowVertices() {
  // The arrow consists of the top circle, the bottom circle base of the cylinder, the wider bottom circle for
  // the arrow head, and then the point at the bottom which is set at the origin.
  const auto color = D3DCOLOR_XRGB(0xff, 0xff, 0xff);
  const float top_height = 4;
  const float mid_height = 2;
  const float inner_radius = 0.75;
  const float outer_radius = 1.5;
  const float angle_step = 2.0f * static_cast<float>(M_PI) / kNumCircleVertices;  // Fixed truncation warning

  arrow_vertices.clear();
  arrow_vertices.resize(kNumArrowVertices);
  for (int i = 0; i < kNumCircleVertices; ++i) {
    float angle = (i * angle_step);
    float cos_angle = cosf(angle);
    float sin_angle = sinf(angle);
    arrow_vertices[i] = {.x = inner_radius * cos_angle, .y = inner_radius * sin_angle, .z = top_height, .color = color};
    arrow_vertices[i + kNumCircleVertices] = arrow_vertices[i];
    arrow_vertices[i + kNumCircleVertices].z = mid_height;
    arrow_vertices[i + 2 * kNumCircleVertices] = {
        .x = outer_radius * cos_angle, .y = outer_radius * sin_angle, .z = mid_height, .color = color};
  }
  arrow_vertices[3 * kNumCircleVertices + 0] = {.x = 0, .y = 0, .z = 0, .color = color};
  arrow_vertices[3 * kNumCircleVertices + 1] = {.x = 0, .y = 0, .z = top_height, .color = color};
}

void TagArrows::AppendArrowIndices() {
  const int16_t bottom_center_vertex_index = 3 * kNumCircleVertices;
  const int16_t top_center_vertex_index = 3 * kNumCircleVertices + 1;

  arrow_index_start = indices.size();
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

  arrow_primitive_count = indices.size() - arrow_index_start - 2;
}

TagArrows::RenderInfo TagArrows::AllocateArrow(const Arrow &arrow) {
  if (arrow_vertices.size() != kNumArrowVertices) return RenderInfo();

  // Update the vertex colors of our pre-calculated vertex model and copy it over to vertex memory.
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
  for (int i = 0; i < kNumCircleVertices; ++i) arrow_vertices[i].color = top_color;
  for (int i = kNumCircleVertices; i < 2 * kNumCircleVertices; ++i) arrow_vertices[i].color = inner_color;
  for (int i = 2 * kNumCircleVertices; i < 3 * kNumCircleVertices; ++i) arrow_vertices[i].color = outer_color;
  arrow_vertices[3 * kNumCircleVertices].color = arrow.color;
  arrow_vertices[3 * kNumCircleVertices + 1].color = arrow.color;

  int start_vertex_index = AppendVertices(arrow_vertices);
  if (start_vertex_index < 0) return RenderInfo();

  render_infos.emplace_back(RenderInfo{.shape = arrow.shape,
                                       .color = arrow.color,
                                       .start_vertex_index = start_vertex_index,
                                       .num_vertices = arrow_vertices.size(),
                                       .start_index = arrow_index_start,
                                       .num_primitives = arrow_primitive_count,
                                       .bearing = arrow.bearing});
  return render_infos.back();
}

void TagArrows::CalculateOctagonVertices() {
  // The Octagon consists of two faces with 8 vertices each.
  const auto color = D3DCOLOR_XRGB(0xff, 0xff, 0xff);
  const float radius = 1.25f;
  const float angle_step = static_cast<float>(2.0 * M_PI / 8);
  const float angle_offset = static_cast<float>(M_PI / 8);
  const float thickness = 0.25f;

  octagon_vertices.clear();
  octagon_vertices.resize(8 * 2);
  for (int i = 0; i < 8; ++i) {
    float angle = i * angle_step + angle_offset;
    float cos_angle = cosf(angle);
    float sin_angle = sinf(angle);
    octagon_vertices[i] = {.x = radius * cos_angle, .y = thickness, .z = radius * (1 + sin_angle), .color = color};
    octagon_vertices[i + 8] = octagon_vertices[i];
    octagon_vertices[i + 8].y = -thickness;
  }
}

void TagArrows::AppendOctagonIndices() {
  octagon_index_start = indices.size();

  // Face:
  indices.push_back(1);
  indices.push_back(2);
  indices.push_back(0);
  indices.push_back(3);
  indices.push_back(7);
  indices.push_back(4);
  indices.push_back(6);
  indices.push_back(5);
  indices.push_back(5);  // Repeat to terminate the strip.
  int face_count = indices.size() - octagon_index_start;

  // Wall:
  for (int i = 0; i < 8; ++i) {
    indices.push_back(i);
    indices.push_back(i + 8);
  }
  indices.push_back(0);
  indices.push_back(8);
  indices.push_back(8);  // Repeat to terminate the strip.

  // Other face:
  for (int i = 0; i < face_count; ++i) indices.push_back(indices[octagon_index_start + i] + 8);

  octagon_primitive_count = indices.size() - octagon_index_start - 2;
}

TagArrows::RenderInfo TagArrows::AllocateOctagon(const Arrow &tag) {
  if (octagon_vertices.size() != kNumOctagonVertices) return RenderInfo();

  // Update the vertex colors of our pre-calculated vertex model and copy it over to vertex memory.
  Gradient gradient(tag.color, 192, octagon_vertices[6].z, octagon_vertices[1].z);
  Gradient gradient2(tag.color, 64, octagon_vertices[6].z, octagon_vertices[1].z, 0.0f, 0.7f);
  for (auto &vertex : octagon_vertices)
    vertex.color = vertex.y < 0 ? gradient.GetColor(vertex.z) : gradient2.GetColor(vertex.z);

  int start_vertex_index = AppendVertices(octagon_vertices);
  if (start_vertex_index < 0) return RenderInfo();

  render_infos.emplace_back(RenderInfo{.shape = tag.shape,
                                       .color = tag.color,
                                       .start_vertex_index = start_vertex_index,
                                       .num_vertices = octagon_vertices.size(),
                                       .start_index = octagon_index_start,
                                       .num_primitives = octagon_primitive_count,
                                       .bearing = tag.bearing});
  return render_infos.back();
}

static void AppendPadPoints(std::span<const PawData::Point> points, std::vector<TagArrows::ArrowVertex> &vertices) {
  const auto color = D3DCOLOR_XRGB(0xff, 0xff, 0xff);
  float scale = 0.025f;
  float h = 0.25f;  // Half-thickness.
  for (const auto point : points)
    vertices.push_back(TagArrows::ArrowVertex{.x = point.x * scale, .y = -h, .z = point.y * scale, .color = color});
}

void TagArrows::CalculatePawVertices() {
  // The Paw pad consists of four toes and a main pad with front and back faces.

  paw_vertices.clear();
  paw_vertices.reserve(2 * (4 * PawData::kNumToePadPoints + PawData::kNumMainPadPoints));
  AppendPadPoints(PawData::kToeLeftOuter, paw_vertices);
  AppendPadPoints(PawData::kToeLeftInner, paw_vertices);
  AppendPadPoints(PawData::kToeRightInner, paw_vertices);
  AppendPadPoints(PawData::kToeRightOuter, paw_vertices);
  AppendPadPoints(PawData::kMainPad, paw_vertices);

  int vertex_count = paw_vertices.size();
  for (int i = 0; i < vertex_count; ++i) {
    paw_vertices.push_back(paw_vertices[i]);
    paw_vertices.back().y = -paw_vertices.back().y;
  }
}

static void AppendPawPadIndices(int offset, int count, std::vector<int16_t> &indices) {
  indices.push_back(offset);  // Repeat first to ensure termination from previous.

  for (int i = 1; i < count - 1; i += 2) {
    indices.push_back(offset);
    indices.push_back(offset + i);
    indices.push_back(offset + i + 1);
  }

  if ((count & 0x01) == 0) {
    indices.push_back(offset);  // Ensure it is closed (only needed for even count).
    indices.push_back(offset + count - 1);
  }
  indices.push_back(offset + count - 1);  // Repeat final to terminate shape.
}

static void AppendPawPadWallIndices(int offset, int count, int back_offset, std::vector<int16_t> &indices) {
  indices.push_back(offset);  // Repeat first to ensure termination from previous.

  for (int i = 0; i < count; i++) {
    indices.push_back(offset + i);
    indices.push_back(offset + i + back_offset);
  }
  indices.push_back(offset);
  indices.push_back(offset + back_offset);
  indices.push_back(offset + back_offset);  // Repeat final to terminate shape.
}

void TagArrows::AppendPawIndices() {
  paw_index_start = indices.size();

  // Add the front faces.
  for (int i = 0; i < 4; ++i) AppendPawPadIndices(i * PawData::kNumToePadPoints, PawData::kNumToePadPoints, indices);
  AppendPawPadIndices(4 * PawData::kNumToePadPoints, PawData::kNumMainPadPoints, indices);

  // Add the other faces by copying and offsetting the front faces.
  int back_offset = paw_vertices.size() / 2;
  int front_indices = indices.size() - paw_index_start;
  for (int i = 0; i < front_indices; ++i) indices.push_back(indices[paw_index_start + i] + back_offset);

  // Add the walls.
  for (int i = 0; i < 4; ++i)
    AppendPawPadWallIndices(i * PawData::kNumToePadPoints, PawData::kNumToePadPoints, back_offset, indices);
  AppendPawPadWallIndices(4 * PawData::kNumToePadPoints, PawData::kNumMainPadPoints, back_offset, indices);

  paw_primitive_count = indices.size() - paw_index_start - 2;
}

TagArrows::RenderInfo TagArrows::AllocatePaw(const Arrow &tag) {
  if (paw_vertices.size() != PawData::kNumPawVertices) return RenderInfo();

  // Update the vertex colors of our pre-calculated vertex model and copy it over to vertex memory.
  Gradient gradient(tag.color, 192, 0, 90 * 0.025);  // Lazy approximation for the gradient.
  Gradient gradient2(tag.color, 64, 0, 90 * 0.025, 0.0f, 0.7f);
  for (auto &vertex : paw_vertices)
    vertex.color = vertex.y < 0 ? gradient.GetColor(vertex.z) : gradient2.GetColor(vertex.z);

  int start_vertex_index = AppendVertices(paw_vertices);
  if (start_vertex_index < 0) return RenderInfo();

  render_infos.emplace_back(RenderInfo{.shape = tag.shape,
                                       .color = tag.color,
                                       .start_vertex_index = start_vertex_index,
                                       .num_vertices = paw_vertices.size(),
                                       .start_index = paw_index_start,
                                       .num_primitives = paw_primitive_count,
                                       .bearing = tag.bearing});
  return render_infos.back();
}

// Copy over the fixed index lookup maps that access the shape vertices in the required strip order.
bool TagArrows::CreateIndexBuffer() {
  if (index_buffer) return true;

  if (indices.empty()) return false;

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
  return true;
}

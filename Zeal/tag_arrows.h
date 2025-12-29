#pragma once

#include <vector>

#include "directx.h"
#include "vectors.h"

// Directx 8 compatible class for rendering a 3-D shapes at specified locations.
class TagArrows {
 public:
  enum class Shape {
    Arrow = 0,    // 3-D arrow pointing down (primary shape).
    Octagon = 1,  // 3-D extruded octagon (stop-sign) shape.
    Paw = 2,      // 3-D extruded pet paw print shape.
  };

  // Vertices allow texturing and color modulation.
  struct ArrowVertex {
    static constexpr DWORD kFvfCode = (D3DFVF_XYZ | D3DFVF_DIFFUSE);

    float x, y, z;   // Transformed position coordinates.
    D3DCOLOR color;  // Color of surfaces.
  };

  explicit TagArrows(IDirect3DDevice8 &device);
  ~TagArrows();

  // Disable copy.
  TagArrows(TagArrows const &) = delete;
  TagArrows &operator=(TagArrows const &) = delete;

  // Primary interface for adding arrows each render. The position is in screen pixel coordinates
  // and specifies the bottom of the tap shape (e.g. tip of the arrow pointing down).
  void QueueTagShape(const Vec3 &position, const D3DCOLOR color, Shape shape = Shape::Arrow, float bearing = 0.f);

  // Renders queued arrows to the screen and clears the queue.
  // Note that the D3D stream source, indices, vertex shader, and texture states
  // are not preserved across this call.
  void FlushQueueToScreen();

  // Releases resources including DirectX. Must call on a DirectX reset / lost device.
  void Release();  // Note: No longer usable after this call (delete object).

  // Print debug information.
  void Dump() const;

 private:
  // Properties of each active Tag Shape (Arrow) stored in the queue.
  struct Arrow {
    Vec3 position;   // In screen coordinates.
    D3DCOLOR color;  // Base color for the vertices.
    Shape shape;     // Shape of the tag.
    float bearing;   // Bearing from player to tag target.
  };

  struct RenderInfo {
    Shape shape = Shape::Arrow;
    D3DCOLOR color = 0;
    int start_vertex_index = -1;
    unsigned int num_vertices = 0;
    unsigned int start_index = 0;
    unsigned int num_primitives = 0;
    float bearing = 0.f;
  };

  void CalculateShapes();           // Calculates the cached, fixed 3D shapes stored in vertices and indices.
  void CalculateArrowVertices();    // Calculates the cached, fixed 3D arrow shape stored in vertices.
  void CalculateOctagonVertices();  // Calculates the cached, fixed 3D octagon shape stored in vertices.
  void CalculatePawVertices();      // Calculates the cached, fixed 3D paw shape stored in vertices.
  bool CreateIndexBuffer();         // Populates the index_buffer LUT for mapping triangles across vertices.
  void AppendArrowIndices();
  void AppendOctagonIndices();
  void AppendPawIndices();
  void RenderQueue();                            // Performs the render of all arrows in queue.
  RenderInfo GetRenderInfo(const Arrow &arrow);  // Returns info to render shape (allocates if needed).
  RenderInfo AllocateArrow(const Arrow &arrow);
  RenderInfo AllocateOctagon(const Arrow &arrow);
  RenderInfo AllocatePaw(const Arrow &arrow);
  int AppendVertices(std::vector<ArrowVertex> &vertices);

  IDirect3DDevice8 &device;
  std::vector<Arrow> arrow_queue;  // Loaded to batch up processing in each render pass.

  // Vertex buffer acts as a cache of most recently used shapes (stored in render_infos).
  IDirect3DVertexBuffer8 *vertex_buffer = nullptr;
  std::vector<RenderInfo> render_infos;  // Lookup table for allocated vertices and indices.

  // Index buffer is a copy of the pre-calculated index mapping to vertices.
  IDirect3DIndexBuffer8 *index_buffer = nullptr;

  // These members are pre-calculated in the constructor.
  std::vector<ArrowVertex> arrow_vertices;  // CPU memory cache of pre-calculated vertices.
  std::vector<ArrowVertex> octagon_vertices;
  std::vector<ArrowVertex> paw_vertices;
  std::vector<int16_t> indices;        // CPU memory cache of pre-calculated indices.
  unsigned int arrow_index_start = 0;  // Set when indices is calculated in constructor.
  unsigned int arrow_primitive_count = 0;
  unsigned int octagon_index_start = 0;
  unsigned int octagon_primitive_count = 0;
  unsigned int paw_index_start = 0;
  unsigned int paw_primitive_count = 0;
};
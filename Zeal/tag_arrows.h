#pragma once

#include "directx.h"
#include "vectors.h"

// Directx 8 compatible classe for rendering a 3-D arrow pointing down at a 3D screen location.
class TagArrows {
 public:
  explicit TagArrows(IDirect3DDevice8 &device);
  ~TagArrows();

  // Disable copy.
  TagArrows(TagArrows const &) = delete;
  TagArrows &operator=(TagArrows const &) = delete;

  // Primary interface for adding arrows each render. The position is in screen pixel coordinates
  // and specifies the bottom tip of the arrow pointing down.
  void QueueArrow(const Vec3 &position, const D3DCOLOR color);

  // Renders queued arrows to the screen and clears the queue.
  // Note that the D3D stream source, indices, vertex shader, and texture states
  // are not preserved across this call.
  void FlushQueueToScreen();

  // Releases resources including DirectX. Must call on a DirectX reset / lost device.
  void Release();  // Note: No longer usable after this call (delete object).

  // Print debug information.
  void Dump() const;

 private:
  // Properties of each active Arrow stored in the queue.
  struct Arrow {
    Vec3 position;   // In screen coordinates.
    D3DCOLOR color;  // Color of all vertices.
  };

  // Vertices allow texturing and color modulation.
  struct ArrowVertex {
    static constexpr DWORD kFvfCode = (D3DFVF_XYZ | D3DFVF_DIFFUSE);

    float x, y, z;   // Transformed position coordinates.
    D3DCOLOR color;  // Color of surfaces.
  };

  // Arrow consists of 3 circles with a vertex at the tip.
  static constexpr int kNumCircleVertices = 60;                         // Spaced every 6 degrees.
  static constexpr int kNumArrowVertices = 3 * kNumCircleVertices + 2;  // Three circles + top and bottom centers.
  static constexpr int kVertexBufferMaxArrowsCount = 8;                 // Cache up to 8 arrow colors.

  void CalculateVertices();                     // Calculates the cached, fixed 3D shape stored in vertices.
  bool CreateIndexBuffer();                     // Populates the index_buffer LUT for mapping triangles across vertices.
  void RenderQueue();                           // Performs the render of all arrows in queue.
  int GetStartVertexIndex(const Arrow &arrow);  // Returns the starting vertex index for the arrow.

  IDirect3DDevice8 &device;
  std::vector<Arrow> arrow_queue;  // Loaded to batch up processing in each render pass.
  IDirect3DVertexBuffer8 *vertex_buffer = nullptr;
  IDirect3DIndexBuffer8 *index_buffer = nullptr;
  int vertex_buffer_wr_index = 0;  // Used for pipelining writes.
  D3DCOLOR vertex_cache_colors[kVertexBufferMaxArrowsCount] = {0};
  int num_indices = 0;                      // Set when index buffer is populated.
  ArrowVertex vertices[kNumArrowVertices];  // CPU memory calculation buffer cache.
};
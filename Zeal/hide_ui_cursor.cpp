#define NOMINMAX
#include "hide_ui_cursor.h"

#include <algorithm>
#include <iterator>

#include "callbacks.h"
#include "commands.h"
#include "directx.h"
#include "game_addresses.h"
#include "game_functions.h"
#include "string_util.h"
#include "zeal.h"

namespace {

// The classic pointer shape on a 12 x 19 unit grid with the tip (the click hotspot) at the origin.
struct Point {
  float x, y;
};

constexpr Point kArrowOutline[] = {{0, 0}, {0, 16}, {4, 12}, {7, 18}, {9, 17}, {6, 11}, {11, 11}};
constexpr int kOutlineCount = sizeof(kArrowOutline) / sizeof(kArrowOutline[0]);
constexpr float kGridHeight = 19.f;

// The (concave) outline split into triangles by index: the head is a fan from the tip and the tail is a quad.
constexpr int kFillTriangles[][3] = {{0, 1, 2}, {0, 2, 5}, {0, 5, 6}, {2, 3, 4}, {2, 4, 5}};
constexpr int kFillTriangleCount = sizeof(kFillTriangles) / sizeof(kFillTriangles[0]);

constexpr float kHeightPixels1080p = 20.f;  // Cursor height at 1080 pixels of screen height.
constexpr float kMinScale = 0.8f;           // Resolution scale clamps (relative to 1080p).
constexpr float kMaxScale = 3.f;
constexpr int16_t kInvalidMouse = 32767;  // The client's marker for an unknown mouse position.

constexpr D3DCOLOR kFillColor = D3DCOLOR_ARGB(255, 255, 255, 255);
constexpr D3DCOLOR kBorderColor = D3DCOLOR_ARGB(255, 0, 0, 0);

// Screen-space vertex (already transformed, so the client's world / view / projection are not involved).
struct CursorVertex {
  static constexpr DWORD kFvfCode = (D3DFVF_XYZRHW | D3DFVF_DIFFUSE);

  float x, y, z, rhw;
  D3DCOLOR color;
};

// The EQ window can be in the background with the UI hidden, so only draw for the focused game window.
bool is_game_window_focused() {
  HWND hwnd = Zeal::Game::get_game_window();
  if (!hwnd) return true;  // Unknown, so assume focused rather than never drawing.
  return ::GetForegroundWindow() == ::GetAncestor(hwnd, GA_ROOT);
}

}  // namespace

HideUiCursor::HideUiCursor(ZealService *zeal) {
  zeal->callbacks->AddGeneric([this]() { CallbackRender(); }, callback_type::RenderUI);

  zeal->commands_hook->Add("/uicursor", {}, "Toggles (or sets on / off) drawing a cursor while the UI is hidden (F10).",
                           [this](std::vector<std::string> &args) {
                             if (args.size() == 2 && Zeal::String::compare_insensitive(args[1], "on"))
                               setting_enabled.set(true);
                             else if (args.size() == 2 && Zeal::String::compare_insensitive(args[1], "off"))
                               setting_enabled.set(false);
                             else if (args.size() == 1)
                               setting_enabled.toggle();
                             else
                               Zeal::Game::print_chat("Usage: /uicursor [on|off]");
                             Zeal::Game::print_chat("Cursor with UI hidden is %s",
                                                    setting_enabled.get() ? "on" : "off");
                             return true;
                           });
}

// Returns true if the client's own cursor is missing and ours should replace it.
bool HideUiCursor::ShouldDraw() const {
  if (!setting_enabled.get() || !Zeal::Game::is_in_game()) return false;
  if (Zeal::Game::is_gui_visible()) return false;           // The client draws its own cursor with the UI visible.
  if (*Zeal::Game::is_right_mouse_look_down) return false;  // Mouse look normally hides the cursor.
  return is_game_window_focused();
}

void HideUiCursor::CallbackRender() {
  if (!ShouldDraw()) return;

  const int16_t mouse_x = *Zeal::Game::mouse_client_x;
  const int16_t mouse_y = *Zeal::Game::mouse_client_y;
  if (mouse_x == kInvalidMouse || mouse_y == kInvalidMouse) return;
  if (mouse_x < 0 || mouse_y < 0 || mouse_x >= Zeal::Game::get_screen_resolution_x() ||
      mouse_y >= Zeal::Game::get_screen_resolution_y())
    return;

  DrawArrow(static_cast<float>(mouse_x), static_cast<float>(mouse_y));
}

// Draws the arrow with its tip at (x, y) in game screen pixels.
void HideUiCursor::DrawArrow(float x, float y) {
  IDirect3DDevice8 *device = ZealService::get_instance()->dx->GetDevice();
  if (!device) return;

  // Scale the shape with the screen height so it stays a similar size on high resolution screens.
  const float screen_x = static_cast<float>(Zeal::Game::get_screen_resolution_x());
  const float screen_y = static_cast<float>(Zeal::Game::get_screen_resolution_y());
  const float scale = kHeightPixels1080p * std::clamp(screen_y / 1080.f, kMinScale, kMaxScale) / kGridHeight;

  CursorVertex outline[kOutlineCount];
  for (int i = 0; i < kOutlineCount; ++i)
    outline[i] = {x + kArrowOutline[i].x * scale, y + kArrowOutline[i].y * scale, 0.f, 1.f, kBorderColor};

  CursorVertex fill[kFillTriangleCount * 3];
  for (int i = 0; i < kFillTriangleCount; ++i) {
    for (int j = 0; j < 3; ++j) {
      fill[i * 3 + j] = outline[kFillTriangles[i][j]];
      fill[i * 3 + j].color = kFillColor;
    }
  }

  CursorVertex border[kOutlineCount + 1];  // Closed line strip.
  std::copy(std::begin(outline), std::end(outline), border);
  border[kOutlineCount] = outline[0];

  // Support temporarily overriding the viewport to full screen mode (same as the bitmap font's full screen mode).
  D3DVIEWPORT8 original_viewport;
  device->GetViewport(&original_viewport);
  const bool modify_viewport =
      (original_viewport.X || original_viewport.Y || original_viewport.Width != static_cast<DWORD>(screen_x) ||
       original_viewport.Height != static_cast<DWORD>(screen_y));
  if (modify_viewport) {
    D3DVIEWPORT8 viewport = {.X = 0,
                             .Y = 0,
                             .Width = static_cast<DWORD>(screen_x),
                             .Height = static_cast<DWORD>(screen_y),
                             .MinZ = original_viewport.MinZ,
                             .MaxZ = original_viewport.MaxZ};
    device->SetViewport(&viewport);
  }

  // Configure for opaque 2D drawing.
  D3DRenderStateStash render_state(*device);
  render_state.store_and_modify({D3DRS_CULLMODE, D3DCULL_NONE});
  render_state.store_and_modify({D3DRS_ALPHABLENDENABLE, FALSE});
  render_state.store_and_modify({D3DRS_ZENABLE, FALSE});  // Rely on render order.
  render_state.store_and_modify({D3DRS_ZWRITEENABLE, FALSE});
  render_state.store_and_modify({D3DRS_LIGHTING, FALSE});

  // Use the vertex colors only (there is no texture).
  D3DTextureStateStash texture_state(*device);
  texture_state.store_and_modify({D3DTSS_COLOROP, D3DTOP_SELECTARG1});
  texture_state.store_and_modify({D3DTSS_COLORARG1, D3DTA_DIFFUSE});

  // Note: Not preserving shader, texture, or stream source to avoid reference counting (same as the other renderers).
  device->SetTexture(0, NULL);
  device->SetVertexShader(CursorVertex::kFvfCode);
  device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, kFillTriangleCount, fill, sizeof(CursorVertex));
  device->DrawPrimitiveUP(D3DPT_LINESTRIP, kOutlineCount, border, sizeof(CursorVertex));

  texture_state.restore_state();
  render_state.restore_state();
  if (modify_viewport) device->SetViewport(&original_viewport);
}

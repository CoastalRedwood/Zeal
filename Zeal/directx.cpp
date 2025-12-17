#include "directx.h"

#include "callbacks.h"
#include "hook_wrapper.h"
#include "zeal.h"

#pragma comment(lib, "d3dx8/d3dx8.lib")  // DirectX math helper library with D3DX functions and classes.

HRESULT WINAPI Local_BeginScene(LPDIRECT3DDEVICE8 pDevice) {
  if (pDevice) {
    static LARGE_INTEGER last_frame = {};
    static LARGE_INTEGER frequency = {};

    if (frequency.QuadPart == 0) QueryPerformanceFrequency(&frequency);

    int fps_limit_val = ZealService::get_instance()->dx->fps_limit.get();
    HRESULT ret = ZealService::get_instance()->hooks->hook_map["BeginScene"]->original(Local_BeginScene)(pDevice);

    if (fps_limit_val > 0) {
      double frame_time = 1.0 / fps_limit_val;  // Desired frame time in seconds
      LARGE_INTEGER current_time;
      QueryPerformanceCounter(&current_time);

      double elapsed_time = (double)(current_time.QuadPart - last_frame.QuadPart) / frequency.QuadPart;
      double sleep_time = frame_time - elapsed_time;

      // Use this while loop for precise waiting, sleep has too much variation
      if (sleep_time > 0) {
        LARGE_INTEGER wait_until;
        QueryPerformanceCounter(&wait_until);
        while (((double)(wait_until.QuadPart - current_time.QuadPart) / frequency.QuadPart) < sleep_time)
          QueryPerformanceCounter(&wait_until);
      }
    }

    QueryPerformanceCounter(&last_frame);
    return ret;
  }
  return ZealService::get_instance()->hooks->hook_map["BeginScene"]->original(Local_BeginScene)(pDevice);
}

HRESULT WINAPI Local_EndScene(LPDIRECT3DDEVICE8 pDevice) {
  HRESULT ret = ZealService::get_instance()->hooks->hook_map["EndScene"]->original(Local_EndScene)(pDevice);
  if (ZealService::get_instance()->callbacks)
    ZealService::get_instance()->callbacks->invoke_generic(callback_type::EndScene);
  return ret;
}

HRESULT WINAPI Local_Reset(IDirect3DDevice8 *pDevice, D3DPRESENT_PARAMETERS *pPresentationParameters) {
  if (ZealService::get_instance()->callbacks)
    ZealService::get_instance()->callbacks->invoke_generic(callback_type::DXReset);
  HRESULT ret =
      ZealService::get_instance()->hooks->hook_map["Reset"]->original(Local_Reset)(pDevice, pPresentationParameters);
  if (ZealService::get_instance()->callbacks)
    ZealService::get_instance()->callbacks->invoke_generic(callback_type::DXResetComplete);
  return ret;
}

void DirectX::InitializeDevice() {
  if (device != nullptr) {
    MessageBoxA(NULL, "D3D device initialization is out of sync, may crash later", "Zeal initialization error",
                MB_OK | MB_ICONWARNING);
    device = nullptr;
  }
  HMODULE gfx_dx8 = GetModuleHandleA("eqgfx_dx8.dll");
  if (!gfx_dx8) {
    MessageBoxA(NULL, "Failed to access gfx dll, may crash later", "Zeal initialization error", MB_OK | MB_ICONWARNING);
    return;
  }

  device = *(IDirect3DDevice8 **)((DWORD)gfx_dx8 + 0xa4f92c);
  uintptr_t *vtable = device ? *(uintptr_t **)device : nullptr;
  if (!device || !vtable) {
    MessageBoxA(NULL, "DirectX device is not available, may crash later", "Zeal initialization error",
                MB_OK | MB_ICONWARNING);
    return;
  }
  DWORD endscene_addr = (DWORD)vtable[35];
  DWORD beginscene_addr = (DWORD)vtable[34];
  DWORD reset_addr = (DWORD)vtable[14];
  ZealService::get_instance()->hooks->Add("EndScene", endscene_addr, Local_EndScene, hook_type_detour);
  ZealService::get_instance()->hooks->Add("BeginScene", beginscene_addr, Local_BeginScene, hook_type_detour);
  ZealService::get_instance()->hooks->Add("Reset", reset_addr, Local_Reset, hook_type_detour);
}

void DirectX::CleanDevice() {
  if (!device) return;  // Nothing to clean.

  // Remove our hooks.
  HMODULE gfx_dx8 = GetModuleHandleA("eqgfx_dx8.dll");
  auto gfx_device = gfx_dx8 ? *(IDirect3DDevice8 **)((DWORD)gfx_dx8 + 0xa4f92c) : nullptr;
  uintptr_t *vtable = *(uintptr_t **)device;
  if (vtable && gfx_device && (gfx_device == device)) {
    ZealService::get_instance()->hooks->Remove("Reset");
    ZealService::get_instance()->hooks->Remove("BeginScene");
    ZealService::get_instance()->hooks->Remove("EndScene");
  } else {
    MessageBoxA(NULL, "Error cleaning up zeal patches in dx8, recommend you close the client", "Zeal shutdown error",
                MB_OK | MB_ICONWARNING);
  }

  device = nullptr;
}

Vec2 DirectX::GetScreenRect() {
  if (!device) return {0, 0};
  D3DVIEWPORT8 viewport;
  device->GetViewport(&viewport);
  return {(float)viewport.Width, (float)viewport.Height};
}

bool IsOffScreen(Vec2 screenPos, const D3DVIEWPORT8 &viewport) {
  return screenPos.x < viewport.X || screenPos.x > viewport.X + viewport.Width || screenPos.y < viewport.Y ||
         screenPos.y > viewport.Y + viewport.Height;
}

bool DirectX::WorldToScreen(Vec3 worldPos, Vec2 &screenPos) {
  GetDevice();
  if (!device) return false;
  // Transform the world coordinates by the combined matrix
  D3DXMATRIX matWorld, matView, matProj;
  D3DVIEWPORT8 viewport;

  device->GetTransform(D3DTS_WORLD, &matWorld);
  device->GetTransform(D3DTS_VIEW, &matView);
  device->GetTransform(D3DTS_PROJECTION, &matProj);
  device->GetViewport(&viewport);
  D3DXVECTOR3 screen;
  D3DXVECTOR3 d3dPOS = {worldPos.x, worldPos.y, worldPos.z};
  D3DXVec3Project(&screen, &d3dPOS, &viewport, &matProj, &matView, &matWorld);
  screenPos.x = screen.y;
  screenPos.y = screen.x;
  return true;
}

// int RenderPartialScene(float a, int* b, int c, int d)
//{
//     if (ZealService::get_instance()->callbacks)
//         ZealService::get_instance()->callbacks->invoke_generic(callback_type::EndScene);
//     ZealService::get_instance()->hooks->hook_map["RenderPartialScene"]->original(RenderPartialScene)(a, b, c, d);
// }

static void __fastcall CDisplayInitDDraw(int this_display, int unused_edx) {
  ZealService::get_instance()->hooks->hook_map["CDisplayInitDDraw"]->original(CDisplayInitDDraw)(this_display,
                                                                                                 unused_edx);
  ZealService::get_instance()->dx->InitializeDevice();  // Plug in hooks now device should exist.
}

static void __fastcall CDisplayCleanUpDDraw(int this_display, int unused_edx) {
  if (ZealService::get_instance()->callbacks)
    ZealService::get_instance()->callbacks->invoke_generic(callback_type::DXCleanDevice);
  ZealService::get_instance()->dx->CleanDevice();  // Unplug before device is cleaned below.
  ZealService::get_instance()->hooks->hook_map["CDisplayCleanUpDDraw"]->original(CDisplayCleanUpDDraw)(this_display,
                                                                                                       unused_edx);
}

// The DirectX interface is initialized after the Zeal DLL is loaded (even the first time) and then cleaned up
// (deleted) when dropping back to the login screen. So we add hooks in those functions to insert and remove
// our hooks.
DirectX::DirectX() {
  ZealService::get_instance()->hooks->Add("CDisplayInitDDraw", 0x004a5171, CDisplayInitDDraw, hook_type_detour);
  ZealService::get_instance()->hooks->Add("CDisplayCleanUpDDraw", 0x004a954b, CDisplayCleanUpDDraw, hook_type_detour);
}
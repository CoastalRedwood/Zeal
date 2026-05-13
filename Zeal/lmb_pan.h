#pragma once
#include <cstdint>

namespace lmb_pan {

// Install the chase-camera vtable slot 0x08 patch. Returns true on success,
// false if the signature gate failed (slot didn't hold the expected original
// pointer — unknown eqgame.exe build, or already patched). Must be called
// after dllmain.cpp's handle_process_attach() has populated aslr_delta.
bool install(uintptr_t aslr_delta);

// Per-frame offsets applied to the chase camera around the original slot 0x08
// call. Mutated by the input poll (wired in a follow-up commit). The MVP
// initial build hardcodes g_yaw_offset to a visible test value to verify the
// architecture in one launch; input wiring replaces it with the actual
// LMB-no-RMB-driven state. Units: EQ heading units (512 = full turn).
extern float g_yaw_offset;
extern float g_pitch_offset;

}  // namespace lmb_pan

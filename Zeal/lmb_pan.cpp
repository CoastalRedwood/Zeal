#include "lmb_pan.h"

#include <Windows.h>

#include <cstdint>

// Phase 1 / R2 Layer 2 / LMB-pan implementation. See:
//   memory/PHASE1_CAMERA.md
//   memory/project_zeal_rof2_addresses.md (Layer 2 section)
//
// Session 14 empirical mapping (overriding Session 13's Zeal-inherited labels):
//   - Mode 6 (S13: "overhead/TotalCameras") = vanilla scroll-out 3rd-person.
//     Activated by scrolling out from 1st-person; stays in mode 6 for the
//     entire 3rd-person zoom range. **Our LMB-pan target.**
//   - Mode 0 (S13: "FirstPerson") = 1st-person view (camera at character head).
//   - Mode 7 (3/4/7 shared class) = character select.
//   - Modes 1/2/3/4/8 = F9-cycle alts (mode 2 in particular = where Zeal
//     upstream installs `ZealCam` — emphatically NOT our target).
//
// Hook: chase camera (mode 6) class vtable at Ghidra 0x009d1188, slot 0x08
// at vtable + 8 = 0x009d1190, storing pointer to FUN_00799140 (the per-frame
// camera-position compute). We replace the slot with mode_6_wrapper, which
// polls LMB-no-RMB input → updates g_yaw_offset → optionally applies the
// offset before chaining to the original.
//
// Yaw strategy = "Approach E" (Session 14 design). FUN_00799140 maintains the
// camera's heading as a delta from entity rotation:
//   cam[0x38]_new = cam[0x38]_pre + (entity_heading - cam[0x48])    (delta path)
//   cam[0x38]_new = entity_heading                                  (snap path,
//                                                                    DAT_00ddf703!=0)
// then runs the anchor compute using cam[0x38] (not entity_heading) to place
// the camera behind a virtual direction. By pre-setting cam[0x48] =
// entity_heading and cam[0x38] = entity_heading + g_yaw_offset, the delta
// path produces cam[0x38] = entity_heading + g_yaw_offset post-original, and
// the anchor compute orbits the camera by g_yaw_offset around the entity
// without ever touching the entity's actual heading.
//
// When g_yaw_offset == 0 the wrapper is a pure pass-through (no cam state
// mutation). Vanilla 3rd-person is byte-identical to no-patch.
//
// Snap-path caveat: when DAT_00ddf703 != 0, the original overwrites cam[0x38]
// with entity_heading and our offset is lost for that frame. Single-frame
// visual glitch on transitions; likely imperceptible.
//
// Input model for this commit (minimal): per-frame poll inside the wrapper.
// LMB held without RMB → accumulate mouse X delta into g_yaw_offset. LMB
// released → snap offset to 0 immediately. No persistence after release, no
// smooth snap-back lerp, no click-vs-pan disambiguation. Those iterate on
// top once the basic pan motion is verified.
//
// Pitch is NOT handled this commit. Mode 6's anchor compute does not read
// cam[0x2c] (the pitch field used by mode 2); pitch will need a different
// injection point and is deferred.

#define ZEAL_ROF2_R3_LMB_PAN_DIAGNOSE 1

namespace lmb_pan {

// Mutated by poll_input() each frame the wrapper runs. Defaults 0 = vanilla.
// Units: EQ heading units (512 = full turn).
float g_yaw_offset = 0.0f;
float g_pitch_offset = 0.0f;  // unused this commit (mode 6 pitch deferred)

// Ghidra static addresses (preferred base 0x00400000). ASLR-adjusted at
// runtime via the delta passed to install().
static const uintptr_t k_mode_6_vtable_slot_08_ghidra = 0x009d1190;
static const uintptr_t k_mode_6_slot_08_original_ghidra = 0x00799140;

// Camera-object field offsets, from Session 14 decompile of FUN_00799140.
static const size_t k_cam_heading_offset = 0x38;              // field [0xe]
static const size_t k_cam_prev_entity_heading_offset = 0x48;  // field [0x12]

// Entity field offset for heading. entity[0x20] as int* → byte offset 0x80.
static const size_t k_entity_heading_byte_offset = 0x80;

using slot_08_fn = void(__fastcall *)(void *cam, int edx_unused, int *entity);
static slot_08_fn g_original = nullptr;

// Stored at install time so the wrapper can read DAT_00ddf703 (the heading-
// update path selector) without re-deriving the ASLR delta.
static uintptr_t g_aslr_delta = 0;
static const uintptr_t k_dat_00ddf703_ghidra = 0x00ddf703;

// Input state machine. Click-vs-pan disambiguation:
//   IDLE     — LMB not held (or LMB+RMB, which is vanilla autorun).
//   PENDING  — LMB just pressed alone; not yet hidden cursor or applied yaw.
//              Waiting for motion threshold to decide pan-vs-click.
//   PANNING  — Motion threshold crossed; cursor hidden, yaw accumulating.
//
// Transitions:
//   IDLE → PENDING:    LMB pressed, RMB not pressed.
//   PENDING → IDLE:    LMB released (was a click — no cursor hide, no yaw).
//   PENDING → IDLE:    RMB pressed (LMB+RMB = autorun, not pan).
//   PENDING → PANNING: cursor moved >= 4px from LMB-down position.
//   PANNING → IDLE:    LMB released OR RMB pressed. Restore cursor + snap yaw.
//
// No time threshold: holding LMB still without moving doesn't auto-engage
// pan. That avoids cursor flicker on slow UI clicks (e.g., 200ms button
// press) that don't intend to pan.
enum class lmb_state {
  IDLE,
  PENDING,
  PANNING,
};

static lmb_state g_state = lmb_state::IDLE;
static POINT g_lmb_down_cursor = {0, 0};  // Where LMB was first pressed (return-to anchor).
static POINT g_prev_cursor = {0, 0};      // Frame-to-frame ref during PANNING.
// ShowCursor is reference-counted (per Windows). EQ may have the count
// pushed above 0; a single ShowCursor(FALSE) only decrements once. We loop
// until the count goes < 0, tracking how many decrements we applied so we
// can balance them on release.
static int g_hides_applied = 0;

// Pixel-distance threshold before LMB-down commits to a pan (vs being a
// click). WoW uses a "tiny hidden distance" — just enough to filter hand
// tremor on a deliberate click, not a deliberate intent gate. Starting at
// 1px (most responsive — single-pixel motion engages). Bump up only if
// hand-tremor false-positives trigger pans during normal targeting/UI clicks.
static const int k_pan_pixel_threshold = 1;

// Sensitivity: yaw units per pixel of cursor X movement. EQ uses 512 = full
// turn, so 1.0 means 100px drag ≈ 70° rotation. Starting high to make the
// effect obviously visible; tune down if too aggressive.
static const float k_mouse_to_yaw_units = 1.0f;

// Anti-spike: clamp the per-frame delta to avoid jumps on mode-switch re-
// entry (wrapper is called only in mode 6; cursor may have moved a lot
// while we were in a different mode).
static const int k_max_dx_per_frame = 50;

static void enter_panning() {
  // Use the LMB-down cursor as the frame-to-frame reference so the motion
  // that crossed the threshold (and got us into PANNING) is applied as the
  // first frame's yaw delta — no "lost" pixels at the start of a pan.
  g_prev_cursor = g_lmb_down_cursor;
  g_hides_applied = 0;
  for (int attempts = 0; attempts < 16; ++attempts) {
    const int new_count = ShowCursor(FALSE);
    g_hides_applied++;
    if (new_count < 0) break;
  }
  g_state = lmb_state::PANNING;
}

static void exit_to_idle_restoring_cursor() {
  SetCursorPos(g_lmb_down_cursor.x, g_lmb_down_cursor.y);
  while (g_hides_applied > 0) {
    ShowCursor(TRUE);
    g_hides_applied--;
  }
  g_yaw_offset = 0.0f;
  g_state = lmb_state::IDLE;
}

static void poll_input() {
  const bool lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
  const bool rmb = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

  switch (g_state) {
    case lmb_state::IDLE:
      if (lmb && !rmb) {
        GetCursorPos(&g_lmb_down_cursor);
        g_state = lmb_state::PENDING;
      }
      break;
    case lmb_state::PENDING:
      if (!lmb || rmb) {
        // LMB released before threshold = click (no cursor hide, no yaw).
        // OR RMB pressed = LMB+RMB autorun (also not pan). Return to IDLE.
        g_state = lmb_state::IDLE;
      } else {
        POINT current;
        GetCursorPos(&current);
        int dx = current.x - g_lmb_down_cursor.x;
        int dy = current.y - g_lmb_down_cursor.y;
        if (dx < 0) dx = -dx;
        if (dy < 0) dy = -dy;
        if (dx >= k_pan_pixel_threshold || dy >= k_pan_pixel_threshold) {
          enter_panning();
        }
      }
      break;
    case lmb_state::PANNING:
      if (!lmb || rmb) {
        exit_to_idle_restoring_cursor();
      } else {
        POINT current;
        GetCursorPos(&current);
        int dx = current.x - g_prev_cursor.x;
        if (dx > k_max_dx_per_frame) dx = k_max_dx_per_frame;
        else if (dx < -k_max_dx_per_frame) dx = -k_max_dx_per_frame;
        // WoW spec: mouse RIGHT rotates camera COUNTER-CLOCKWISE (camera to
        // the player's LEFT). Subtraction, not addition.
        g_yaw_offset -= static_cast<float>(dx) * k_mouse_to_yaw_units;
        if (g_yaw_offset > 256.0f) g_yaw_offset -= 512.0f;
        else if (g_yaw_offset < -256.0f) g_yaw_offset += 512.0f;
        g_prev_cursor = current;
      }
      break;
  }
}

static void __fastcall mode_6_wrapper(void *cam, int /*edx_unused*/, int *entity) {
  poll_input();

  if (g_yaw_offset != 0.0f) {
    char *const cam_bytes = reinterpret_cast<char *>(cam);
    const float entity_heading =
        *reinterpret_cast<float *>(reinterpret_cast<char *>(entity) +
                                   k_entity_heading_byte_offset);

    // Approach F (Session 14 diagnostic finding): DAT_00ddf703 is 1 in normal
    // play, which makes FUN_00799140 take the SNAP path:
    //     cam[0x38] = entity_heading
    // unconditionally. That overrides our pre-set every frame. To make our
    // pre-set survive, temporarily force DAT_00ddf703 = 0 so the delta path
    // runs:
    //     cam[0x38] = (entity_heading - cam[0x48]) + cam[0x38]_pre
    //               = (entity_heading - entity_heading) + (entity_heading + offset)
    //               = entity_heading + offset      ← survives, anchor uses this.
    // Restore DAT_00ddf703 after the call so other code that depends on it
    // sees the original value next frame.
    unsigned char *const dat_703_ptr = reinterpret_cast<unsigned char *>(
        k_dat_00ddf703_ghidra + g_aslr_delta);
    const unsigned char saved_dat_703 = *dat_703_ptr;
    *dat_703_ptr = 0;

    // Force the original's delta math to produce cam[0x38] = entity_heading +
    // g_yaw_offset.
    *reinterpret_cast<float *>(cam_bytes + k_cam_prev_entity_heading_offset) =
        entity_heading;
    *reinterpret_cast<float *>(cam_bytes + k_cam_heading_offset) =
        entity_heading + g_yaw_offset;

    g_original(cam, 0, entity);

    *dat_703_ptr = saved_dat_703;
  } else {
    g_original(cam, 0, entity);
  }
}

bool install(uintptr_t aslr_delta) {
  g_aslr_delta = aslr_delta;

  const uintptr_t slot_runtime = k_mode_6_vtable_slot_08_ghidra + aslr_delta;
  const uintptr_t expected_runtime = k_mode_6_slot_08_original_ghidra + aslr_delta;
  uintptr_t *const slot_ptr = reinterpret_cast<uintptr_t *>(slot_runtime);
  const uintptr_t actual = *slot_ptr;
  const bool match = (actual == expected_runtime);

#if ZEAL_ROF2_R3_LMB_PAN_DIAGNOSE
  char msg[1024];
  wsprintfA(msg,
            "Mode 6 (vanilla 3rd-person) vtable slot 0x08 patch:\r\n\r\n"
            "Vtable slot runtime: 0x%08x\r\n"
            "Expected original (FUN_00799140 + delta): 0x%08x\r\n"
            "Actual value at slot: 0x%08x\r\n\r\n"
            "Signature match: %s\r\n"
            "Decision: %s",
            (unsigned)slot_runtime, (unsigned)expected_runtime, (unsigned)actual,
            match ? "YES" : "NO", match ? "INSTALL" : "SKIP (silent no-op)");
  MessageBoxA(NULL, msg, "Zeal-RoF2 R3 / LMB-pan install diagnostic",
              MB_OK | MB_ICONINFORMATION);
#endif

  if (!match) return false;

  g_original = reinterpret_cast<slot_08_fn>(actual);

  DWORD old_protect;
  VirtualProtect(slot_ptr, sizeof(uintptr_t), PAGE_READWRITE, &old_protect);
  *slot_ptr = reinterpret_cast<uintptr_t>(&mode_6_wrapper);
  VirtualProtect(slot_ptr, sizeof(uintptr_t), old_protect, &old_protect);
  FlushInstructionCache(GetCurrentProcess(), slot_ptr, sizeof(uintptr_t));

  return true;
}

}  // namespace lmb_pan

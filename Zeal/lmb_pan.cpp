// fopen / ctime are MSVC-deprecated in favor of fopen_s / ctime_s, but the
// secure variants don't buy us anything for a single-threaded append-only log
// where the file path is a compile-time constant. Define before any includes.
#define _CRT_SECURE_NO_WARNINGS

#include "lmb_pan.h"

#include <Windows.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <ctime>

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
// Yaw strategy = "Approach F" (Session 14 — supersedes Approach E which left
// our pre-set cam[0x38] subject to override by the original's snap path).
// FUN_00799140 maintains the camera's heading as a delta from entity rotation:
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
// When LMB-pan is not active (g_state == IDLE, g_yaw_offset == 0, no pitch
// restore pending), the wrapper is a pure pass-through (no cam state
// mutation). Vanilla 3rd-person is byte-identical to no-patch.
//
// Snap-path caveat (yaw): when DAT_00ddf703 != 0, the original overwrites
// cam[0x38] with entity_heading and our offset is lost for that frame.
// Single-frame visual glitch on transitions; likely imperceptible.
//
// Input model: per-frame poll inside the wrapper with click-vs-pan
// disambiguation. State machine (Session 15 r2 spec — supersedes Session 14
// "no-persistence" spec at user request):
//   IDLE     — no offset, no LMB held.
//   PENDING  — LMB pressed; awaiting motion threshold to decide click-vs-pan.
//   PANNING  — cursor hidden; dx → g_yaw_offset, dy → g_pitch_offset
//              accumulating each frame.
//   HELD     — LMB released after a pan; offsets PERSIST so the camera holds
//              its new angle. Cursor restored to LMB-down position. Vanilla
//              clicks (target select etc.) still work in HELD.
// Offsets ONLY reset on RMB press (PANNING→IDLE or HELD→IDLE). That's the
// explicit snap-back affordance. LMB pressed from HELD re-engages PENDING
// without resetting offsets — additional pan stacks on the held angle.
// No smooth snap-back lerp; instant transition on RMB.
//
// Pitch strategy (Session 15, "Approach A"): cam[0x30] is the pitch input —
// READ by FUN_00799140 (trig on cam[0x30] feeds Z component via
// z_target = fStack_c - sin/cos(cam[0x30]) * cam[0x34]) but NEVER WRITTEN
// by it. So we can pre-set cam[0x30] before chaining and the position
// compute uses our value. Unlike yaw's cam[0x38] (which the original
// overwrites every frame from entity_heading via Approach F's delta math),
// cam[0x30] persists across frames — so we snapshot vanilla pitch on
// pan-enter into g_pitch_base and restore it on pan-exit, otherwise residual
// pitch would survive after LMB release. Snapshot/restore happen inside the
// wrapper since only it has the cam pointer (enter_panning /
// exit_to_idle_restoring_cursor set flags that the wrapper acts on).
//
// Session 14's memory file labeled cam[0x30] "head-height tracker"; the
// FUN_00799140 decompile (Session 15) corrected that — cam[0x30] is pitch.
// cam[0x14] = cam[0x30] (or cam[0x30] - 8.5) is the head-height-like output
// derived from pitch by the original at function tail.

// Set to 1 to enable install-time MessageBoxes + per-pan file logging at
// D:\EQEmu\Full_RoF2\lmb_pan_diag.log. Friend builds ship with this OFF —
// the install popup would otherwise fire every DLL load, and the log file
// would grow unbounded across play sessions.
#define ZEAL_ROF2_R3_LMB_PAN_DIAGNOSE 0
// Sub-flag: the [pan-frame N] per-wrapper-call log fires from inside the
// camera wrapper, which runs multiple times per visual frame. With the
// per-call fflush in diag_logf this caused enough synchronous disk I/O to
// visibly throttle the renderer ("camera feels wild / spins erratically").
// Default OFF so the lighter state-transition logs can stay enabled. Flip to
// 1 only when investigating per-frame yaw/pitch math.
#define ZEAL_ROF2_R3_LMB_PAN_DIAGNOSE_PER_FRAME 0

namespace lmb_pan {

// Mutated by poll_input() each frame the wrapper runs. Defaults 0 = vanilla.
// g_yaw_offset units: EQ heading units (512 = full turn).
// g_pitch_offset units: same as cam[0x30]'s units (TBD empirically — see
// k_mouse_to_pitch_units comment).
float g_yaw_offset = 0.0f;
float g_pitch_offset = 0.0f;

// Ghidra static addresses (preferred base 0x00400000). ASLR-adjusted at
// runtime via the delta passed to install().
static const uintptr_t k_mode_6_vtable_slot_08_ghidra = 0x009d1190;
static const uintptr_t k_mode_6_slot_08_original_ghidra = 0x00799140;

// Camera-object field offsets, from Session 14 decompile of FUN_00799140.
static const size_t k_cam_heading_offset = 0x38;              // field [0xe]
static const size_t k_cam_prev_entity_heading_offset = 0x48;  // field [0x12]
// Pitch input — fed to trig in the original's Z-math (Session 15 decompile).
// Read but never written by FUN_00799140.
static const size_t k_cam_pitch_offset = 0x30;                // field [0xc]
// Position-output offsets (cam[1..3]), used by the first-pitch-apply diagnostic.
static const size_t k_cam_pos_x_offset = 0x04;
static const size_t k_cam_pos_y_offset = 0x08;
static const size_t k_cam_pos_z_offset = 0x0c;

// Entity field offset for heading. entity[0x20] as int* → byte offset 0x80.
static const size_t k_entity_heading_byte_offset = 0x80;

using slot_08_fn = void(__fastcall *)(void *cam, int edx_unused, int *entity);
static slot_08_fn g_original = nullptr;

// Stored at install time so the wrapper can read DAT_00ddf703 (the heading-
// update path selector) without re-deriving the ASLR delta.
static uintptr_t g_aslr_delta = 0;
static const uintptr_t k_dat_00ddf703_ghidra = 0x00ddf703;

// Runtime shadow of MouseSensitivity bucket (1-8). Session 11 decode:
//   loadOptions tail copies ini storage DAT_00de0be4 → runtime shadow
//   DAT_00ddf69c after the [1,8] clamp. procMouse (FUN_00516d40) reads the
//   shadow and applies mult = ((bucket-1)/7) * 1.5 + 0.5 (= 0.5x..2.0x).
// We read the same shadow per-frame so LMB-pan tracks RMB-drive: when the
// user nudges the in-game slider, both pipelines scale together.
static const uintptr_t k_dat_mouse_sens_bucket_ghidra = 0x00ddf69c;

static float read_sens_mult() {
  const int raw = *reinterpret_cast<int *>(k_dat_mouse_sens_bucket_ghidra + g_aslr_delta);
  const int bucket = (raw < 1) ? 1 : (raw > 8 ? 8 : raw);
  return (static_cast<float>(bucket - 1) / 7.0f) * 1.5f + 0.5f;
}

// True when EQ's main window (any window in our process) currently has
// Windows foreground focus. Used to gate input polling so that when EQ is
// in the background (e.g. user alt-tabbed to another app), we don't:
//   - read GetAsyncKeyState(VK_LBUTTON) — it's a GLOBAL state, so clicks in
//     other windows would otherwise drive us into PENDING/PANNING.
//   - leave ClipCursor active — it would trap the cursor inside the EQ
//     window even while the user is interacting with other applications
//     (breaks text selection, screenshot tools, etc. — user-reported bug
//     after v1.2.0 shipped).
static bool eq_has_foreground_focus() {
  const HWND fg = GetForegroundWindow();
  if (!fg) return false;
  DWORD fg_pid = 0;
  GetWindowThreadProcessId(fg, &fg_pid);
  return fg_pid == GetCurrentProcessId();
}

// Input state machine. Click-vs-pan disambiguation + persistent offset
// (Session 15 r2). Offsets reset ONLY on RMB-triggered snap-back; LMB
// release after a pan transitions to HELD (offset persists).
//   IDLE     — LMB not held; no offset.
//   PENDING  — LMB just pressed (from IDLE or HELD); cursor not yet hidden;
//              no offset application change this frame. Waiting for motion
//              threshold to decide pan-vs-click.
//   PANNING  — motion threshold crossed; cursor hidden; offsets accumulating.
//   HELD     — LMB released after a pan. Cursor restored. Offsets persist.
//              cam[0x30] still pre-set each frame so the held pitch stays
//              applied. Vanilla clicks (target select, autorun) still work.
//
// Transitions:
//   IDLE    → PENDING:  LMB pressed, RMB not pressed, [TODO: not over UI].
//   PENDING → IDLE:     LMB released before threshold (was a click).
//   PENDING → IDLE:     RMB pressed (LMB+RMB = autorun, not pan).
//   PENDING → PANNING:  cursor moved ≥ k_pan_pixel_threshold (1 px).
//   PANNING → HELD:     LMB released. Restore cursor; KEEP offsets.
//   PANNING → IDLE:     RMB pressed mid-pan. Restore cursor; SNAP-BACK
//                       offsets to 0; schedule pitch restore.
//   HELD    → PENDING:  LMB pressed again. Re-engage pan; KEEP offsets so
//                       further drag stacks on top of the held angle.
//   HELD    → IDLE:     RMB pressed. SNAP-BACK offsets to 0; pitch restore.
//
// No time threshold: holding LMB still without moving doesn't auto-engage
// pan. That avoids cursor flicker on slow UI clicks (e.g., 200ms button
// press) that don't intend to pan.
enum class lmb_state {
  IDLE,
  PENDING,
  PANNING,
  HELD,
};

static lmb_state g_state = lmb_state::IDLE;
static POINT g_lmb_down_cursor = {0, 0};  // Where LMB was first pressed (return-to anchor).
static POINT g_prev_cursor = {0, 0};      // Frame-to-frame ref during PANNING.
// ShowCursor is reference-counted (per Windows). EQ may have the count
// pushed above 0; a single ShowCursor(FALSE) only decrements once. We loop
// until the count goes < 0, tracking how many decrements we applied so we
// can balance them on release.
static int g_hides_applied = 0;

// PITCH state. cam[0x30] persists across frames (the original reads but
// doesn't write it), so we snapshot vanilla pitch on pan-enter and restore
// it on pan-exit. Snapshot/restore happen INSIDE the wrapper (where the cam
// pointer is in scope), gated by flags set elsewhere.
static float g_pitch_base = 0.0f;       // Snapshot of cam[0x30] on pan-enter.
static bool g_pitch_snapshotted = false; // True once we've captured g_pitch_base.
static bool g_pitch_restore_pending = false; // Set on exit; wrapper restores once.

// Cursor-edge recenter state. When cursor approaches game-window edge,
// SetCursorPos warps it back to window center so pan can continue past the
// screen-edge OS clamp. Multi-call-per-frame race: SetCursorPos doesn't
// synchronously update GetCursorPos — secondary calls in the same frame
// will still see the pre-set position. Track the pre-set position and skip
// delta processing while the cursor still reports it (race in progress).
static POINT g_pre_set_cursor = {0, 0};
static bool g_recenter_pending = false;

// Post-snap-back diagnostic: after RMB triggers a snap-back out of PANNING
// or PENDING-with-offsets, log the next N wrapper frames. We want to confirm
// whether EQ's "both buttons held = autorun forward" engages while the user
// is still physically holding LMB+RMB. If autorun engaged we'll see cam[1..3]
// (position) drift forward across frames; if not, position stays steady.
// Also captures the raw GetAsyncKeyState(VK_LBUTTON/VK_RBUTTON) bits so we
// can confirm Windows still reports both as held post-snap-back.
static int g_post_snap_log_remaining = 0;
static const int k_post_snap_log_frames = 90;  // ~1.5s at 60fps

// Pixel-distance threshold before LMB-down commits to a pan (vs being a
// click). WoW uses a "tiny hidden distance" — just enough to filter hand
// tremor on a deliberate click, not a deliberate intent gate. Starting at
// 1px (most responsive — single-pixel motion engages). Bump up only if
// hand-tremor false-positives trigger pans during normal targeting/UI clicks.
static const int k_pan_pixel_threshold = 1;

// Sensitivity: yaw units per pixel of cursor X movement. EQ uses 512 = full
// turn, so 0.2 means 100px drag ≈ 14° rotation. Calibrated Session 15 to
// roughly match the in-game RMB-drive feel at a mid-bucket MouseSensitivity
// slider position; user retunes from here.
static const float k_mouse_to_yaw_units = 0.2f;

// Anti-spike: clamp the per-frame delta to avoid jumps on mode-switch re-
// entry (wrapper is called only in mode 6; cursor may have moved a lot
// while we were in a different mode).
static const int k_max_dx_per_frame = 50;
static const int k_max_dy_per_frame = 50;

// Pitch sensitivity — cam[0x30] units per pixel. cam[0x30]'s real unit is
// still unconfirmed (the FUN_00799140 decompile reads it through a trig
// vtable so could be radians or degrees), so the ratio to yaw is empirical
// rather than analytical. Tuning history:
//   0.5  (Session 14 initial — too fast, not yet validated)
//   0.1  (Session 15 first cut — felt slightly faster than yaw per-pixel)
//   0.08 (Session 15 reduce — overshot once RMB-drive was aspect-corrected)
//   0.09 (Session 15 final — user-confirmed feel after both fixes landed)
static const float k_mouse_to_pitch_units = 0.09f;

// Pitch clamp range. cam[0x2c] (a sibling field, not the one we use) is
// clamped to [0, 30] in mode 5 — circumstantial evidence that EQ pitches
// live in roughly the [0, 30] range. We allow ±60 here (wider than circum-
// stantial guess) so first empirical test can see whether cam[0x30] reaches
// position compute at all; narrow once we know the working range.
static const float k_pitch_offset_min = -60.0f;
static const float k_pitch_offset_max =  60.0f;

// --- File-log diagnostics ---
// Why a file log instead of MessageBox: the wrapper hides the cursor on
// pan-enter (looped ShowCursor(FALSE) to defeat EQ's positive refcount), and
// MessageBoxA blocks the same thread that owns the cursor state. User sees
// a modal dialog with no cursor and can't dismiss it without alt-tab / Esc.
// File log captures per-frame data without UI interference; user reads it
// after the test instead of trying to click through it during.
//
// Install-time MessageBox stays in install() because that runs pre-game
// (before any cursor hide), so it's still safely clickable.
static FILE *g_diag_log = nullptr;

// NOTE: no per-call fflush. Calling fflush on every line caused a
// synchronous disk write per call — and the wrapper runs multiple times per
// visual frame, so when this fires from the per-frame log it visibly
// throttles the renderer (Bitdefender scanning the appended bytes makes it
// worse). The stdio default line/full buffering is fine here. Call
// diag_flush() at meaningful boundaries (install end, snap-back, post-snap
// finish) if you want the on-disk file caught up immediately.
static void diag_logf(const char *fmt, ...) {
  if (!g_diag_log) {
    g_diag_log = fopen("D:\\EQEmu\\Full_RoF2\\lmb_pan_diag.log", "a");
    if (!g_diag_log) return;
    const time_t now = time(nullptr);
    fprintf(g_diag_log, "\n=== Zeal.asi load: %s", ctime(&now));
  }
  va_list ap;
  va_start(ap, fmt);
  vfprintf(g_diag_log, fmt, ap);
  va_end(ap);
}

static void diag_flush() {
  if (g_diag_log) fflush(g_diag_log);
}

// UI hit-test (Session 15). Returns true when the LMB-down event should NOT
// engage pan because the cursor is over a vanilla EQ UI window (so the click
// can reach a slider, button, etc.). Mirrors upstream Zeal's
// is_game_ui_window_hovered() — reads *WndManager → CXWndManager → Hovered.
//
// RoF2 address discovery (Session 15, static analysis of eqgame.exe):
//   - RTTI ".?AVCXWndManager@@" name at .data file 0x762E88 (VA 0x00B64C88)
//   - Type Descriptor at VA 0x00B64C80
//   - Complete Object Locator referencing TD at VA 0x00A51D1C (.rdata)
//   - CXWndManager base vtable at VA 0x00A1A338 (only one `MOV [ESI], vt`
//     site in the binary, inside ctor at VA 0x008772D0 — prolog PUSH -1)
//   - Derived UI manager ctor at VA 0x00666B80 (calls base ctor, then
//     writes derived vtable 0x009E7728 over [EDI])
//   - Single caller of derived ctor at VA 0x0048E644; the bytes
//     immediately following are `MOV [0x015D3D00], EAX` storing the new
//     object pointer to a .data global → that global IS WndManager.
//
// CXWndManager.Hovered offset: empirically 0x70 in RoF2 (Session 15 dumps,
// second-pass calibration).
//
// Upstream 2002's CXWndManager (see game_ui.h) had:
//   /* 0x0030 */ SidlWnd *Focused;     (after-click sticky)
//   /* 0x003C */ SidlWnd *Hovered;     (current cursor)
//
// First-pass landed on 0x5C, which turned out to be RoF2's Focused (sticky
// once set, persisted after the user clicked off the UI window — the user
// confirmed pan stayed blocked even on subsequent world clicks). Second-pass
// dump comparison across the same 18 events:
//   - 0x5C/0x64: heap ptr after first UI click, persistent through dumps
//     12-18 even when subsequent clicks were on world → Focused/Active.
//   - 0x70:      heap ptr ONLY when cursor over UI at LMB-down time; clears
//     on world clicks → Hovered (current cursor pointing-at semantics).
//   - 0x74:      tracks 0x70 most of the time, occasional discrepancies →
//     likely a secondary mouse-capture field.
//
// Class layout shifted ~0x40 bytes between 2002 and RoF2, almost certainly
// because the base CXWnd class added members in newer expansions.
static const uintptr_t k_wnd_mgr_global_ghidra = 0x015D3D00;
static const size_t k_cxwndmgr_hovered_offset = 0x70;

static bool is_mouse_over_ui_window() {
  void *const mgr =
      *reinterpret_cast<void **>(k_wnd_mgr_global_ghidra + g_aslr_delta);
  if (mgr == nullptr) return false;
  void *const hovered = *reinterpret_cast<void **>(
      reinterpret_cast<char *>(mgr) + k_cxwndmgr_hovered_offset);
  const bool over_ui = (hovered != nullptr);
#if ZEAL_ROF2_R3_LMB_PAN_DIAGNOSE
  // Log first 10 hit-test calls with all four candidate field values so we
  // can verify 0x70 is still the right choice (vs 0x5C / 0x68 / 0x74) on
  // re-test. After 10 calls, log goes silent.
  static int s_diag_count = 0;
  if (s_diag_count < 10) {
    s_diag_count++;
    const unsigned *p = reinterpret_cast<const unsigned *>(mgr);
    diag_logf("[hit-test #%d] mgr=0x%x  0x5c=%08x 0x64=%08x 0x68=%08x "
              "0x70=%08x 0x74=%08x  decision=%s\n",
              s_diag_count, (unsigned)(uintptr_t)mgr,
              p[0x5C / 4], p[0x64 / 4], p[0x68 / 4],
              p[0x70 / 4], p[0x74 / 4],
              over_ui ? "SUPPRESS" : "ALLOW");
  }
#endif
  return over_ui;
}

static void enter_panning() {
  // Use the LMB-down cursor as the frame-to-frame reference so the motion
  // that crossed the threshold (and got us into PANNING) is applied as the
  // first frame's yaw/pitch delta — no "lost" pixels at the start of a pan.
  g_prev_cursor = g_lmb_down_cursor;
  g_hides_applied = 0;
  for (int attempts = 0; attempts < 16; ++attempts) {
    const int new_count = ShowCursor(FALSE);
    g_hides_applied++;
    if (new_count < 0) break;
  }
  // Multi-monitor safety: trap the (hidden) cursor inside the game window
  // so it can't wander onto a second monitor mid-pan. Released on PANNING
  // exit (enter_held_keeping_offset / exit_to_idle_snapping_back).
  {
    HWND fg = GetForegroundWindow();
    RECT wr;
    if (fg && GetWindowRect(fg, &wr)) {
      ClipCursor(&wr);
    }
  }
  // Persistence: do NOT reset g_pitch_offset or g_pitch_snapshotted here.
  // They carry over from any prior HELD state, so re-engaging a pan stacks
  // on top of the previously-held angle. Offsets only reset via RMB
  // snap-back (exit_to_idle_snapping_back).
  // Cursor-recenter state: fresh start.
  g_recenter_pending = false;
  g_state = lmb_state::PANNING;
}

// Pan-end without snap-back: cursor restored, offsets kept. The held pitch
// remains applied to cam[0x30] each frame (the wrapper's PANNING/HELD path
// writes cam[0x30] = g_pitch_base + g_pitch_offset; g_pitch_snapshotted
// stays true, base stays valid).
static void enter_held_keeping_offset() {
  ClipCursor(nullptr);  // release multi-monitor trap from enter_panning()
  SetCursorPos(g_lmb_down_cursor.x, g_lmb_down_cursor.y);
  while (g_hides_applied > 0) {
    ShowCursor(TRUE);
    g_hides_applied--;
  }
  // Keep g_yaw_offset, g_pitch_offset, g_pitch_base, g_pitch_snapshotted.
#if ZEAL_ROF2_R3_LMB_PAN_DIAGNOSE
  diag_logf("[pan-held] LMB released; offsets persist (yaw=%+.3f pitch=%+.3f)\n",
            g_yaw_offset, g_pitch_offset);
#endif
  g_state = lmb_state::HELD;
}

// RMB-triggered snap-back: offsets reset to 0, pitch restored to vanilla.
// Called from both PANNING (cursor was hidden, restore it) and HELD (cursor
// already visible). Idempotent w.r.t. cursor hide state.
static void exit_to_idle_snapping_back() {
#if ZEAL_ROF2_R3_LMB_PAN_DIAGNOSE
  const char *from = (g_state == lmb_state::PANNING) ? "PANNING"
                    : (g_state == lmb_state::HELD)    ? "HELD"
                    : (g_state == lmb_state::PENDING) ? "PENDING" : "IDLE";
  diag_logf("[pan-snap-back] RMB triggered; was %s, yaw=%+.3f pitch=%+.3f → 0,0\n",
            from, g_yaw_offset, g_pitch_offset);
#endif
  // Release the multi-monitor cursor trap if it was set (only PANNING set it;
  // HELD already released it). Safe to call even if no clip is active.
  if (g_state == lmb_state::PANNING) {
    ClipCursor(nullptr);
  }
#if ZEAL_ROF2_R3_LMB_PAN_DIAGNOSE
  // Arm post-snap diagnostic so the next ~90 wrapper frames capture button
  // state + camera position. Useful for diagnosing the "RMB-during-pan does
  // not engage EQ's both-buttons-held autorun" bug.
  //
  // CRITICAL: no diag_flush() here. This function runs on the renderer/
  // message-pump thread inside the camera wrapper. A synchronous fflush
  // here stalls Windows message processing for the duration of the disk
  // write — EQ's WM_RBUTTONDOWN handler can't run until the flush returns,
  // and a queue of WM_MOUSEMOVE events piles up. When the renderer
  // catches up, EQ's RMB-look processes the entire batch in one frame and
  // the camera spins wildly. Let the OS buffer the writes; the log is
  // flushed at process exit or when stdio's internal buffer fills.
  g_post_snap_log_remaining = k_post_snap_log_frames;
#endif
  if (g_hides_applied > 0) {
    SetCursorPos(g_lmb_down_cursor.x, g_lmb_down_cursor.y);
    while (g_hides_applied > 0) {
      ShowCursor(TRUE);
      g_hides_applied--;
    }
  }
  g_yaw_offset = 0.0f;
  g_pitch_offset = 0.0f;
  // Schedule the pitch restore to cam[0x30] = g_pitch_base. Wrapper's next
  // call will execute it once and clear g_pitch_snapshotted.
  g_pitch_restore_pending = true;
  g_state = lmb_state::IDLE;
}

static void poll_input() {
  // Focus gate: if EQ is in the background, do nothing — GetAsyncKeyState
  // reads global key state, so clicks in other apps would otherwise enter
  // our state machine. Also release any lingering ClipCursor so the user's
  // other apps work normally.
  if (!eq_has_foreground_focus()) {
    // Always release ClipCursor on background — cheap idempotent call,
    // ensures we never leave the clip lingering across alt-tab.
    ClipCursor(nullptr);
    if (g_state == lmb_state::PANNING) {
      // Restore cursor visibility (it was hidden in enter_panning). DON'T
      // SetCursorPos — the user is in another window now, snapping their
      // cursor would be more disruptive than leaving it where they put it.
      while (g_hides_applied > 0) {
        ShowCursor(TRUE);
        g_hides_applied--;
      }
      // Demote to HELD so the camera angle persists. Re-engaging the pan
      // when the user comes back requires a fresh LMB release+click (the
      // HELD → PENDING transition fires only on a new LMB-down edge).
      g_recenter_pending = false;
      g_state = lmb_state::HELD;
    } else if (g_state == lmb_state::PENDING) {
      // No pan committed yet — drop back to IDLE so we don't auto-engage
      // when focus returns.
      g_state = lmb_state::IDLE;
    }
    return;
  }

  const bool lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
  const bool rmb = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

  switch (g_state) {
    case lmb_state::IDLE:
      if (lmb && !rmb && !is_mouse_over_ui_window()) {
        GetCursorPos(&g_lmb_down_cursor);
        g_state = lmb_state::PENDING;
      }
      break;
    case lmb_state::PENDING:
      if (rmb) {
        // LMB+RMB. If we have held offsets (re-engage path from HELD), the
        // RMB is a snap-back intent. Else just drop to IDLE (vanilla LMB+RMB
        // autorun, no pan was in progress yet).
        if (g_yaw_offset != 0.0f || g_pitch_offset != 0.0f) {
          exit_to_idle_snapping_back();
        } else {
          g_state = lmb_state::IDLE;
        }
      } else if (!lmb) {
        // LMB released before threshold = a click. Return to the resting
        // state that matches our offset: HELD if offsets persist from a
        // prior pan, IDLE otherwise.
        if (g_yaw_offset == 0.0f && g_pitch_offset == 0.0f) {
          g_state = lmb_state::IDLE;
        } else {
          g_state = lmb_state::HELD;
        }
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
      if (rmb) {
        exit_to_idle_snapping_back();
      } else if (!lmb) {
        enter_held_keeping_offset();
      } else {
        POINT current;
        GetCursorPos(&current);

        // Race-aware skip: if we just SetCursorPos in a prior call but the
        // OS hasn't propagated the new position to GetCursorPos yet, the
        // cursor will still report its pre-set position. Skip delta-
        // processing this frame to avoid a phantom large-delta kick.
        bool race_skip = false;
        if (g_recenter_pending) {
          if (current.x == g_pre_set_cursor.x && current.y == g_pre_set_cursor.y) {
            race_skip = true;  // race in progress
          } else {
            g_recenter_pending = false;  // OS has caught up
          }
        }

        if (!race_skip) {
          int dx = current.x - g_prev_cursor.x;
          int dy = current.y - g_prev_cursor.y;
          if (dx > k_max_dx_per_frame) dx = k_max_dx_per_frame;
          else if (dx < -k_max_dx_per_frame) dx = -k_max_dx_per_frame;
          if (dy > k_max_dy_per_frame) dy = k_max_dy_per_frame;
          else if (dy < -k_max_dy_per_frame) dy = -k_max_dy_per_frame;
          // Slider coupling: scale by the same bucket-mult procMouse uses for
          // RMB-drive. LMB-pan and RMB-drive now track together when the user
          // adjusts the in-game MouseSensitivity slider.
          const float sens_mult = read_sens_mult();
          // YAW. WoW spec: mouse RIGHT rotates camera COUNTER-CLOCKWISE (camera
          // to the player's LEFT). Subtraction, not addition.
          g_yaw_offset -= static_cast<float>(dx) * k_mouse_to_yaw_units * sens_mult;
          if (g_yaw_offset > 256.0f) g_yaw_offset -= 512.0f;
          else if (g_yaw_offset < -256.0f) g_yaw_offset += 512.0f;
          // PITCH. WoW spec: mouse DOWN tilts camera UP and OVER the player's
          // head (bird's-eye, looking down). Z-math from FUN_00799140:
          //   z_target = fStack_c - sin(cam[0x30]) * cam[0x34]
          // So decreasing cam[0x30] lifts the camera (less subtraction from
          // the anchor Z). Pull-down (positive dy) ⇒ camera should go up ⇒
          // g_pitch_offset DECREASES. Hence subtraction. (Symmetric with the
          // yaw direction fix above: mouse-right → camera-CCW also uses -=.)
          g_pitch_offset -= static_cast<float>(dy) * k_mouse_to_pitch_units * sens_mult;
          if (g_pitch_offset > k_pitch_offset_max) g_pitch_offset = k_pitch_offset_max;
          else if (g_pitch_offset < k_pitch_offset_min) g_pitch_offset = k_pitch_offset_min;
          g_prev_cursor = current;

          // Edge guard: if cursor is approaching the game-window edge,
          // recenter so OS-level cursor clamping doesn't stall future
          // deltas in that direction. Cursor is hidden during PANNING, so
          // the warp is invisible to the user.
          RECT wr;
          HWND fg = GetForegroundWindow();
          if (fg && GetWindowRect(fg, &wr)) {
            const int margin = 100;  // px from edge before recentering
            if (current.x < wr.left + margin ||
                current.x > wr.right - margin ||
                current.y < wr.top + margin ||
                current.y > wr.bottom - margin) {
              const POINT center = {
                  (wr.left + wr.right) / 2,
                  (wr.top + wr.bottom) / 2};
              g_pre_set_cursor = current;
              SetCursorPos(center.x, center.y);
              g_prev_cursor = center;
              g_recenter_pending = true;
            }
          }
        }
      }
      break;
    case lmb_state::HELD:
      if (rmb) {
        // RMB while a pan is held: instant snap-back per spec.
        exit_to_idle_snapping_back();
      } else if (lmb && !is_mouse_over_ui_window()) {
        // LMB pressed from HELD: re-engage. Offsets persist (we DON'T reset
        // them here or in enter_panning); further drag stacks on the held
        // angle.
        GetCursorPos(&g_lmb_down_cursor);
        g_state = lmb_state::PENDING;
      }
      // else: stay in HELD (cursor visible, offsets persistent, pitch
      // continues being applied each frame via the wrapper's PANNING/HELD
      // pitch block).
      break;
  }
}

static void __fastcall mode_6_wrapper(void *cam, int /*edx_unused*/, int *entity) {
  poll_input();

  char *const cam_bytes = reinterpret_cast<char *>(cam);

#if ZEAL_ROF2_R3_LMB_PAN_DIAGNOSE
  // Post-snap-back diagnostic. Logs button state + camera position for the
  // first k_post_snap_log_frames frames after each RMB-triggered snap-back.
  // If EQ's "both buttons = autorun" engaged, cam position will drift forward
  // across these frames (because the player is actually moving). If it
  // didn't engage, position will be steady. We also log lmb/rmb raw bits so
  // we can confirm Windows still reports both as physically held.
  if (g_post_snap_log_remaining > 0) {
    const int frame_idx = k_post_snap_log_frames - g_post_snap_log_remaining;
    const bool lmb_now = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    const bool rmb_now = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
    const float px = *reinterpret_cast<float *>(cam_bytes + k_cam_pos_x_offset);
    const float py = *reinterpret_cast<float *>(cam_bytes + k_cam_pos_y_offset);
    const float pz = *reinterpret_cast<float *>(cam_bytes + k_cam_pos_z_offset);
    const char *state_name = (g_state == lmb_state::IDLE)    ? "IDLE"
                           : (g_state == lmb_state::PENDING) ? "PENDING"
                           : (g_state == lmb_state::PANNING) ? "PANNING"
                                                              : "HELD";
    diag_logf("[post-snap %3d] state=%s lmb=%d rmb=%d  "
              "cam=(%.3f, %.3f, %.3f)\n",
              frame_idx, state_name, lmb_now ? 1 : 0, rmb_now ? 1 : 0,
              px, py, pz);
    g_post_snap_log_remaining--;
    // Intentionally no diag_flush here — same renderer-thread-stall reason
    // as in exit_to_idle_snapping_back. Rely on stdio buffer flush at exit.
  }
#endif

  // PITCH state machine (Session 15, "Approach A", revised r2 for persistence):
  //   On first PANNING frame:   snapshot cam[0x30] → g_pitch_base.
  //   While PANNING or HELD:    write cam[0x30] = g_pitch_base + g_pitch_offset.
  //                             (HELD keeps the held pitch applied every frame
  //                             so the camera doesn't drift back to vanilla.)
  //   On the wrapper call after RMB snap-back (state→IDLE, restore_pending=true):
  //                             write cam[0x30] = g_pitch_base once, clear flags.
  //
  // cam[0x30] is READ by FUN_00799140 (trig feeds Z component:
  //   z_target = fStack_c - sin/cos(cam[0x30]) * cam[0x34])
  // but NEVER WRITTEN by it. Hence pure pre-set works; no snap/delta gate to
  // mutate around (unlike yaw's DAT_00ddf703). The block is idempotent across
  // the renderer's multi-call-per-frame pattern (shadows/reflections call
  // slot 0x08 several times) since each call writes the same value.
  if (g_state == lmb_state::PANNING || g_state == lmb_state::HELD) {
    if (!g_pitch_snapshotted) {
      g_pitch_base = *reinterpret_cast<float *>(cam_bytes + k_cam_pitch_offset);
      g_pitch_snapshotted = true;
#if ZEAL_ROF2_R3_LMB_PAN_DIAGNOSE
      {
        const int bucket_raw = *reinterpret_cast<int *>(
            k_dat_mouse_sens_bucket_ghidra + g_aslr_delta);
        const float sens_mult = read_sens_mult();
        diag_logf("[pan-enter] cam[0x30]_base=%f  cam[0x38]=%f  "
                  "cam[1..3]=(%.3f, %.3f, %.3f)  "
                  "MouseSens bucket=%d (clamped 1..8), sens_mult=%.3f  "
                  "effective_yaw_per_px=%.4f  effective_pitch_per_px=%.4f\n",
                  g_pitch_base,
                  *reinterpret_cast<float *>(cam_bytes + k_cam_heading_offset),
                  *reinterpret_cast<float *>(cam_bytes + k_cam_pos_x_offset),
                  *reinterpret_cast<float *>(cam_bytes + k_cam_pos_y_offset),
                  *reinterpret_cast<float *>(cam_bytes + k_cam_pos_z_offset),
                  bucket_raw, sens_mult,
                  k_mouse_to_yaw_units * sens_mult,
                  k_mouse_to_pitch_units * sens_mult);
      }
#endif
    }
    *reinterpret_cast<float *>(cam_bytes + k_cam_pitch_offset) =
        g_pitch_base + g_pitch_offset;
  } else if (g_pitch_restore_pending) {
    *reinterpret_cast<float *>(cam_bytes + k_cam_pitch_offset) = g_pitch_base;
#if ZEAL_ROF2_R3_LMB_PAN_DIAGNOSE
    diag_logf("[pan-restore] cam[0x30] = %f (base)\n", g_pitch_base);
#endif
    g_pitch_restore_pending = false;
    g_pitch_snapshotted = false;
  }

  // Pre-original capture for the per-frame pitch diagnostic. Captures the
  // first k_max_pan_frames_to_log PANNING frames of every pan; counter resets
  // when the pan ends so each pan starts fresh.
#if ZEAL_ROF2_R3_LMB_PAN_DIAGNOSE_PER_FRAME
  static int s_pan_frame_log_count = 0;
  static const int k_max_pan_frames_to_log = 200;
  float diag_pre_x = 0.0f, diag_pre_y = 0.0f, diag_pre_z = 0.0f;
  const bool diag_log_this_frame = (g_state == lmb_state::PANNING) &&
                                    (s_pan_frame_log_count < k_max_pan_frames_to_log);
  if (diag_log_this_frame) {
    diag_pre_x = *reinterpret_cast<float *>(cam_bytes + k_cam_pos_x_offset);
    diag_pre_y = *reinterpret_cast<float *>(cam_bytes + k_cam_pos_y_offset);
    diag_pre_z = *reinterpret_cast<float *>(cam_bytes + k_cam_pos_z_offset);
  }
#endif

  // YAW: Approach F (Session 14). cam[0x38] gets overwritten by the original
  // every frame, so we pre-set it AND temporarily force the delta-math path
  // (DAT_00ddf703 = 0). Restore DAT_00ddf703 after so other code sees its
  // normal value. Only mutate when offset is non-zero — keeps the vanilla
  // pass-through path byte-identical when LMB-pan is yaw-idle.
  unsigned char *const dat_703_ptr = reinterpret_cast<unsigned char *>(
      k_dat_00ddf703_ghidra + g_aslr_delta);
  unsigned char saved_dat_703 = 0;
  const bool yaw_apply = (g_yaw_offset != 0.0f);
  if (yaw_apply) {
    const float entity_heading =
        *reinterpret_cast<float *>(reinterpret_cast<char *>(entity) +
                                   k_entity_heading_byte_offset);
    saved_dat_703 = *dat_703_ptr;
    *dat_703_ptr = 0;
    *reinterpret_cast<float *>(cam_bytes + k_cam_prev_entity_heading_offset) =
        entity_heading;
    *reinterpret_cast<float *>(cam_bytes + k_cam_heading_offset) =
        entity_heading + g_yaw_offset;
  }

  g_original(cam, 0, entity);

  if (yaw_apply) {
    *dat_703_ptr = saved_dat_703;
  }

#if ZEAL_ROF2_R3_LMB_PAN_DIAGNOSE_PER_FRAME
  if (diag_log_this_frame) {
    const float post_x = *reinterpret_cast<float *>(cam_bytes + k_cam_pos_x_offset);
    const float post_y = *reinterpret_cast<float *>(cam_bytes + k_cam_pos_y_offset);
    const float post_z = *reinterpret_cast<float *>(cam_bytes + k_cam_pos_z_offset);
    const float cam30_written = *reinterpret_cast<float *>(cam_bytes + k_cam_pitch_offset);
    const float cam38 = *reinterpret_cast<float *>(cam_bytes + k_cam_heading_offset);
    diag_logf("[pan-frame %3d] yaw_off=%+8.3f pitch_off=%+8.3f  "
              "cam[0x30]=%+.4f cam[0x38]=%+.4f  "
              "pre=(%.2f, %.2f, %.2f) post=(%.2f, %.2f, %.2f)  "
              "dX=%+.4f dY=%+.4f dZ=%+.4f\n",
              s_pan_frame_log_count,
              g_yaw_offset, g_pitch_offset,
              cam30_written, cam38,
              diag_pre_x, diag_pre_y, diag_pre_z,
              post_x, post_y, post_z,
              post_x - diag_pre_x, post_y - diag_pre_y, post_z - diag_pre_z);
    s_pan_frame_log_count++;
  }
  // Reset the per-pan frame counter once we've fully returned to IDLE
  // (state IDLE AND no restore pending). Logs a summary line for grep-ability.
  if (g_state != lmb_state::PANNING && !g_pitch_restore_pending &&
      s_pan_frame_log_count > 0) {
    diag_logf("[pan-end] logged %d frames in this pan\n", s_pan_frame_log_count);
    s_pan_frame_log_count = 0;
  }
#endif
}

bool install(uintptr_t aslr_delta) {
  g_aslr_delta = aslr_delta;

  const uintptr_t slot_runtime = k_mode_6_vtable_slot_08_ghidra + aslr_delta;
  const uintptr_t expected_runtime = k_mode_6_slot_08_original_ghidra + aslr_delta;
  uintptr_t *const slot_ptr = reinterpret_cast<uintptr_t *>(slot_runtime);
  const uintptr_t actual = *slot_ptr;
  const bool match = (actual == expected_runtime);

#if ZEAL_ROF2_R3_LMB_PAN_DIAGNOSE
  diag_logf("[install] aslr_delta=0x%08x  slot_runtime=0x%08x  "
            "expected=0x%08x  actual=0x%08x  match=%s  decision=%s\n",
            (unsigned)aslr_delta, (unsigned)slot_runtime,
            (unsigned)expected_runtime, (unsigned)actual,
            match ? "YES" : "NO", match ? "INSTALL" : "SKIP");
  diag_logf("[config] k_mouse_to_yaw_units=%f  k_mouse_to_pitch_units=%f  "
            "k_pitch_clamp=[%f, %f]  k_pan_pixel_threshold=%d  "
            "slider_coupling=ON (reads DAT_00ddf69c, applies "
            "(bucket-1)/7*1.5+0.5 mult per procMouse)\n",
            k_mouse_to_yaw_units, k_mouse_to_pitch_units,
            k_pitch_offset_min, k_pitch_offset_max, k_pan_pixel_threshold);
  // Keep the install MessageBox — runs pre-game, cursor is visible, safe.
  char msg[1024];
  wsprintfA(msg,
            "Mode 6 (vanilla 3rd-person) vtable slot 0x08 patch:\r\n\r\n"
            "Vtable slot runtime: 0x%08x\r\n"
            "Expected original (FUN_00799140 + delta): 0x%08x\r\n"
            "Actual value at slot: 0x%08x\r\n\r\n"
            "Signature match: %s\r\n"
            "Decision: %s\r\n\r\n"
            "Diagnostics now go to D:\\EQEmu\\Full_RoF2\\lmb_pan_diag.log\r\n"
            "(no more mid-pan popups blocking your hidden cursor).",
            (unsigned)slot_runtime, (unsigned)expected_runtime, (unsigned)actual,
            match ? "YES" : "NO", match ? "INSTALL" : "SKIP (silent no-op)");
  MessageBoxA(NULL, msg, "Zeal-RoF2 R3 / LMB-pan install diagnostic",
              MB_OK | MB_ICONINFORMATION);
  diag_flush();
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

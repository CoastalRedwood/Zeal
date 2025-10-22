#include "camera_mods.h"

#include <algorithm>
#include <thread>

#include "binds.h"
#include "callbacks.h"
#include "camera_math.h"
#include "commands.h"
#include "game_addresses.h"
#include "game_functions.h"
#include "game_packets.h"
#include "game_structures.h"
#include "hook_wrapper.h"
#include "string_util.h"
#include "zeal.h"

// #define debug_cam

// ExecuteCmd keeps an array (at least through [0xcd]) of key states.
static int *const kKeyDownStates = reinterpret_cast<int *>(0x007ce04c);
const int CMD_RIGHT = 5;
const int CMD_LEFT = 6;
const int CMD_PITCH_UP = 15;
const int CMD_PITCH_DOWN = 16;
const int CMD_CENTER_VIEW = 17;
const int CMD_ZOOM_IN = 18;
const int CMD_ZOOM_OUT = 19;
const int CMD_TOGGLE_CAM = 20;

// Workaround for mouse hovering state check.
static bool is_explore_mode() {
  if (Zeal::Game::is_in_char_select()) {
    const auto char_select = Zeal::Game::Windows ? Zeal::Game::Windows->CharacterSelect : nullptr;
    return (char_select && char_select->Explore);
  }
  return false;
}

static int get_camera_view() { return *Zeal::Game::camera_view; }

static void set_camera_view(int view) { *Zeal::Game::camera_view = view; }

// Pitch control depends on whether character is mounted. Logic copied from procMouse.
static Zeal::GameStructures::Entity *get_pitch_control_entity() {
  auto self = Zeal::Game::get_self();
  if (self && self->ActorInfo && self->ActorInfo->Mount && self->ActorInfo->Mount == Zeal::Game::get_controlled())
    return self;
  return Zeal::Game::get_controlled();
}

// Returns true if the Zeal replacement camera is active and should update the final display.
bool CameraMods::is_zeal_cam_active() const {
  return (ui_active && enabled.get() && get_camera_view() == Zeal::GameEnums::CameraView::ZealCam);
}

// Calculates the head and wanted camera positions. Returns true if no collisions.
bool CameraMods::calc_camera_positions(Vec3 &head_pos, Vec3 &wanted_pos) const {
  head_pos = Zeal::Game::get_view_actor_head_pos();
  wanted_pos = camera_math::get_cam_pos_behind(head_pos, zeal_cam_zoom, zeal_cam_yaw, -zeal_cam_pitch);

#ifdef debug_cam
  auto self = Zeal::Game::get_view_actor_entity();
  ZealService::get_instance()->labels_hook->print_debug_info("View actor: 0x%08x\nHead: %s\nWanted: %s", (uint32_t)self,
                                                             head_pos.toString().c_str(),
                                                             wanted_pos.toString().c_str());
#endif

  return !Zeal::Game::collide_with_world(head_pos, wanted_pos, wanted_pos);
}

// Calculates the camera positions and overrides the display's CameraInfo structure used in rendering.
void CameraMods::handle_do_cam_ai() {
  if (!is_zeal_cam_active()) return;

  Vec3 head_pos;
  Vec3 wanted_pos;
  calc_camera_positions(head_pos, wanted_pos);
  auto *cam = Zeal::Game::get_camera();
  cam->Position = wanted_pos;
  cam->Heading = zeal_cam_yaw;
  cam->Pitch = camera_math::get_pitch(cam->Position, head_pos);
  cam->RegionNumber = Zeal::Game::get_region_from_pos(&cam->Position);
}

// Intercepts the mouse wheel to handle zooming and camera transitions when active.
bool CameraMods::handle_mouse_wheel(int delta) {
  if (!ui_active || !enabled.get()) return false;

  DWORD camera_view = get_camera_view();
  if (camera_view != Zeal::GameEnums::CameraView::FirstPerson && camera_view != Zeal::GameEnums::CameraView::ZealCam)
    return false;

  if (Zeal::Game::is_mouse_hovering_window() && !is_explore_mode()) return false;

  // Handles proper activation of zeal_cam when zoomed out of first and updates zeal cam zoom.
  if (delta < 0 && get_camera_view() == Zeal::GameEnums::CameraView::FirstPerson)
    set_zeal_cam_active(true);                                                  // Flips mode to ZealCam if enabled.
  else if (delta && get_camera_view() == Zeal::GameEnums::CameraView::ZealCam)  // Ignore in other modes.
    update_desired_zoom(delta > 0 ? -zoom_speed : zoom_speed);

  return true;
}

// Handles the transition into Zeal camera mode (sets state).
void CameraMods::set_zeal_cam_active(bool activate) {
  if (activate && enabled.get()) {
    set_camera_view(Zeal::GameEnums::CameraView::ZealCam);
    if (Zeal::Game::get_controlled()) {
      zeal_cam_yaw = Zeal::Game::get_controlled()->Heading;
      zeal_cam_pitch = camera_math::pitch_to_normal(get_pitch_control_entity()->Pitch);
    }
    desired_zoom = std::clamp(desired_zoom, min_zoom_in, max_zoom_out);
    zeal_cam_zoom = desired_zoom;
  }
}

// Updates the Zeal cam's desired final zoom distance that is used as the interpolation target.
// Also handles the transition to first person when zoomed in.
void CameraMods::update_desired_zoom(float zoom) {
  desired_zoom += zoom;
  if (desired_zoom > max_zoom_out)
    desired_zoom = max_zoom_out;
  else if (desired_zoom < min_zoom_in && zoom < 0)
    desired_zoom = 0;

  if (desired_zoom == 0 && is_zeal_cam_active()) {
    Zeal::Game::get_self()->Pitch = camera_math::pitch_to_game(zeal_cam_pitch);
    set_camera_view(Zeal::GameEnums::CameraView::FirstPerson);
  }

  // Perform a collision detect and reduce zoom distance if necessary.
  if (zoom > 0 && is_zeal_cam_active()) {
    Vec3 head_pos;
    Vec3 wanted_pos;
    if (!calc_camera_positions(head_pos, wanted_pos)) desired_zoom -= zoom;
  }
}

// Fox delta x, the native procMouse either sets the controlled MovementSpeedHeading (low cameras) or disables strafing
// and sets the camera specific yaw for the others.
// For delta y, it skips pitch processing on a horse. Otherwise it sets some camera specific pitch or directly sets
// the entity pitch.
bool CameraMods::handle_proc_mouse() {
  Zeal::GameStructures::Entity *self = Zeal::Game::get_controlled();
  if (!self || !ui_active || !enabled.get()) return false;

  DWORD camera_view = get_camera_view();
  if (camera_view != Zeal::GameEnums::CameraView::ZealCam && camera_view != Zeal::GameEnums::CameraView::FirstPerson)
    return false;

  static float smoothMouseDeltaX = 0;
  static float smoothMouseDeltaY = 0;
  Zeal::GameStructures::CameraInfo *cam = Zeal::Game::get_camera();
  Zeal::GameStructures::MouseDelta *delta = (Zeal::GameStructures::MouseDelta *)0x798586;
  bool lbutton = *Zeal::Game::is_left_mouse_down;
  bool rbutton = *Zeal::Game::is_right_mouse_look_down;

  const bool is_zeal_cam = (camera_view == Zeal::GameEnums::CameraView::ZealCam);
  if (!rbutton && !(lbutton && is_zeal_cam)) return true;  // Nothing to do; skip hooked procMouse.

  float delta_y = delta->y;
  float delta_x = delta->x;
  float t = 1.0f / 1.5f;
  smoothMouseDeltaX = camera_math::lerp(delta_x * sensitivity_x, smoothMouseDeltaX, t);
  smoothMouseDeltaY = camera_math::lerp(delta_y * sensitivity_y, smoothMouseDeltaY, t);
  if (fabs(smoothMouseDeltaX) > 5) smoothMouseDeltaY /= 2;

  delta->y = 0;  // May not be necessary but just in case reset to avoid downstream usage.
  delta->x = 0;

  if (*(BYTE *)0x7985E8)  // invert
    smoothMouseDeltaY = -smoothMouseDeltaY;

  // Right button has highest priority to control direction (when not stunned).
  if (rbutton && Zeal::Game::can_move()) {
    if (fabs(smoothMouseDeltaX) > 0.1)
      self->MovementSpeedHeading = -smoothMouseDeltaX / 1000;
    else
      self->MovementSpeedHeading = 0;

    if (is_zeal_cam) {
      zeal_cam_yaw -= smoothMouseDeltaX;
      zeal_cam_yaw = fmodf(zeal_cam_yaw + 512.f, 512.f);  // Wrap within 0 to 512.
      self->Heading = zeal_cam_yaw;                       // Should only be set between 0 to 512.
    } else
      self->Heading += -smoothMouseDeltaX;
  } else {
    zeal_cam_yaw -= smoothMouseDeltaX;  // Pan camera independent of heading.
  }

  if (fabs(smoothMouseDeltaY) > 0) {
    auto self_pitch = get_pitch_control_entity();  // Control entity varies based on mounted state.
    if (is_zeal_cam) {
      zeal_cam_pitch -= smoothMouseDeltaY;
      zeal_cam_pitch = std::clamp(zeal_cam_pitch, -89.9f, 89.9f);
      if (Zeal::Game::KeyMods->Shift) self_pitch->Pitch = camera_math::pitch_to_game(zeal_cam_pitch);
    } else if (self_pitch == Zeal::Game::get_view_actor_entity()) {
      self_pitch->Pitch -= smoothMouseDeltaY;
      self_pitch->Pitch = std::clamp(self_pitch->Pitch, -128.f, 128.f);
    }
  }

  return true;  // Skip hooked procMouse.
}

// This callback is invoked when the enabled setting is changed. It synchronizes the state.
void CameraMods::synchronize_set_enable() {
  if (Zeal::Game::get_gamestate() != GAMESTATE_INGAME) return;
  set_zeal_cam_active(get_camera_view() == Zeal::GameEnums::CameraView::ZealCam);
}

// Interpolate zoom is called repeatedly in main_callback(). The alpha coefficient below acts as a
// first order IIR low pass filter for smoothing out the transitions.
void CameraMods::interpolate_zoom() { zeal_cam_zoom = zeal_cam_zoom + (desired_zoom - zeal_cam_zoom) * 0.3f; }

// Returns true if the mouse is over a visible client window.
static bool is_over_client_rect(void) {
  auto hwnd = Zeal::Game::get_game_window();
  if (!::IsWindowVisible(hwnd)) return false;

  POINT cursor;
  ::GetCursorPos(&cursor);  // This returns absolute screen x, y.

  POINT offset = {0, 0};
  ::ClientToScreen(hwnd, &offset);
  cursor.x -= offset.x;
  cursor.y -= offset.y;

  RECT rect;
  ::GetClientRect(hwnd, &rect);

  return ::PtInRect(&rect, cursor);
}

// Setting enable allows the game to control the display or hiding of the game cursor.
static void set_internal_cursor_enable(bool enable) {
  const int draw_cursor_jump_addr = 0x0053edef;
  BYTE opcode = enable ? 0x75 : 0xEB;  // Toggle conditional vs unconditional jump past draw cursor.
  if (*(BYTE *)draw_cursor_jump_addr != opcode) mem::write<BYTE>(0x53edef, opcode);
}

// Sets all internal absolute mouse position state to (x,y).
static void set_game_mouse_position(int x, int y) {
  *Zeal::Game::mouse_client_x_dinput_accum = x;
  *Zeal::Game::mouse_client_y_dinput_accum = y;
  *Zeal::Game::mouse_client_x_dinput_state = x;
  *Zeal::Game::mouse_client_y_dinput_state = y;
  *Zeal::Game::mouse_client_x = x;
  *Zeal::Game::mouse_client_y = y;
}

// Synchronizes the win32 cursor to the internal cursor position.
void set_win32_cursor_to_client_position(POINT pt) {
  ::ClientToScreen(Zeal::Game::get_game_window(), &pt);
  ::SetCursorPos(pt.x, pt.y);
}

// Sets the internal cursor location and synchronizes the win32 cursor with it.
void set_game_mouse_and_win32_position(POINT pt) {
  set_game_mouse_position(pt.x, pt.y);
  set_win32_cursor_to_client_position(pt);
}

// Periodic call to handle left button panning in ZealCam. Note that unlike rmb mouse look,
// the mouse button down and up are not hooked. Instead we have a pan delay and rely on the
// same processing loop as camera interpolation to update the view. Note that we don't
// recenter the mouse and win32 to the center during left pan as this could confuse the
// previous mouse location state, so we could get some glitching with fast cursor moves at
// the window edges, but this isn't as critical for lmb as rmb.
void CameraMods::update_left_pan(DWORD camera_view) {
  if (!*Zeal::Game::is_right_mouse_look_down && *Zeal::Game::is_left_mouse_down && is_zeal_cam_active() &&
      (lmouse_time || is_over_client_rect()) && !Zeal::Game::is_game_ui_window_hovered()) {
    if (!lmouse_time) {
      lmouse_time = GetTickCount64();
      lmouse_cursor_pos = POINT{32767, 32767};
      hide_cursor = true;
    }

    if (GetTickCount64() - lmouse_time > pan_delay.get()) {
      if (hide_cursor) {
        hide_cursor = false;
        lmouse_cursor_pos = POINT{*Zeal::Game::mouse_client_x_dinput_accum, *Zeal::Game::mouse_client_y_dinput_accum};
        set_internal_cursor_enable(false);  // Hide the internal cursor (GAMESTATE_INGAME only).
      }

      if (lmouse_cursor_pos.x != 32767) {
        handle_proc_mouse();
        set_game_mouse_and_win32_position(lmouse_cursor_pos);
      }
    }
  } else if (lmouse_time) {
    lmouse_time = 0;
    set_internal_cursor_enable(true);  // Restore showcursor check.
    hide_cursor = false;
  }
}

// Turns Zeal Cam into a chase cam in autofollow mode (unless left panning).
void CameraMods::update_autofollow() {
  if (!is_zeal_cam_active()) return;

  if (lmouse_time) return;  // Skip chase mode when left panning.

  // Must be in follow mode with control relevant to self heading.
  auto self = Zeal::Game::get_self();
  if (!self) return;
  auto leader = self->ActorInfo ? self->ActorInfo->Following : nullptr;
  if (!leader) return;  // Not in auto-follow.
  auto controlled = Zeal::Game::get_controlled();
  if (!controlled || (controlled != self && controlled != self->ActorInfo->Mount)) return;

  zeal_cam_yaw = self->Heading;
}

// Called repeatedly by main_callback() to handle periodic calls and process held down keys.
void CameraMods::process_time_tick() {
  if (reset_camera) callback_zone();
  reset_camera = false;

  DWORD camera_view = get_camera_view();
  bool zoom_out = kKeyDownStates[CMD_ZOOM_OUT];
  if (zoom_out && camera_view == Zeal::GameEnums::CameraView::FirstPerson) {
    set_zeal_cam_active(true);  // Activates if enabled.
    zoom_out = false;           // Reset zoom key press so not double-counted below.
  }

  update_left_pan(camera_view);  // Call to keep cursor visibility updated.

  if (!is_zeal_cam_active()) return;

  update_autofollow();

  if (kKeyDownStates[CMD_ZOOM_IN])
    update_desired_zoom(-0.3f);
  else if (zoom_out)
    update_desired_zoom((min_zoom_in > desired_zoom) ? (min_zoom_in - desired_zoom) : 0.3f);
  interpolate_zoom();

  // Cam lock mode yaws camera to match heading when enabled.
  // Since we see the key press state *before* the key press causes a change in self->Heading, we
  // pipeline the updates to happen in the next next tick.
  Zeal::GameStructures::Entity *self = Zeal::Game::get_controlled();
  if (chase_mode_active) zeal_cam_yaw = self->Heading;

  chase_mode_active = (kKeyDownStates[CMD_RIGHT] || kKeyDownStates[CMD_LEFT]) && cam_lock.get();

  // In Zeal camera mode, we allow some keyboard control of pitch.
  if (kKeyDownStates[CMD_CENTER_VIEW]) zeal_cam_pitch = 0.f;  // Center to match client behavior.

  // The self pitch is controlled to enhance control during levitation or swimming.
  // - The client doesn't center the self pitch when center_view is pressed.
  // - The current integrated self pitch isn't visible in 3rd person, so just cancel out
  //   any integrated large pitch if an opposite direction key is pressed.
  auto pitch_self = get_pitch_control_entity();
  if (pitch_self && (kKeyDownStates[CMD_CENTER_VIEW] || (kKeyDownStates[CMD_PITCH_UP] && pitch_self->Pitch < 0) ||
                     (kKeyDownStates[CMD_PITCH_DOWN] && pitch_self->Pitch > 0)))
    pitch_self->Pitch = 0.f;
}

void CameraMods::update_fps_sensitivity() {
  auto currentTime = std::chrono::steady_clock::now();
  auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - lastTime).count();
  // Restrict fps updates to reasonable range (500 to 10 Hz).
  if ((elapsedTime > 2) && (elapsedTime < 100)) {
    float new_fps = 1000.0f / elapsedTime;
    fps += (new_fps - fps) * 0.5;  // Light low-pass IIR filtering of fps changes.
  }

  if (use_old_sens.get()) {
    sensitivity_x = user_sensitivity_x.get() * (fps / 144.f);
    sensitivity_y = user_sensitivity_y.get() * (fps / 144.f);

    if (get_camera_view() == Zeal::GameEnums::CameraView::ZealCam) {
      sensitivity_x = user_sensitivity_x_3rd.get() * (fps / 144.f);
      sensitivity_y = user_sensitivity_y_3rd.get() * (fps / 144.f);
    }
  } else  // this 'should' eliminate the sensitivity going down significantly when going into low fps regions
  {
    sensitivity_x = user_sensitivity_x.get();
    sensitivity_y = user_sensitivity_y.get();
    if (get_camera_view() == Zeal::GameEnums::CameraView::ZealCam) {
      sensitivity_x = user_sensitivity_x_3rd.get();
      sensitivity_y = user_sensitivity_y_3rd.get();
    }
  }

  float current_sens = (float)(*(BYTE *)0x798b0c);
  float multiplier = current_sens / 4.0f;
  sensitivity_x *= multiplier;
  sensitivity_y *= multiplier;
  lastTime = currentTime;
}

void CameraMods::callback_zone() {
  if (Zeal::Game::get_controlled()) {
    zeal_cam_yaw = Zeal::Game::get_controlled()->Heading;
    zeal_cam_pitch = 0;  // First person pitch is not reset on zoning.
  }
  if (desired_zoom > 0) {
    desired_zoom = std::clamp(desired_zoom, min_zoom_in, max_zoom_out);
    zeal_cam_zoom = desired_zoom;
  }
  chase_mode_active = false;
}

void CameraMods::synchronize_old_ui() {
  if (Zeal::Game::is_new_ui()) return;

  // Old UI doesn't have the init or cleans to synchronize the active state.
  auto display = Zeal::Game::get_display();
  if (display && ui_active != (display->WorldDisplayStarted != 0))
    ui_active = display->WorldDisplayStarted;  // Sync with display global state.
}

// Called periodically to keep the camera synced and compensate for fps rates.
void CameraMods::handle_process_mouse_and_get_key() {
  static int prev_view = get_camera_view();
  synchronize_old_ui();
  if (!ui_active || !enabled.get()) return;
  update_fps_sensitivity();
  process_time_tick();

  // Monitor for a change in camera view to stay in sync (such as F9).
  DWORD camera_view = get_camera_view();
  if (prev_view != camera_view && ui_active)  // this simply checks if your camera view has changed
  {
    bool is_zeal_index = (camera_view == Zeal::GameEnums::CameraView::ZealCam);
    set_zeal_cam_active(is_zeal_index);  // Activates only if enabled.
  }
  prev_view = get_camera_view();
}

void CameraMods::handle_proc_rmousedown(int x, int y) {
  if (!is_zeal_cam_active() || !Zeal::Game::can_move()) return;

  // Support chase control mode: Shift the player to track the camera yaw if more than a 5 degree difference.
  if (fabs(camera_math::angle_difference(zeal_cam_yaw, Zeal::Game::get_controlled()->Heading)) > 5) {
    Zeal::Game::get_controlled()->Heading = zeal_cam_yaw;
  }
}

// Execute update processing right after the latest mouse and keyboard inputs are fetched.
static int ProcessMouseAndGetKey() {
  auto zeal = ZealService::get_instance();
  int result = zeal->hooks->hook_map["ProcessMouseAndGetKey"]->original(ProcessMouseAndGetKey)();
  zeal->camera_mods->handle_process_mouse_and_get_key();
  return result;
}

// Consumes relevant mouse scroll wheel messages otherwise passes it on.
static int HandleMouseWheel(int delta) {
  if (!ZealService::get_instance()->camera_mods->handle_mouse_wheel(delta))
    return ZealService::get_instance()->hooks->hook_map["HandleMouseWheel"]->original(HandleMouseWheel)(delta);
  return 0;
}

// Consumes relevant right held mouse movement messages otherwise passes it on.
static void __fastcall procMouse(int game, int unused, int a1) {
  Zeal::GameStructures::CameraInfo *cam = Zeal::Game::get_camera();
  ZealService *zeal = ZealService::get_instance();
  if (!zeal->camera_mods->handle_proc_mouse())
    zeal->hooks->hook_map["procMouse"]->original(procMouse)(game, unused, a1);
}

// Monitors for mouse right button down messages.
static void __fastcall RMouseDown(void *game_this, int unused_edx, int x, int y) {
  ZealService *zeal = ZealService::get_instance();
  zeal->camera_mods->handle_proc_rmousedown(x, y);
  zeal->hooks->hook_map["RMouseDown"]->original(RMouseDown)(game_this, unused_edx, x, y);
}

void CameraMods::synchronize_fov() {
  if (!enabled.get() || Zeal::Game::get_gamestate() != GAMESTATE_INGAME) return;

  // TODO: This is not setting the camera lens, so is it accurate?
  Zeal::GameStructures::CameraInfo *ci = Zeal::Game::get_camera();
  if (ci) ci->FieldOfView = fov.get();
}

// Overrides the default FOV when enabled.
static int SetCameraLens(int a1, float fov, float aspect_ratio, float a4, float a5) {
  ZealService *zeal = ZealService::get_instance();
  bool enabled = zeal->camera_mods->enabled.get();
  if (enabled) fov = zeal->camera_mods->fov.get();
  int rval = zeal->hooks->hook_map["SetCameraLens"]->original(SetCameraLens)(a1, fov, aspect_ratio, a4, a5);
  if (enabled && Zeal::Game::get_gamestate() != GAMESTATE_PRECHARSELECT) {
    Zeal::GameStructures::CameraInfo *ci = Zeal::Game::get_camera();
    if (ci) ci->FieldOfView = fov;
  }
  return rval;
}

// The DoCamAI process uses the currently active get_camera_view() value as an index into an array of
// per camera values (yaw, pitches) that were updated earlier in calls like procMouse. Those values are then
// transformed and written into the display's CameraInfo structure.
// Note: The camera info array starts at 0x00799688 with [6] entries of 0x1C each.
static void __fastcall DoCamAI(int display, int u, Zeal::GameStructures::Entity *player) {
  ZealService *zeal = ZealService::get_instance();
  zeal->hooks->hook_map["DoCamAI"]->original(DoCamAI)(display, u, player);
  zeal->camera_mods->handle_do_cam_ai();  // Overrides CameraInfo if enabled.
}

// Supports temporarily reducing the bounding radius or player and mount during the GetClickedActor call.
static int __fastcall GetClickedActor(int this_display, int unused_edx, int mouse_x, int mouse_y, int get_on_actor) {
  auto self = Zeal::Game::get_self();
  bool clickthru = !get_on_actor && ZealService::get_instance()->camera_mods->setting_selfclickthru.get();
  float *bounding_radius = (clickthru && self && self->ActorInfo && self->ActorInfo->ViewActor_)
                               ? &self->ActorInfo->ViewActor_->BoundingRadius
                               : nullptr;
  float original_radius = bounding_radius ? *bounding_radius : 0;
  if (bounding_radius) *bounding_radius = 0.001f;  // Small, hard to click value.

  // Just copy the logic for mounts as well.
  auto mount = (clickthru && self && self->ActorInfo) ? self->ActorInfo->Mount : nullptr;
  float *mount_bounding_radius = (clickthru && mount && mount->ActorInfo && mount->ActorInfo->ViewActor_)
                                     ? &mount->ActorInfo->ViewActor_->BoundingRadius
                                     : nullptr;
  float mount_original_radius = mount_bounding_radius ? *mount_bounding_radius : 0;
  if (mount_bounding_radius) *mount_bounding_radius = 0.001f;  // Small, hard to click value.

  ZealService *zeal = ZealService::get_instance();
  int rval = zeal->hooks->hook_map["GetClickedActor"]->original(GetClickedActor)(this_display, unused_edx, mouse_x,
                                                                                 mouse_y, get_on_actor);
  if (bounding_radius) *bounding_radius = original_radius;
  if (mount_bounding_radius) *mount_bounding_radius = mount_original_radius;
  return rval;
}

// Old UI
static int __fastcall EQ3DView_MouseUp(int this_view, int unused_edx, int right_button, int mouse_x, int mouse_y) {
  auto pre_target = Zeal::Game::get_target();
  auto zeal = ZealService::get_instance();
  int result = zeal->hooks->hook_map["EQ3DView_MouseUp"]->original(GetClickedActor)(this_view, unused_edx, right_button,
                                                                                    mouse_x, mouse_y);
  auto target = Zeal::Game::get_target();
  if (!right_button && target && target != pre_target && target != Zeal::Game::get_self() &&
      target->Type == Zeal::GameEnums::EntityTypes::NPC && Zeal::Game::is_in_game() &&
      zeal->camera_mods->setting_leftclickcon.get()) {
    Zeal::Game::do_consider(target, " ");
  }
  return result;
}

// New UI
static void __fastcall LeftClickedOnPlayer(int this_game, int unused_edx, Zeal::GameStructures::Entity *player) {
  auto zeal = ZealService::get_instance();
  zeal->hooks->hook_map["LeftClickedOnPlayer"]->original(LeftClickedOnPlayer)(this_game, unused_edx, player);
  auto target = Zeal::Game::get_target();
  if (target && target != Zeal::Game::get_self() && target->Type == Zeal::GameEnums::EntityTypes::NPC &&
      Zeal::Game::is_in_game() && zeal->camera_mods->setting_leftclickcon.get()) {
    Zeal::Game::do_consider(target, " ");
  }
}

void CameraMods::handle_toggle_cam() {
  // The camera view is going to be incremented in the CDisplay::ToggleView() call. If the next slot has
  // been disabled, we adjust it so it will increment to an enabled slot.
  int next_view = get_camera_view();
  int view_update = -1;
  while (view_update < 0) {
    next_view++;  // Peek ahead at next slot.
    switch (next_view) {
      case 1:
        if (setting_toggle_overhead_view.get()) view_update = 0;
        break;
      case 2:
        if (setting_toggle_zeal_view.get()) view_update = 1;
        break;
      case 3:
        if (setting_toggle_free1_view.get()) view_update = 2;
        break;
      case 4:
        if (setting_toggle_free2_view.get()) view_update = 3;
        break;
      default:
        view_update = 4;  // Will toggle to first-person, always enabled.
        break;
    }
  }

  set_camera_view(view_update);
}

// Reset camera on summons. Post a reset_camera pending flag to first let the client packet get processed
// and update the characters position and heading.
bool CameraMods::callback_packet(UINT opcode, char *buffer, UINT len) {
  if (opcode == Zeal::Packets::RequestClientZoneChange) reset_camera = true;
  return false;
}

CameraMods::CameraMods(ZealService *zeal) {
  mem::write<BYTE>(0x4db8d9, 0xEB);  // Unconditional jump to skip an optional bad camera position debug message.

  lastTime = std::chrono::steady_clock::now();
  zeal->callbacks->AddGeneric([this]() { ui_active = true; }, callback_type::InitUI);
  zeal->callbacks->AddGeneric([this]() { ui_active = false; }, callback_type::CleanUI);
  zeal->callbacks->AddGeneric([this]() { ui_active = true; }, callback_type::InitCharSelectUI);
  zeal->callbacks->AddGeneric([this]() { ui_active = false; }, callback_type::CleanCharSelectUI);
  zeal->callbacks->AddGeneric([this]() { callback_zone(); }, callback_type::EnterZone);

  zeal->callbacks->AddPacket(
      [this](UINT opcode, char *buffer, UINT len) { return callback_packet(opcode, buffer, len); },
      callback_type::WorldMessage);

  zeal->binds_hook->replace_cmd(CMD_CENTER_VIEW, [](int state) {
    if (!state) kKeyDownStates[CMD_CENTER_VIEW] = state;  // Client is not clearing this state.
    return false;
  });
  zeal->binds_hook->replace_cmd(CMD_TOGGLE_CAM, [this](int state) {
    if (state)  // Only key down.
      handle_toggle_cam();
    return false;
  });

  zeal->hooks->Add("ProcessMouseAndGetKey", 0x0052437f, ProcessMouseAndGetKey, hook_type_detour);
  zeal->hooks->Add("HandleMouseWheel", 0x55B2E0, HandleMouseWheel, hook_type_detour);
  zeal->hooks->Add("procMouse", 0x537707, procMouse, hook_type_detour);
  zeal->hooks->Add("RMouseDown", 0x54699d, RMouseDown, hook_type_detour);
  zeal->hooks->Add("DoCamAI", 0x4db384, DoCamAI, hook_type_detour);
  zeal->hooks->Add("GetClickedActor", 0x004b008a, GetClickedActor, hook_type_detour);
  zeal->hooks->Add("EQ3DView_MouseUp", 0x0043c8af, EQ3DView_MouseUp, hook_type_detour);
  zeal->hooks->Add("LeftClickedOnPlayer", 0x0053271e, LeftClickedOnPlayer, hook_type_detour);

  FARPROC gfx_dx8 = GetProcAddress(GetModuleHandleA("eqgfx_dx8.dll"), "t3dSetCameraLens");
  if (gfx_dx8 != NULL) zeal->hooks->Add("SetCameraLens", (int)gfx_dx8, SetCameraLens, hook_type_detour);

  zeal->commands_hook->Add("/fov", {}, "Set your field of view requires a value between 45 and 90.",
                           [this](std::vector<std::string> &args) {
                             Zeal::GameStructures::CameraInfo *ci = Zeal::Game::get_camera();
                             if (ci) {
                               float _fov = 0;
                               if (args.size() > 1 && Zeal::String::tryParse(args[1], &_fov)) {
                                 if (_fov < 45 || _fov > 90) {
                                   Zeal::Game::print_chat("Use a fov value between 45 and 90");
                                   return true;
                                 }

                                 fov.set(_fov);
                                 if (update_options_ui_callback) update_options_ui_callback();
                               } else {
                                 Zeal::Game::print_chat("Current FOV [%f]", ci->FieldOfView);
                               }
                             }

                             return true;  // return true to stop the game from processing any further on this command,
                                           // false if you want to just add features to an existing cmd
                           });
  zeal->commands_hook->Add("/pandelay", {"/pd"},
                           "Adjust the delay required before left click panning happens in zeal cam.",
                           [this](std::vector<std::string> &args) {
                             int delay = 200;
                             if (args.size() == 2) {
                               if (Zeal::String::tryParse(args[1], &delay)) {
                                 pan_delay.set(delay);
                                 Zeal::Game::print_chat("Click to pan delay is now %i", pan_delay.get());
                               }
                             } else
                               Zeal::Game::print_chat("Invalid arguments for pandelay example usage: /pandelay 200");
                             return true;
                           });
  zeal->commands_hook->Add("/selfclickthru", {}, "Disable (on) or enable (off) clicking on self.",
                           [this](const std::vector<std::string> &args) {
                             if (args.size() == 2 && args[1] == "on")
                               setting_selfclickthru.set(true);
                             else if (args.size() == 2 && args[1] == "off")
                               setting_selfclickthru.set(false);
                             else
                               Zeal::Game::print_chat("Usage: /selfclickthru on or /selfclickthru off");
                             Zeal::Game::print_chat("Selfclickthru is now %s",
                                                    setting_selfclickthru.get() ? "on" : "off");
                             if (update_options_ui_callback) update_options_ui_callback();
                             return true;
                           });
  zeal->commands_hook->Add("/leftclickcon", {}, "Disable (on) or enable (off) left-click consider.",
                           [this](const std::vector<std::string> &args) {
                             if (args.size() == 2 && args[1] == "on")
                               setting_leftclickcon.set(true);
                             else if (args.size() == 2 && args[1] == "off")
                               setting_leftclickcon.set(false);
                             else
                               Zeal::Game::print_chat("Usage: /leftclickcon on or /leftclickcon off");
                             Zeal::Game::print_chat("Left click consider is now %s",
                                                    setting_leftclickcon.get() ? "on" : "off");
                             if (update_options_ui_callback) update_options_ui_callback();
                             return true;
                           });

  zeal->commands_hook->Add(
      "/zealcam", {"/smoothing"}, "Toggles the zealcam on/off as well as adjusting the sensitivities.",
      [this](std::vector<std::string> &args) {
        if (args.size() == 2 && Zeal::String::compare_insensitive(args[1], "info")) {
          Zeal::Game::print_chat("camera sensitivity FirstPerson : [% f] [% f] ThirdPerson : [% f] [% f] ",
                                 user_sensitivity_x, user_sensitivity_y, user_sensitivity_x_3rd,
                                 user_sensitivity_y_3rd);
          return true;
        } else if (args.size() == 3)  // the first arg is the command name itself
        {
          float x_sens = 0;
          float y_sens = 0;

          if (!Zeal::String::tryParse(args[1], &x_sens)) return true;
          if (!Zeal::String::tryParse(args[2], &y_sens)) return true;

          user_sensitivity_x = x_sens;
          user_sensitivity_y = y_sens;
          user_sensitivity_x_3rd = x_sens;
          user_sensitivity_y_3rd = y_sens;
          enabled.set(true);
          Zeal::Game::print_chat("New camera sensitivity [%f] [%f]", user_sensitivity_x, user_sensitivity_y);
        } else if (args.size() == 5) {
          float x_sens = 0;
          float y_sens = 0;
          float x_sens_3rd = 0;
          float y_sens_3rd = 0;

          if (!Zeal::String::tryParse(args[1], &x_sens)) return true;
          if (!Zeal::String::tryParse(args[2], &y_sens)) return true;
          if (!Zeal::String::tryParse(args[3], &x_sens_3rd)) return true;
          if (!Zeal::String::tryParse(args[4], &y_sens_3rd)) return true;

          user_sensitivity_x = x_sens;
          user_sensitivity_y = y_sens;
          user_sensitivity_x_3rd = x_sens_3rd;
          user_sensitivity_y_3rd = y_sens_3rd;
          enabled.set(true);
          Zeal::Game::print_chat("New camera sensitivity FirstPerson: [%f] [%f] ThirdPerson: [%f] [%f]",
                                 user_sensitivity_x, user_sensitivity_y, user_sensitivity_x_3rd,
                                 user_sensitivity_y_3rd);
        } else {
          enabled.toggle();
          if (ZealService::get_instance()->camera_mods->enabled.get()) {
            Zeal::Game::print_chat("Zealcam enabled");
            Zeal::Game::print_chat("camera sensitivity FirstPerson : [% f] [% f] ThirdPerson : [% f] [% f] ",
                                   user_sensitivity_x, user_sensitivity_y, user_sensitivity_x_3rd,
                                   user_sensitivity_y_3rd);
          } else {
            Zeal::Game::print_chat("Zealcam disabled");
          }
        }
        if (update_options_ui_callback) update_options_ui_callback();
        return true;
      });
}

CameraMods::~CameraMods() {}

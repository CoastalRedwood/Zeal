#include "camera_mods.h"

#include <windowsx.h>

#include <algorithm>
#include <thread>

#include "camera_math.h"
#include "game_addresses.h"
#include "game_functions.h"
#include "game_structures.h"
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
  if (Zeal::Game::get_gamestate() == GAMESTATE_CHARSELECT) {
    DWORD character_select = *(DWORD *)0x63D5D8;
    if (character_select) return *(BYTE *)(character_select + 0x171) != 0;
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
    Zeal::Game::get_self()->Pitch = zeal_cam_pitch;
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
// and sets the campera specific yaw for the others.
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
  bool rbutton = *Zeal::Game::is_right_mouse_down;

  if (rbutton || (lbutton && camera_view == Zeal::GameEnums::CameraView::ZealCam)) {
    float delta_y = delta->y;
    float delta_x = delta->x;
    float t = 1.0f / 1.5f;
    smoothMouseDeltaX = camera_math::lerp(delta_x * sensitivity_x, smoothMouseDeltaX, t);
    smoothMouseDeltaY = camera_math::lerp(delta_y * sensitivity_y, smoothMouseDeltaY, t);
    if (fabs(smoothMouseDeltaX) > 5) smoothMouseDeltaY /= 2;

    delta->y = 0;  // May not be necessary but just in case reset to avoid downstream usage.
    delta->x = 0;

    if (*(byte *)0x7985E8)  // invert
      smoothMouseDeltaY = -smoothMouseDeltaY;

    if (Zeal::Game::can_move()) {
      if (rbutton) {
        if (fabs(smoothMouseDeltaX) > 0.1)
          self->MovementSpeedHeading = -smoothMouseDeltaX / 1000;
        else
          self->MovementSpeedHeading = 0;

        if (camera_view == Zeal::GameEnums::CameraView::ZealCam) {
          zeal_cam_yaw -= smoothMouseDeltaX;
          zeal_cam_yaw = fmodf(zeal_cam_yaw + 512.f, 512.f);  // Wrap within 0 to 512.
          self->Heading = zeal_cam_yaw;                       // Should only set between 0 to 512.
        } else
          self->Heading += -smoothMouseDeltaX;
      }
    }
    if (lbutton || (rbutton && !Zeal::Game::can_move())) {
      zeal_cam_yaw -= smoothMouseDeltaX;
    }

    if (fabs(smoothMouseDeltaY) > 0) {
      auto self_pitch = get_pitch_control_entity();  // Control entity varies based on mounted state.
      if (camera_view == Zeal::GameEnums::CameraView::ZealCam) {
        zeal_cam_pitch -= smoothMouseDeltaY;
        zeal_cam_pitch = std::clamp(zeal_cam_pitch, -89.9f, 89.9f);
        if (Zeal::Game::KeyMods->Shift) self_pitch->Pitch = camera_math::pitch_to_game(zeal_cam_pitch);
      } else {
        if (self_pitch == Zeal::Game::get_view_actor_entity()) {
          self_pitch->Pitch -= smoothMouseDeltaY;
          self_pitch->Pitch = std::clamp(self_pitch->Pitch, -128.f, 128.f);
        }
      }
    }
  }

  return true;  // Skip hooked procMouse.
}

// This callback is invoked when the enabled setting is changed. It synchronizes the state.
void CameraMods::synchronize_set_enable() {
  set_zeal_cam_active(get_camera_view() == Zeal::GameEnums::CameraView::ZealCam);
  ZealService::get_instance()->ui->options->UpdateOptions();  // Can be called by command line.
}

// Interpolate zoom is called repeatedly in main_callback(). The alpha coefficient below acts as a
// first order IIR low pass filter for smoothing out the transitions.
void CameraMods::interpolate_zoom() { zeal_cam_zoom = zeal_cam_zoom + (desired_zoom - zeal_cam_zoom) * 0.3f; }

// check to help fix left mouse panning from preventing repositioning the game in windowed mode.
static bool is_over_title_bar(void) {
  // was going to reuse Zeal::GameStructures::MouseDelta* but the values flow over INT16
  // Zeal::GameStructures::MouseDelta* mouse_pos = (Zeal::GameStructures::MouseDelta*)0x798580;
  WORD mouse_y = *(WORD *)0x798582;
  return (mouse_y >= 0xE6FF && mouse_y <= 0xFFFF);
}

// Periodic call to handle left button panning in ZealCam.
void CameraMods::update_left_pan(DWORD camera_view) {
  if (!*Zeal::Game::is_right_mouse_down && *Zeal::Game::is_left_mouse_down && is_zeal_cam_active() &&
      !is_over_title_bar() && !Zeal::Game::is_game_ui_window_hovered()) {
    if (!lmouse_time) {
      GetCursorPos(&lmouse_cursor_pos);
      hide_cursor = true;
      lmouse_time = GetTickCount64();
    }

    if (GetTickCount64() - lmouse_time > pan_delay.get()) {
      HWND gwnd = Zeal::Game::get_game_window();
      POINT cursor_pos_for_window;
      GetCursorPos(&cursor_pos_for_window);
      if (gwnd == WindowFromPoint(cursor_pos_for_window)) {
        if (hide_cursor && GetTickCount64() - lmouse_time > pan_delay.get()) {
          mem::write<byte>(0x53edef, 0xEB);  // Unconditional jump past a showcursor.
          hide_cursor = false;
        }
        handle_proc_mouse();
        SetCursorPos(lmouse_cursor_pos.x, lmouse_cursor_pos.y);
      }
    }
  } else {
    if (lmouse_time) mem::write<byte>(0x53edef, 0x75);  // Restore showcursor check.
    lmouse_time = 0;
  }
}

// Called repeatedly by main_callback() to handle periodic calls and process held down keys.
void CameraMods::process_time_tick() {
  DWORD camera_view = get_camera_view();
  bool zoom_out = kKeyDownStates[CMD_ZOOM_OUT];
  if (zoom_out && camera_view == Zeal::GameEnums::CameraView::FirstPerson) {
    set_zeal_cam_active(true);  // Activates if enabled.
    zoom_out = false;           // Reset zoom key press so not double-counted below.
  }

  update_left_pan(camera_view);  // Call to keep cursor visibility updated.

  if (!is_zeal_cam_active()) return;

  if (kKeyDownStates[CMD_ZOOM_IN])
    update_desired_zoom(-0.3f);
  else if (zoom_out)
    update_desired_zoom((min_zoom_in > desired_zoom) ? (min_zoom_in - desired_zoom) : 0.3f);
  interpolate_zoom();

  // Cam lock mode yaws camera to match heading when enabled.
  Zeal::GameStructures::Entity *self = Zeal::Game::get_controlled();
  if ((kKeyDownStates[CMD_RIGHT] || kKeyDownStates[CMD_LEFT]) && cam_lock.get()) {
    zeal_cam_yaw = self->Heading;
  }

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

  float current_sens = (float)(*(byte *)0x798b0c);
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
}

// Called periodically to keep the camera synced and compensate for fps rates.
void CameraMods::callback_main() {
  static int prev_view = get_camera_view();
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

// Overrides the default FOV when enabled.
static int SetCameraLens(int a1, float fov, float aspect_ratio, float a4, float a5) {
  ZealService *zeal = ZealService::get_instance();
  fov = zeal->camera_mods->fov.get();
  int rval = zeal->hooks->hook_map["SetCameraLens"]->original(SetCameraLens)(a1, fov, aspect_ratio, a4, a5);
  if (Zeal::Game::get_gamestate() != GAMESTATE_PRECHARSELECT) {
    Zeal::GameStructures::CameraInfo *ci = Zeal::Game::get_camera();
    if (ci) ci->FieldOfView = zeal->camera_mods->fov.get();
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

CameraMods::CameraMods(ZealService *zeal) {
  mem::write<BYTE>(0x4db8d9, 0xEB);  // Unconditional jump to skip an optional bad camera position debug message.

  lastTime = std::chrono::steady_clock::now();
  zeal->callbacks->AddGeneric([this]() { callback_main(); }, callback_type::MainLoop);
  zeal->callbacks->AddGeneric([this]() { callback_main(); }, callback_type::CharacterSelectLoop);
  zeal->callbacks->AddGeneric([this]() { ui_active = true; }, callback_type::InitUI);
  zeal->callbacks->AddGeneric([this]() { ui_active = true; }, callback_type::InitCharSelectUI);
  zeal->callbacks->AddGeneric([this]() { ui_active = false; }, callback_type::CleanUI);  // Also covers char select.
  zeal->callbacks->AddGeneric([this]() { callback_zone(); }, callback_type::Zone);

  zeal->binds_hook->replace_cmd(CMD_CENTER_VIEW, [](int state) {
    if (!state) kKeyDownStates[CMD_CENTER_VIEW] = state;  // Client is not clearing this state.
    return false;
  });

  zeal->hooks->Add("HandleMouseWheel", 0x55B2E0, HandleMouseWheel, hook_type_detour);
  zeal->hooks->Add("procMouse", 0x537707, procMouse, hook_type_detour);
  zeal->hooks->Add("RMouseDown", 0x54699d, RMouseDown, hook_type_detour);
  zeal->hooks->Add("DoCamAI", 0x4db384, DoCamAI, hook_type_detour);
  zeal->hooks->Add("GetClickedActor", 0x004b008a, GetClickedActor, hook_type_detour);

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
        return true;
      });
}

CameraMods::~CameraMods() {}

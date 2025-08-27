#pragma once
#include <Windows.h>

#include <chrono>
#include <functional>

#include "game_functions.h"
#include "memory.h"
#include "vectors.h"
#include "zeal_settings.h"

class CameraMods {
 public:
  ZealSetting<bool> enabled = {true, "Zeal", "MouseSmoothing", false, [this](bool val) { synchronize_set_enable(); }};
  ZealSetting<bool> cam_lock = {true, "Zeal", "CamLock", false};
  ZealSetting<bool> use_old_sens = {false, "Zeal", "OldSens", false};
  ZealSetting<float> user_sensitivity_x = {0.1f, "Zeal", "MouseSensitivityX", false};
  ZealSetting<float> user_sensitivity_y = {0.1f, "Zeal", "MouseSensitivityY", false};
  ZealSetting<float> user_sensitivity_x_3rd = {0.1f, "Zeal", "MouseSensitivityX3rd", false};
  ZealSetting<float> user_sensitivity_y_3rd = {0.1f, "Zeal", "MouseSensitivityY3rd", false};
  ZealSetting<float> fov = {45.f, "Zeal", "Fov", false, [this](float val) {
                              Zeal::GameStructures::CameraInfo *ci = Zeal::Game::get_camera();
                              if (ci) ci->FieldOfView = val;
                            }};
  ZealSetting<int> pan_delay = {0, "Zeal", "PanDelay", false};
  ZealSetting<bool> setting_selfclickthru = {false, "Zeal", "SelfClickThru", false};
  ZealSetting<bool> setting_leftclickcon = {false, "Zeal", "LeftClickCon", false};
  ZealSetting<bool> setting_toggle_overhead_view = {true, "Camera", "ToggleOverheadView", false};
  ZealSetting<bool> setting_toggle_zeal_view = {true, "Camera", "ToggleZealView", false};
  ZealSetting<bool> setting_toggle_free1_view = {true, "Camera", "ToggleFree1View", false};
  ZealSetting<bool> setting_toggle_free2_view = {true, "Camera", "ToggleFree2View", false};

  const float max_zoom_out = 100;
  const float min_zoom_in = 5.f;  // Transitions to first person below this.
  const float zoom_speed = 5.f;

  CameraMods(class ZealService *pHookWrapper);
  ~CameraMods();
  bool handle_mouse_wheel(int delta);
  bool handle_proc_mouse();
  void handle_proc_rmousedown(int x, int y);
  void handle_do_cam_ai();

  void add_options_callback(std::function<void()> callback) { update_options_ui_callback = callback; };

 private:
  float fps = 60.f;
  float zeal_cam_pitch = 0.f;
  float zeal_cam_yaw = 0.f;
  float zeal_cam_zoom = 0.f;
  float desired_zoom = 0.f;
  ULONGLONG lmouse_time = 0;
  POINT lmouse_cursor_pos;
  bool hide_cursor = false;
  float sensitivity_x = 0.7f;
  float sensitivity_y = 0.4f;
  std::chrono::steady_clock::time_point lastTime;
  bool ui_active = false;
  bool reset_camera = false;  // Allows for a deferred reset of zeal cam.
  std::function<void()> update_options_ui_callback;

  void synchronize_set_enable();
  void synchronize_old_ui();
  void handle_toggle_cam();
  void callback_zone();
  void callback_main();
  bool callback_packet(UINT opcode, char *buffer, UINT len);
  void update_desired_zoom(float zoom);
  void set_zeal_cam_active(bool activate);
  bool is_zeal_cam_active() const;
  bool calc_camera_positions(Vec3 &head_pos, Vec3 &wanted_pos) const;
  void update_left_pan(DWORD camera_view);
  void interpolate_zoom();
  void process_time_tick();
  void update_fps_sensitivity();
};

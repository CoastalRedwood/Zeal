#pragma once
#include <Windows.h>

#include "zeal_settings.h"

// Draws a simple arrow mouse cursor while the UI is hidden (F10). The client draws its cursor as part of the
// window manager's DrawWindows(), which does not run in the hidden UI mode, so the cursor vanishes even though the
// mouse still works for clicking on mobs in the world. This class fills that gap with flat colored geometry.
class HideUiCursor {
 public:
  explicit HideUiCursor(class ZealService *zeal);
  ~HideUiCursor() = default;

  // Disable copy.
  HideUiCursor(HideUiCursor const &) = delete;
  HideUiCursor &operator=(HideUiCursor const &) = delete;

  ZealSetting<bool> setting_enabled = {true, "Zeal", "ShowCursorWithUiHidden", false};

 private:
  void CallbackRender();
  bool ShouldDraw() const;
  void DrawArrow(float x, float y);
};

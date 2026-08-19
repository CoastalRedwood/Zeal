#pragma once
#include "zeal_settings.h"

class Tooltip {
 public:
  ZealSetting<bool> all_containers = {false, "Zeal", "alt_all_containers", false,
                                      [this](const bool& val) { synchronize_alt_all_containers(); }};
  ZealSetting<int> hover_timeout = {500, "Zeal", "TooltipTime", false,
                                    [this](const int& val) { synchronize_hover_timeout(); }};
  Tooltip(class ZealService* pHookWrapper);
  ~Tooltip();

  // Used by mouseover_display to suppress the native name-only tooltip on windows it covers,
  // by pushing the in-memory hover delay out of reach (and restoring it when released).
  void set_native_tooltip_suppressed(bool suppressed);

 private:
  void synchronize_hover_timeout();
  void synchronize_alt_all_containers();
  bool native_tooltip_suppressed = false;  // Keeps /tooltiptimer from clearing an active suppression.
};

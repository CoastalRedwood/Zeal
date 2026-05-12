#pragma once
#include "callbacks.h"
#include "zeal_settings.h"

class ZealService;

class UI_HideFakeSlots {
 public:
  UI_HideFakeSlots(ZealService* zeal);
  ~UI_HideFakeSlots();

  ZealSetting<bool> Enabled;
  void update_slot_visibility();
};
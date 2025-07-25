#pragma once
#include <stdint.h>

#include <unordered_map>

#include "bitmap_font.h"
#include "d3dx8/d3d8.h"
#include "game_structures.h"
#include "game_ui.h"
#include "hook_wrapper.h"
#include "memory.h"
#include "zeal_settings.h"

struct ExploreZoneData {
  Vec3 Position;
  float Heading;

  ExploreZoneData(Vec3 _Position, float _Heading) {
    Heading = _Heading;
    Position = _Position;
  }

  ExploreZoneData() {
    Position = {0, 0, 0};
    Heading = 0;
  }
};

class CharacterSelect {
 private:
  std::unique_ptr<BitmapFont> bmp_font;
  void load_bmp_font();
  void render();
  void load_zonedata();

 public:
  CharacterSelect(class ZealService *zeal);
  ~CharacterSelect();
  ZealSetting<int> ZoneIndex = {-1, "CharacterSelect", "ZoneIndex", false};
  std::unordered_map<int, ExploreZoneData> ZoneData;
};

// Auto-generated file using maps_to_cpp.py
#pragma once
#include <stdint.h>

struct ZoneMapLine {
  int16_t x0;  // Starting point of line segment.
  int16_t y0;
  int16_t z0;
  int16_t x1;  // Ending point of line segment.
  int16_t y1;
  int16_t z1;
  uint8_t red;  // Line RGB color.
  uint8_t green;
  uint8_t blue;
  int8_t level_id;
};

struct ZoneMapLabel {
  int16_t x;  // Label location.
  int16_t y;
  int16_t z;
  uint8_t red;  // Label color.
  uint8_t green;
  uint8_t blue;
  const char* label;  // Label text.
};

static constexpr int8_t kZoneMapInvalidLevelId = -128;

struct ZoneMapLevel {
  int8_t level_id;  // -6 to +4, InvalidLevelId for non-level
  int max_z;
  int min_z;
};

struct ZoneMapData {
  const char* name;
  const int max_x;
  const int min_x;
  const int max_y;
  const int min_y;
  const int max_z;
  const int min_z;
  const int num_lines;
  const int num_labels;
  const int num_levels;
  const ZoneMapLine* lines;
  const ZoneMapLabel* labels;
  const ZoneMapLevel* levels;  // Sorted by ascending level_id.
};

const ZoneMapData* get_zone_map_data(int zone_id);

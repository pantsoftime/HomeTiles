#pragma once

#include <cstdint>

// FORK: monospace value fonts (JetBrains Mono, see tile_renderer_fonts.h),
// numbered far above upstream's size choices so the two cannot collide again.
// Fork releases before v0.7.01 stored them as 5-8; upstream v0.7.0 then gave 5
// to its 28 px size. 6-8 have never been upstream values, so they are read
// forward on load. A stored 5 now means 28: it was "20 Mono", which no panel
// used when this changed. tile_config.h asserts that upstream still stops at 5.
static constexpr uint8_t SENSOR_VALUE_FONT_MONO_20 = 200;
static constexpr uint8_t SENSOR_VALUE_FONT_MONO_24 = 201;
static constexpr uint8_t SENSOR_VALUE_FONT_MONO_BOLD_20 = 202;
static constexpr uint8_t SENSOR_VALUE_FONT_MONO_BOLD_24 = 203;
inline bool sensor_value_font_is_mono(int choice) {
  return choice >= SENSOR_VALUE_FONT_MONO_20 && choice <= SENSOR_VALUE_FONT_MONO_BOLD_24;
}
inline uint8_t sensor_value_font_from_legacy_fork(uint8_t choice) {
  switch (choice) {
    case 6: return SENSOR_VALUE_FONT_MONO_24;
    case 7: return SENSOR_VALUE_FONT_MONO_BOLD_20;
    case 8: return SENSOR_VALUE_FONT_MONO_BOLD_24;
    default: return choice;
  }
}

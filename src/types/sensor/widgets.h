#pragma once

#include <stdint.h>
#include <lvgl.h>

struct SensorTileWidgets {
  lv_obj_t* value_label = nullptr;
  // Optional small line under the value (humidity next to a temperature,
  // say). Fed from the same entity: the payload's first line goes in
  // value_label and the remainder here, so no second subscription is
  // involved.
  lv_obj_t* subtitle_label = nullptr;
  // Icon for per-tile color rules; recolored only on state updates.
  lv_obj_t* icon_label = nullptr;
  lv_obj_t* unit_label = nullptr;
  lv_obj_t* gauge = nullptr;
  // The gauge's bounds with their decimals (gauge_range.h).
  float gauge_min = 0;
  float gauge_max = 100;
  lv_obj_t* chart = nullptr;
  lv_chart_series_t* series = nullptr;
};

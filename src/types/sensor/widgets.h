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
  lv_obj_t* unit_label = nullptr;
  lv_obj_t* gauge = nullptr;
  int32_t gauge_min = 0;
  int32_t gauge_max = 100;
  lv_obj_t* chart = nullptr;
  lv_chart_series_t* series = nullptr;
};

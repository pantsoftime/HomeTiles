#include "src/types/clock/clock_face.h"

#include <Arduino.h>
#include <math.h>
#include <new>
#include <time.h>

namespace clock_face {
namespace {

constexpr float kPi = 3.14159265f;

struct Face {
  lv_obj_t* dial = nullptr;
  lv_obj_t* hour = nullptr;
  lv_obj_t* minute = nullptr;
  lv_point_precise_t hour_points[2] = {};
  lv_point_precise_t minute_points[2] = {};
  lv_point_precise_t marks[12][2] = {};
  lv_coord_t size = 0;
  // Hand lengths as shares of the radius (the mockup's: analog 0.45 / 0.66,
  // wall clock 0.5 / 0.78).
  float hour_share = 0.45f;
  float minute_share = 0.66f;
  int shown = -1;
  lv_timer_t* timer = nullptr;
};

lv_obj_t* plain(lv_obj_t* parent) {
  lv_obj_t* obj = lv_obj_create(parent);
  if (!obj) return nullptr;
  lv_obj_remove_style_all(obj);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  return obj;
}

// A point `share` of the radius from the center at `degrees` clockwise from 12.
lv_point_precise_t polar(float radius, float degrees, float share) {
  const float a = degrees * kPi / 180.0f;
  lv_point_precise_t p;
  p.x = static_cast<lv_value_precise_t>(radius + share * radius * sinf(a));
  p.y = static_cast<lv_value_precise_t>(radius - share * radius * cosf(a));
  return p;
}

lv_obj_t* line(lv_obj_t* dial, lv_coord_t size, const lv_point_precise_t* points, lv_coord_t width, uint32_t color) {
  lv_obj_t* obj = lv_line_create(dial);
  if (!obj) return nullptr;
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_pos(obj, 0, 0);
  lv_obj_set_size(obj, size, size);
  lv_obj_set_style_line_width(obj, width, 0);
  lv_obj_set_style_line_color(obj, lv_color_hex(color), 0);
  lv_obj_set_style_line_rounded(obj, true, 0);
  lv_line_set_points(obj, points, 2);
  return obj;
}

lv_obj_t* dot(lv_obj_t* dial, lv_point_precise_t center, lv_coord_t diameter, uint32_t color) {
  lv_obj_t* obj = plain(dial);
  if (!obj) return nullptr;
  lv_obj_set_size(obj, diameter, diameter);
  lv_obj_set_pos(obj, static_cast<lv_coord_t>(center.x) - diameter / 2, static_cast<lv_coord_t>(center.y) - diameter / 2);
  lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  return obj;
}

// The hands for the time now, only when the minute changed.
void update(Face* face) {
  struct tm now;
  if (!face || !getLocalTime(&now, 0)) return;
  const int minute_of_day = now.tm_hour * 60 + now.tm_min;
  if (minute_of_day == face->shown) return;
  face->shown = minute_of_day;
  const float radius = face->size / 2.0f;
  const float hour_deg = ((now.tm_hour % 12) + now.tm_min / 60.0f) * 30.0f;
  const float minute_deg = now.tm_min * 6.0f;
  face->hour_points[0] = polar(radius, 0, 0);
  face->hour_points[1] = polar(radius, hour_deg, face->hour_share);
  face->minute_points[0] = polar(radius, 0, 0);
  face->minute_points[1] = polar(radius, minute_deg, face->minute_share);
  if (face->hour) lv_line_set_points(face->hour, face->hour_points, 2);
  if (face->minute) lv_line_set_points(face->minute, face->minute_points, 2);
}

void timer_cb(lv_timer_t* timer) { update(static_cast<Face*>(lv_timer_get_user_data(timer))); }

}  // namespace

lv_obj_t* create(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_coord_t size, const Colors& colors, Marks marks,
                 bool small) {
  if (!parent || size < 8) return nullptr;
  Face* face = new (std::nothrow) Face{};
  if (!face) return nullptr;
  lv_obj_t* dial = plain(parent);
  if (!dial) {
    delete face;
    return nullptr;
  }
  face->dial = dial;
  face->size = size;
  lv_obj_set_pos(dial, x, y);
  lv_obj_set_size(dial, size, size);
  lv_obj_set_style_radius(dial, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(dial, lv_color_hex(colors.dial), 0);
  lv_obj_set_style_bg_opa(dial, LV_OPA_COVER, 0);
  lv_obj_add_flag(dial, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

  const float radius = size / 2.0f;
  for (int i = 0; i < 12; ++i) {
    const bool quarter = i % 3 == 0;
    if (marks == Marks::kBatons) {
      face->marks[i][0] = polar(radius, i * 30.0f, small ? 0.62f : 0.68f);
      face->marks[i][1] = polar(radius, i * 30.0f, 0.86f);
      line(dial, size, face->marks[i], LV_MAX(1, size / 50), colors.major);
    } else if (quarter || !small) {
      const lv_coord_t diameter = LV_MAX(3, static_cast<lv_coord_t>(size * (quarter ? 0.048f : 0.028f)));
      dot(dial, polar(radius, i * 30.0f, 0.8f), diameter, quarter ? colors.major : colors.minor);
    }
  }
  // The hands from the center; placed by update().
  const bool batons = marks == Marks::kBatons;
  face->hour_share = batons ? 0.5f : 0.45f;
  face->minute_share = batons ? 0.78f : 0.66f;
  const lv_coord_t hour_width = LV_MAX(2, static_cast<lv_coord_t>(size * (batons ? 0.022f : 0.05f)));
  const lv_coord_t minute_width = LV_MAX(2, static_cast<lv_coord_t>(size * (batons ? 0.016f : 0.032f)));
  face->hour_points[0] = face->hour_points[1] = polar(radius, 0, 0);
  face->minute_points[0] = face->minute_points[1] = polar(radius, 0, 0);
  face->hour = line(dial, size, face->hour_points, hour_width, colors.hour);
  face->minute = line(dial, size, face->minute_points, minute_width, colors.minute);
  const lv_coord_t center = LV_MAX(4, static_cast<lv_coord_t>(size * (batons ? 0.036f : 0.09f)));
  dot(dial, polar(radius, 0, 0), center, colors.center);
  update(face);
  face->timer = lv_timer_create(timer_cb, 1000, face);

  lv_obj_add_event_cb(
      dial,
      [](lv_event_t* e) {
        Face* face = static_cast<Face*>(lv_event_get_user_data(e));
        if (!face) return;
        if (face->timer) lv_timer_delete(face->timer);
        delete face;
      },
      LV_EVENT_DELETE, face);
  return dial;
}

}  // namespace clock_face

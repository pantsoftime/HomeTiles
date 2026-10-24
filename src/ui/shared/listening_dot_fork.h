#pragma once

// FORK: listening dot. While the microphone feeds Assist (from the wake word
// to the end of the capture, speaker::assistCapturing()) a small orange dot
// sits in the screen's top left corner, in the gap between the screen corner
// and the rounded corner of the first tile (user 2026-10-24: the panel gave
// no sign that it listened, so the wake word was said again and became the
// request). Orange like a phone's microphone-in-use dot.
// The dot lies on the system layer, above every screen, popup and the
// screensaver. Its place follows the shown grid margin and the tile radius,
// read each time it appears: centred halfway between the screen corner and
// the nearest point of the tile's corner arc, its radius a bit over half the
// free distance, so it never touches the tile or the screen edge.
// Polls like camera_indicator.h. Header-only; ui_manager.cpp starts it.

#include <lvgl.h>

#include "src/audio/speaker.h"
#include "src/tiles/config/tile_config.h"
#include "src/tiles/runtime/tile_renderer_fonts.h"
#include "src/ui/shared/ui_surface_style.h"
#include "src/ui/shared/listening_dot_place_fork.h"

namespace listening_dot {
namespace detail {

constexpr uint32_t kPollMs = 100;
constexpr uint32_t kOrange = 0xFF9F0A;

inline lv_obj_t*& dot() {
  static lv_obj_t* instance = nullptr;
  return instance;
}

inline void show(bool shown) {
  lv_obj_t*& obj = dot();
  if (!shown) {
    if (obj && !lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  if (!obj) {
    obj = lv_obj_create(lv_layer_sys());
    lv_obj_remove_style_all(obj);
    lv_obj_set_style_bg_color(obj, lv_color_hex(kOrange), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_remove_flag(obj, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_add_flag(obj, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_IGNORE_LAYOUT | LV_OBJ_FLAG_HIDDEN));
  }
  if (!lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return;
  // The same outer tile radius the tiles and camera_indicator.h use.
  const listening_dot_place::Place spot = listening_dot_place::place(GRID_PAD_LEFT, GRID_PAD_TOP,
                           ui_surface_style::radius(tile_layout::scale_480(22)));
  lv_obj_set_size(obj, 2 * spot.radius, 2 * spot.radius);
  lv_obj_set_pos(obj, spot.center_x - spot.radius, spot.center_y - spot.radius);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

inline void poll(lv_timer_t*) { show(speaker::assistCapturing()); }

}  // namespace detail

// The Settings switch clears the framebuffer and redraws only what it
// invalidates (camera_indicator::invalidateVisible()). LVGL task only.
inline void invalidateVisible() {
  lv_obj_t* obj = detail::dot();
  if (obj && !lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) lv_obj_invalidate(obj);
}

// Starts the poll timer on panels with a microphone. LVGL task only.
inline void init() {
  static bool started = false;
  if (started || !speaker::supported()) return;
  started = true;
  lv_timer_create(detail::poll, detail::kPollMs, nullptr);
}

}  // namespace listening_dot

#pragma once

// FORK: where the listening dot (listening_dot_fork.h) sits. Pure, host-tested.

#include <algorithm>
#include <cmath>

namespace listening_dot_place {

// Share of the free distance the dot's radius takes (the rest is margin).
constexpr float kFill = 0.55f;
constexpr int kMinRadius = 2;
constexpr int kMaxRadius = 10;

struct Place {
  int center_x;
  int center_y;
  int radius;
};

// The corner arc of the first tile: centre (pad_left + R, pad_top + R). Its
// nearest point to the screen corner lies on the line from the corner to that
// centre, R short of it; the dot sits halfway along, as large as the smaller
// of that half distance and its distance to either screen edge allows.
inline Place place(int pad_left, int pad_top, int tile_radius) {
  const float cx = static_cast<float>(pad_left + tile_radius);
  const float cy = static_cast<float>(pad_top + tile_radius);
  const float length = std::sqrt(cx * cx + cy * cy);
  const float reach = length > 0.0f ? 1.0f - tile_radius / length : 0.0f;
  const float px = cx * reach;
  const float py = cy * reach;
  const float ox = px / 2.0f;
  const float oy = py / 2.0f;
  const float room = std::min({ox, oy, std::sqrt(px * px + py * py) / 2.0f});
  const int radius = std::clamp(static_cast<int>(room * kFill), kMinRadius, kMaxRadius);
  return {static_cast<int>(std::lround(ox)), static_cast<int>(std::lround(oy)), radius};
}

}  // namespace listening_dot_place

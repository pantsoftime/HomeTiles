#pragma once

#include <stdint.h>

#include "src/ui/popups/popup_layout.h"

namespace camera_geometry {

constexpr uint16_t evenCeil(uint32_t numerator, uint32_t denominator) {
  const uint32_t rounded = (numerator + denominator - 1U) / denominator;
  return static_cast<uint16_t>((rounded + 1U) & ~1U);
}

// The camera frame always fills the popup width. A 16:9 height is rounded up
// to an even value so FFmpeg can produce yuv420 JPEG frames without padding
// the visible image differently on each display profile.
inline constexpr uint16_t kWidth =
    static_cast<uint16_t>(popup_layout::kContentWidth & ~1);
inline constexpr uint16_t kHeight = evenCeil(
    static_cast<uint32_t>(kWidth) * 9U, 16U);
inline constexpr uint16_t kCornerRadius =
    static_cast<uint16_t>(popup_layout::scale480(18));
// Target for the bounded low-latency camera path. 24 FPS leaves enough time
// for JPEG decode plus the synchronized PPA/DSI presentation while MQTT keeps
// running on the other core.
// Requested stream rate, sent to the bridge in the camera "open" payload and
// used to validate its reply. This is the panel asking for what it can
// actually absorb, not a cap on the source.
//
// 24 was more than these panels can present. Measured on a Waveshare 4B
// watching a PTZ camera: ffmpeg produced 24 fps, the panel acknowledged only
// 8-12, and the bridge discarded 58-86 frames per five-second interval. Wire
// rate was ~1 Mbit/s, so the limit is decode and present latency, not network:
// a single 8 KB chunk took 32-57 ms to acknowledge, spiking past 1000 ms. The
// bridge's ACK timeout is 5 s, and when the panel's DMA headroom guard aborts
// a frame mid-transfer the two ends desync and the stream dies with
// camera_invalid_ack ("Invalid camera response" on the tile).
//
// Asking for 15 keeps a margin above the ~10 fps actually presented while
// removing the frames that were only ever going to be dropped.
inline constexpr uint8_t kFps = 15;

// ESP32-P4's JPEG hardware decoder writes in 16-pixel-aligned dimensions.
// LVGL still receives the visible width/height and the aligned row stride.
inline constexpr uint16_t kDecodedWidth = (kWidth + 15U) & ~15U;
inline constexpr uint16_t kDecodedHeight = (kHeight + 15U) & ~15U;

static_assert(kWidth >= 320 && kWidth <= 752,
              "Camera popup width is outside the supported P4 range");
static_assert(kHeight >= 180 && kHeight <= 424,
              "Camera popup height is outside the supported P4 range");

}  // namespace camera_geometry

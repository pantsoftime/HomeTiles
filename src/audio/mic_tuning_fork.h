#pragma once

// FORK: microphone tuning for Assist and the wake word. Which I2S slot (mic)
// feeds the mono stream, and a digital boost on top of the codec's analog
// gain. The Bridge asks Home Assistant for no gain of its own (no
// AudioSettings: auto gain off, volume multiplier 1), so the level the panel
// sends is the level speech-to-text gets; on the Tab5 normal speech at a
// metre stayed below Home Assistant's voice detection (user 2026-10-24: "had
// to speak right into the microphone"). Pure, host-tested.

#include <stddef.h>
#include <stdint.h>

namespace mic_tuning {

// Mix averages both slots (upstream's behaviour); Left/Right take one slot,
// for a board whose other slot is a quieter mic or no mic at all.
enum class Channel : uint8_t { Mix = 0, Left = 1, Right = 2 };
constexpr uint8_t kChannelCount = 3;

constexpr uint8_t kMaxBoostDb = 24;

// The boost as a Q8 factor: round(256 * 10^(dB / 20)) for 0..24 dB.
inline uint32_t boostQ8(uint8_t db) {
  static constexpr uint16_t kTable[kMaxBoostDb + 1] = {
      256,  287,  322,  362,  406,  455,  511,  573,  643,  722,  810,  908,  1019,
      1144, 1283, 1440, 1615, 1812, 2033, 2282, 2560, 2872, 3223, 3616, 4057};
  return kTable[db > kMaxBoostDb ? kMaxBoostDb : db];
}

inline int16_t saturate(int32_t value) {
  if (value > 32767) return 32767;
  if (value < -32768) return -32768;
  return static_cast<int16_t>(value);
}

// Interleaved 16-bit stereo frames to mono through the chosen slot(s), times
// the Q8 boost, clipped at full scale.
inline void toMono(const int16_t* stereo, size_t frames, Channel channel, uint32_t boost_q8,
                   int16_t* out) {
  const int32_t gain = static_cast<int32_t>(boost_q8);
  for (size_t i = 0; i < frames; ++i) {
    int32_t sample;
    switch (channel) {
      case Channel::Left: sample = stereo[2 * i]; break;
      case Channel::Right: sample = stereo[2 * i + 1]; break;
      default: sample = (static_cast<int32_t>(stereo[2 * i]) + stereo[2 * i + 1]) / 2; break;
    }
    out[i] = saturate((sample * gain) / 256);
  }
}

}  // namespace mic_tuning

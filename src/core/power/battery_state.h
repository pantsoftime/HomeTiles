#ifndef BATTERY_STATE_H
#define BATTERY_STATE_H

#include <Arduino.h>

struct BatteryTelemetry {
  bool on_mains = false;
  bool battery_missing = false;
  bool level_valid = false;
  int32_t level_pct = -1;
  int32_t raw_level_pct = -1;
  int32_t voltage_mv = 0;
  int32_t current_ma = 0;
  int16_t vbus_mv = -1;
  bool has_vbus = false;
  bool charging = false;
};

// Capability is independent from whether a battery is temporarily absent.
bool batteryStateSupportsMeasurement();
void batteryStateUpdate();
const BatteryTelemetry& batteryStateGet();
bool batteryStateIsOnMains();
bool batteryStateIsBatteryMissing();
bool batteryStateHasDisplayPercent();
int32_t batteryStateDisplayPercent();
int32_t batteryStateRawPercent();

// FORK: which way the battery current flows, for the Settings tile's
// indicator. Decided from the measured current (CHG_STAT is unreliable on the
// Tab5: it reads "charging" at ~2 mA) with a dead band and a hold, so a short
// load spike does not flip it. Unknown without a measured battery.
enum class BatteryFlow : uint8_t { Unknown, Charging, Discharging, Idle };
BatteryFlow batteryStateFlow();

#endif // BATTERY_STATE_H

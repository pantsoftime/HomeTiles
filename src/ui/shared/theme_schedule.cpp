#include "src/ui/shared/theme_schedule.h"

#include <Arduino.h>
#include <string.h>
#include <time.h>

#include "src/core/config/config_manager.h"
#include "src/types/weather/weather_icons.h"
#include "src/ui/shared/ui_theme.h"
#include "src/ui/ui_manager.h"

namespace theme_schedule {
namespace {

weather_icons::SunTimes g_sun;
bool g_time_zone = false;
// The loop asks at most this often; sunrise and sunset need no more.
constexpr uint32_t kPollMs = 20000;

}  // namespace

bool day_at(const weather_icons::SunTimes& sun, const char* date, int minute) {
  if (date) {
    for (uint8_t i = 0; i < sun.count; ++i) {
      if (strncmp(sun.days[i].date, date, 10) == 0 && date[10] == '\0') {
        return !weather_icons::is_night(sun, date, minute);
      }
    }
  }
  return minute >= kFallbackDayStart && minute < kFallbackDayEnd;
}

uint8_t effective(uint8_t stored) {
  if (stored != kAutomatic) return stored == 1 ? 1 : 0;
  struct tm now {};
  // Before the time zone is set the clock shows UTC (or nothing yet).
  if (!g_time_zone || !getLocalTime(&now, 0) || now.tm_year < 2020 - 1900) {
    return configManager.getConfig().theme_auto_last;
  }
  char date[16];
  strftime(date, sizeof(date), "%Y-%m-%d", &now);
  return day_at(g_sun, date, now.tm_hour * 60 + now.tm_min) ? 1 : 0;
}

uint8_t at_boot(uint8_t stored) {
  return stored == kAutomatic ? configManager.getConfig().theme_auto_last : (stored == 1 ? 1 : 0);
}

void note_sun(const weather_icons::SunTimes& sun) {
  if (sun.count) g_sun = sun;
}

void note_time_zone() { g_time_zone = true; }

void poll() {
  static uint32_t last = 0;
  const uint32_t now = millis();
  if (last && now - last < kPollMs) return;
  last = now ? now : 1;
  const uint8_t stored = configManager.getConfig().theme;
  if (stored != kAutomatic) return;
  if ((effective(stored) == 1) != ui_theme::theme_light()) uiManager.requestThemeChange();
}

}  // namespace theme_schedule

#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include "translations.h"

namespace aura {

constexpr int forecast_rows = 7;
constexpr int rain_probability_threshold = 60;
constexpr uint32_t stale_after_ms = 30UL * 60 * 1000;
constexpr int64_t earliest_weather_time = 1577836800;  // 2020-01-01 UTC

// Probability belongs to [start, end), not to the hour after the API timestamp.
struct OutlookHour {
  int64_t start = 0;
  int64_t end = 0;
  float probability = NAN;
  int weather_code = -1;
  float temperature_c = 0;
  int is_day = 1;
};

struct ForecastDay {
  int64_t time = 0;
  float low_c = 0;
  float high_c = 0;
  int weather_code = 0;
};

struct WeatherOutlook {
  bool available = false;
  float temperature_c = 0;
  float feels_like_c = 0;
  int weather_code = 0;
  int is_day = 1;
  int64_t valid_at = 0;
  int utc_offset_seconds = 0;
  uint32_t received_ms = 0;
  OutlookHour hours[forecast_rows];
  ForecastDay days[forecast_rows];
};

enum class OutlookState { unavailable, stale, feels_like, precipitation };

inline int64_t forecast_now(const WeatherOutlook &weather, int64_t clock, uint32_t uptime) {
  if (clock >= earliest_weather_time) return clock;
  return weather.valid_at + static_cast<uint32_t>(uptime - weather.received_ms) / 1000;
}

inline bool weather_stale(const WeatherOutlook &weather, int64_t now, uint32_t uptime) {
  return static_cast<uint32_t>(uptime - weather.received_ms) >= stale_after_ms
      || now - weather.valid_at >= stale_after_ms / 1000;
}

inline bool valid_probability(float value) {
  return std::isfinite(value) && value >= 0 && value <= 100;
}

inline const char *precipitation_name(int code, const LocalizedStrings *s) {
  if (code == 71 || code == 73 || code == 75 || code == 77 || code == 85 || code == 86)
    return s->outlook_snow;
  if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82) || code == 95)
    return s->outlook_rain;
  return s->outlook_precipitation;
}

inline void format_outlook_hour(char *out, size_t size, const tm &value, bool clock_24h,
                               const char *suffix) {
  const int hour = clock_24h ? value.tm_hour : (value.tm_hour % 12 ? value.tm_hour % 12 : 12);
  if (value.tm_min)
    std::snprintf(out, size, clock_24h ? "%02d:%02d%s" : "%d:%02d%s", hour, value.tm_min, suffix);
  else
    std::snprintf(out, size, clock_24h ? "%02d%s" : "%d%s", hour, suffix);
}

inline void format_outlook_period(char *out, size_t size, const OutlookHour &period,
                                 int offset, bool clock_24h, const LocalizedStrings *s) {
  time_t start = static_cast<time_t>(period.start + offset);
  time_t end = static_cast<time_t>(period.end + offset);
  tm from{}, to{};
  gmtime_r(&start, &from);
  gmtime_r(&end, &to);
  // Languages with no distinct am/pm markers use an unambiguous 24-hour range.
  clock_24h = clock_24h || std::strcmp(s->am, s->pm) == 0;
  const char *start_suffix = clock_24h ? "" : (from.tm_hour < 12 ? s->am : s->pm);
  const char *end_suffix = clock_24h ? "" : (to.tm_hour < 12 ? s->am : s->pm);
  char left[24], right[24];
  format_outlook_hour(left, sizeof(left), from, clock_24h,
                      std::strcmp(start_suffix, end_suffix) == 0 ? "" : start_suffix);
  format_outlook_hour(right, sizeof(right), to, clock_24h, end_suffix);
  std::snprintf(out, size, "%s-%s", left, right);
}

inline OutlookState format_outlook(char *out, size_t size, const WeatherOutlook &weather,
                                   int64_t clock, uint32_t uptime, bool fahrenheit,
                                   bool clock_24h, Language language) {
  const LocalizedStrings *s = get_strings(language);
  if (!weather.available) {
    std::snprintf(out, size, "%s", s->outlook_unavailable);
    return OutlookState::unavailable;
  }
  const int64_t now = forecast_now(weather, clock, uptime);
  if (weather_stale(weather, now, uptime)) {
    std::snprintf(out, size, "%s", s->outlook_stale);
    return OutlookState::stale;
  }
  bool has_future = false;
  for (const OutlookHour &hour : weather.hours) {
    if (hour.end <= now) continue;
    has_future = true;
    if (hour.start > now + 8 * 3600 || hour.end <= hour.start || !valid_probability(hour.probability)) {
      std::snprintf(out, size, "%s", s->outlook_unavailable);
      return OutlookState::unavailable;
    }
    if (hour.probability < rain_probability_threshold) continue;
    char period[52];
    format_outlook_period(period, sizeof(period), hour, weather.utc_offset_seconds, clock_24h, s);
    // Montserrat's fallback includes this bullet; the range uses an ASCII hyphen.
    std::snprintf(out, size, "%s %s • %.0f%%", precipitation_name(hour.weather_code, s),
                  period, hour.probability);
    return OutlookState::precipitation;
  }
  if (!has_future) {
    std::snprintf(out, size, "%s", s->outlook_unavailable);
    return OutlookState::unavailable;
  }
  const float feels = fahrenheit ? weather.feels_like_c * 9 / 5 + 32 : weather.feels_like_c;
  std::snprintf(out, size, "%s %.0f°%c", s->feels_like_temp, feels, fahrenheit ? 'F' : 'C');
  return OutlookState::feels_like;
}

}  // namespace aura

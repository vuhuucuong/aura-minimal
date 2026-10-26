#pragma once

#include <ArduinoJson.h>
#include "weather_outlook.h"

namespace aura {

inline bool json_temperature(JsonVariantConst value) {
  return value.is<float>() && std::isfinite(value.as<float>());
}

// Publish a complete snapshot atomically. Optional probabilities may be missing;
// missing readings must never become a reassuring 0% through JSON conversion.
inline bool parse_weather_response(JsonVariantConst doc, uint32_t received_ms,
                                   WeatherOutlook &result) {
  if (!json_temperature(doc["current"]["temperature_2m"])
      || !json_temperature(doc["current"]["apparent_temperature"])
      || !doc["current"]["time"].is<int64_t>()
      || !doc["current"]["weather_code"].is<int>()
      || !doc["current"]["is_day"].is<int>()
      || !doc["utc_offset_seconds"].is<int>()) return false;
  const char *daily_fields[] = {"time", "temperature_2m_min", "temperature_2m_max", "weather_code"};
  const char *hourly_fields[] = {"time", "temperature_2m", "precipitation_probability", "weather_code", "is_day"};
  for (const char *key : daily_fields)
    if (doc["daily"][key].size() < forecast_rows) return false;
  for (const char *key : hourly_fields)
    if (doc["hourly"][key].size() < forecast_rows + 1) return false;

  WeatherOutlook next;
  next.valid_at = doc["current"]["time"].as<int64_t>();
  next.utc_offset_seconds = doc["utc_offset_seconds"].as<int>();
  if (next.valid_at < earliest_weather_time || next.utc_offset_seconds < -43200
      || next.utc_offset_seconds > 50400) return false;
  next.temperature_c = doc["current"]["temperature_2m"].as<float>();
  next.feels_like_c = doc["current"]["apparent_temperature"].as<float>();
  next.weather_code = doc["current"]["weather_code"].as<int>();
  next.is_day = doc["current"]["is_day"].as<int>();
  next.received_ms = received_ms;
  for (int i = 0; i <= forecast_rows; ++i) {
    if (!doc["hourly"]["time"][i].is<int64_t>()) return false;
    const int64_t timestamp = doc["hourly"]["time"][i].as<int64_t>();
    if (timestamp < earliest_weather_time) return false;
    if (i && timestamp != doc["hourly"]["time"][i - 1].as<int64_t>() + 3600) return false;
  }
  const int64_t first_hour = doc["hourly"]["time"][0].as<int64_t>();
  if (next.valid_at < first_hour || next.valid_at >= first_hour + 3600) return false;
  for (int i = 0; i < forecast_rows; ++i) {
    if (!doc["daily"]["time"][i].is<int64_t>()
        || doc["daily"]["time"][i].as<int64_t>() < earliest_weather_time
        || !json_temperature(doc["daily"]["temperature_2m_min"][i])
        || !json_temperature(doc["daily"]["temperature_2m_max"][i])
        || !doc["daily"]["weather_code"][i].is<int>()
        || !json_temperature(doc["hourly"]["temperature_2m"][i])) return false;
    ForecastDay &day = next.days[i];
    day.time = doc["daily"]["time"][i].as<int64_t>();
    if (i && day.time <= next.days[i - 1].time) return false;
    day.low_c = doc["daily"]["temperature_2m_min"][i].as<float>();
    day.high_c = doc["daily"]["temperature_2m_max"][i].as<float>();
    day.weather_code = doc["daily"]["weather_code"][i].as<int>();
    OutlookHour &hour = next.hours[i];
    hour.start = doc["hourly"]["time"][i].as<int64_t>();
    hour.end = doc["hourly"]["time"][i + 1].as<int64_t>();
    hour.temperature_c = doc["hourly"]["temperature_2m"][i].as<float>();
    // Open-Meteo's probability at i+1 describes the hour beginning at i.
    JsonVariantConst probability = doc["hourly"]["precipitation_probability"][i + 1];
    if (probability.is<float>() && valid_probability(probability.as<float>()))
      hour.probability = probability.as<float>();
    if (doc["hourly"]["weather_code"][i + 1].is<int>())
      hour.weather_code = doc["hourly"]["weather_code"][i + 1].as<int>();
    hour.is_day = doc["hourly"]["is_day"][i].as<int>();
  }
  next.available = true;
  result = next;
  return true;
}

}  // namespace aura

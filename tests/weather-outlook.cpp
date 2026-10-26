#include <cassert>
#include <iostream>
#include <limits>
#include <string>
#include "weather_outlook.h"
#ifdef AURA_TEST_RESPONSE
#include "weather_response.h"
#endif

using namespace aura;
constexpr int64_t afternoon = 1793019600;  // 2026-10-26 13:00 UTC

WeatherOutlook sample() {
  WeatherOutlook w;
  w.available = true;
  w.valid_at = afternoon + 15 * 60;
  w.received_ms = 1000;
  w.feels_like_c = 24;
  for (int i = 0; i < forecast_rows; ++i) {
    w.hours[i].start = afternoon + i * 3600;
    w.hours[i].end = afternoon + (i + 1) * 3600;
    w.hours[i].probability = 10;
    w.hours[i].weather_code = 2;
  }
  return w;
}

std::string summary(const WeatherOutlook &w, int64_t now = afternoon + 20 * 60,
                    uint32_t uptime = 1000, bool f = false, bool h24 = false,
                    Language language = LANG_EN) {
  char text[192];
  format_outlook(text, sizeof(text), w, now, uptime, f, h24, language);
  return text;
}

void check(bool condition, const char *message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::abort();
  }
}

int main() {
  auto w = sample();
  check(summary(w) == "Feels Like 24°C", "low risk keeps feels-like");
  check(summary(w, w.valid_at, 1000, true) == "Feels Like 75°F", "Fahrenheit conversion");
  check(summary(WeatherOutlook{}) == "Forecast unavailable", "startup without data");
  w.hours[2].probability = 70;
  w.hours[2].weather_code = 61;
  check(summary(w) == "Rain 3-4pm • 70%", "rain range and probability");
  check(summary(w, w.valid_at, 1000, false, true) == "Rain 15-16 • 70%", "24-hour format");
  w.hours[4].probability = 99;
  check(summary(w) == "Rain 3-4pm • 70%", "earliest likely period wins, not largest chance");
  w.hours[2].probability = 59;
  check(summary(w).find("5-6pm") != std::string::npos, "threshold does not round up 59%");
  w.hours[2].probability = 60;
  check(summary(w).find("3-4pm") != std::string::npos, "60% reaches threshold");
  for (int code : {71, 73, 75, 77, 85, 86}) {
    w.hours[2].weather_code = code;
    check(summary(w) == "Snow 3-4pm • 60%", "snow types are distinguished from rain");
  }
  w.hours[2].weather_code = -1;
  check(summary(w) == "Precip. 3-4pm • 60%", "unknown type is not called rain");
  for (float invalid : {NAN, -1.0f, 101.0f, std::numeric_limits<float>::infinity()}) {
    auto missing = sample();
    missing.hours[0].probability = invalid;
    check(summary(missing) == "Forecast unavailable", "unknown chance must not become a dry forecast");
  }
  check(summary(w, w.valid_at + 1800) == "Weather outdated", "validity timestamp ages out");
  check(summary(w, w.valid_at, 1000 + stale_after_ms) == "Weather outdated", "monotonic freshness independent of clock");
  check(summary(w, 0, 1000 + stale_after_ms) == "Weather outdated", "stale without NTP");
  w.received_ms = UINT32_MAX - 500;
  check(summary(w, 0, 499).find("3-4pm") != std::string::npos, "millis wrap does not expire fresh data");
  check(summary(w, 0, stale_after_ms - 501) == "Weather outdated", "millis wrap still ages data");

  auto expired = sample();
  expired.valid_at = afternoon + 3 * 3600;
  expired.hours[0].probability = 100;
  check(summary(expired, expired.valid_at) == "Feels Like 24°C", "expired wet periods are excluded");
  expired.valid_at = afternoon + 10 * 3600;
  check(summary(expired, expired.valid_at) == "Forecast unavailable", "empty future coverage is not a dry forecast");

  OutlookHour boundary;
  boundary.start = afternoon - 2 * 3600;
  boundary.end = boundary.start + 3600;
  char range[64];
  format_outlook_period(range, sizeof(range), boundary, 0, false, get_strings(LANG_EN));
  check(std::string(range) == "11am-12pm", "noon crossing retains both suffixes");
  boundary.start = afternoon + 10 * 3600;
  boundary.end = boundary.start + 3600;
  format_outlook_period(range, sizeof(range), boundary, 0, false, get_strings(LANG_EN));
  check(std::string(range) == "11pm-12am", "midnight crossing");
  boundary.start = afternoon;
  boundary.end = boundary.start + 3600;
  format_outlook_period(range, sizeof(range), boundary, 19800, true, get_strings(LANG_EN));
  check(std::string(range) == "18:30-19:30", "fractional timezone offset preserves minutes");
  format_outlook_period(range, sizeof(range), boundary, -18000, false, get_strings(LANG_EN));
  check(std::string(range) == "8-9am", "negative timezone offset");

  w = sample();
  w.hours[2].probability = 70;
  w.hours[2].weather_code = 61;
  const char *names[] = {"Rain", "Lluvia", "Regen", "Pluie", "Yagmur", "Regn", "Pioggia"};
  for (int language = LANG_EN; language <= LANG_IT; ++language) {
    check(summary(w, w.valid_at, 1000, false, false, static_cast<Language>(language)).find(names[language]) == 0,
          "each language translates the outlook");
    check(summary(w, w.valid_at + 1800, 1000, false, false, static_cast<Language>(language))
          == get_strings(static_cast<Language>(language))->outlook_stale, "each language translates staleness");
  }
  check(summary(w, w.valid_at, 1000, false, false, LANG_DE).find("15-16") != std::string::npos,
        "languages without am/pm markers use 24-hour ranges");

#ifdef AURA_TEST_RESPONSE
  JsonDocument doc;
  doc["current"]["time"] = w.valid_at;
  doc["current"]["temperature_2m"] = 23;
  doc["current"]["apparent_temperature"] = 24;
  doc["current"]["weather_code"] = 2;
  doc["current"]["is_day"] = 1;
  doc["utc_offset_seconds"] = 0;
  for (int i = 0; i <= forecast_rows; ++i) {
    doc["hourly"]["time"][i] = afternoon + i * 3600;
    doc["hourly"]["temperature_2m"][i] = 20 + i;
    doc["hourly"]["precipitation_probability"][i] = i == 3 ? 70 : 10;
    doc["hourly"]["weather_code"][i] = i == 3 ? 61 : 2;
    doc["hourly"]["is_day"][i] = 1;
    if (i == forecast_rows) continue;
    doc["daily"]["time"][i] = afternoon - 13 * 3600 + i * 86400;
    doc["daily"]["temperature_2m_min"][i] = 17;
    doc["daily"]["temperature_2m_max"][i] = 24;
    doc["daily"]["weather_code"][i] = 2;
  }
  WeatherOutlook parsed;
  check(parse_weather_response(doc.as<JsonVariantConst>(), 1000, parsed), "complete API snapshot parses");
  check(summary(parsed) == "Rain 3-4pm • 70%", "API probability timestamp describes the preceding hour");
  check(parsed.hours[2].temperature_c == 22, "temperature remains aligned with period start");
  check(parse_weather_response(doc.as<JsonVariantConst>(), 100000, parsed), "repeated response can parse");
  check(summary(parsed, parsed.valid_at + 1800, 100000) == "Weather outdated",
        "repeated frozen API data does not renew its validity timestamp");
  doc["hourly"]["precipitation_probability"][0] = 100;
  check(parse_weather_response(doc.as<JsonVariantConst>(), 1000, parsed), "past probability accepted");
  check(summary(parsed) == "Rain 3-4pm • 70%", "past rain is never used as a future outlook");
  doc["hourly"]["precipitation_probability"][1] = nullptr;
  check(parse_weather_response(doc.as<JsonVariantConst>(), 1000, parsed), "null probability keeps temperature data");
  check(summary(parsed) == "Forecast unavailable", "null probability stays unknown");
  doc["hourly"]["temperature_2m"][2] = nullptr;
  const auto previous = parsed;
  check(!parse_weather_response(doc.as<JsonVariantConst>(), 9000, parsed), "incomplete payload rejected");
  check(parsed.received_ms == previous.received_ms, "failed payload does not renew freshness");
  check(parsed.hours[2].temperature_c == previous.hours[2].temperature_c, "failed payload keeps last readings");
  doc["hourly"]["temperature_2m"][2] = 22;
  doc["hourly"]["time"][5] = afternoon + 4 * 3600;
  check(!parse_weather_response(doc.as<JsonVariantConst>(), 1000, parsed), "duplicated timestamps rejected");
  doc["hourly"]["time"][5] = afternoon + 5 * 3600;
  doc["hourly"]["time"].as<JsonArray>().remove(7);
  check(!parse_weather_response(doc.as<JsonVariantConst>(), 1000, parsed), "short API arrays rejected");
  std::cout << "API parsing and timestamp alignment passed.\n";
#endif
  std::cout << "Outlook, missing data, freshness, rollover, timezone and localization checks passed.\n";
}

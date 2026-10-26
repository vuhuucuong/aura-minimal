// Headless LVGL preview harness. Uses the real firmware layout, fonts and renderer.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "lvgl.h"
#include "weather_outlook.h"
#include "manga_icons.h"

LV_FONT_DECLARE(lv_font_montserrat_latin_12);
LV_FONT_DECLARE(lv_font_montserrat_latin_14);
LV_FONT_DECLARE(lv_font_montserrat_latin_16);
static Language current_language = LANG_EN;
static bool use_fahrenheit = false;
static std::string location = "London";
static lv_obj_t *lbl_today_temp, *lbl_today_feels_like, *img_today_icon, *lbl_forecast;
static lv_obj_t *box_daily, *box_hourly, *lbl_loc, *lbl_clock;
static lv_obj_t *lbl_daily_day[7], *lbl_daily_high[7], *lbl_daily_low[7], *img_daily[7];
static lv_obj_t *lbl_hourly[7], *lbl_precipitation_probability[7], *lbl_hourly_temp[7], *img_hourly[7];
const lv_font_t *get_font_12() { return &lv_font_montserrat_latin_12; }
const lv_font_t *get_font_14() { return &lv_font_montserrat_latin_14; }
const lv_font_t *get_font_16() { return &lv_font_montserrat_latin_16; }
static void screen_event_cb(lv_event_t *) {}
static void settings_event_handler(lv_event_t *) {}
static void change_location_event_cb(lv_event_t *) {}
static void reset_wifi_event_handler(lv_event_t *) {}
void render_cached_weather() {}
#include "minimal_ui.h"
void daily_cb(lv_event_t *) { minimal_show_forecast(false); }
void hourly_cb(lv_event_t *) { minimal_show_forecast(true); }

static unsigned char pixels[240 * 320 * 3];
static void flush(lv_display_t *display, const lv_area_t *area, uint8_t *data) {
  for (int y = area->y1; y <= area->y2; ++y) {
    for (int x = area->x1; x <= area->x2; ++x) {
      const size_t dest = (y * 240 + x) * 3;
      pixels[dest] = data[2];
      pixels[dest + 1] = data[1];
      pixels[dest + 2] = data[0];
      data += 3;
    }
  }
  lv_display_flush_ready(display);
}

int main(int argc, char **argv) {
  if (argc != 5) return 2;
  const std::string view = argv[1], conditions = argv[2], state = argv[3];
  lv_init();
  lv_display_t *display = lv_display_create(240, 320);
  lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB888);
  static uint8_t buffer[240 * 320 * 3];
  lv_display_set_buffers(display, buffer, nullptr, sizeof(buffer), LV_DISPLAY_RENDER_MODE_FULL);
  lv_display_set_flush_cb(display, flush);
  lv_display_set_theme(display, lv_theme_default_init(display, lv_color_black(), lv_color_black(),
                                                      false, get_font_14()));
  create_ui();
  lv_label_set_text(lbl_clock, "13:20");
  lv_label_set_text(lbl_today_temp, "23°C");
  const int code = conditions == "sun" ? 0 : conditions == "rain" ? 61 : conditions == "storm" ? 95 : 2;
  lv_image_set_src(img_today_icon, manga_weather_image(code, 1, true));

  aura::WeatherOutlook weather;
  weather.available = true;
  weather.feels_like_c = 24;
  weather.valid_at = 1793019600 + 15 * 60;
  for (int i = 0; i < 7; ++i) {
    weather.hours[i].start = 1793019600 + i * 3600;
    weather.hours[i].end = weather.hours[i].start + 3600;
    weather.hours[i].probability = state == "clear" ? 10 : (i == 2 ? 70 : 20);
    weather.hours[i].weather_code = state == "snow" ? 73 : 61;
  }
  char summary[192];
  aura::format_outlook(summary, sizeof(summary), weather,
                       weather.valid_at + (state == "stale" ? 1800 : 300), 0,
                       false, false, current_language);
  lv_label_set_text(lbl_today_feels_like, summary);
  const char *days[] = {"Today", "Tues", "Wed", "Thurs", "Fri", "Sat", "Sun"};
  const int icons[] = {2, 0, 61, 95, 2, 0, 0};
  const int lows[] = {17, 18, 17, 18, 19, 17, 18};
  const int highs[] = {24, 25, 21, 22, 24, 26, 25};
  for (int i = 0; i < 7; ++i) {
    lv_label_set_text(lbl_daily_day[i], days[i]);
    lv_label_set_text_fmt(lbl_daily_low[i], "%d°", lows[i]);
    lv_label_set_text_fmt(lbl_daily_high[i], "%d°", highs[i]);
    lv_image_set_src(img_daily[i], manga_weather_image(icons[i], 1, false));
    lv_label_set_text_fmt(lbl_hourly[i], "%dpm", i + 1);
    if (i == 0) lv_label_set_text(lbl_hourly[i], "Now");
    lv_label_set_text_fmt(lbl_precipitation_probability[i], "%.0f%%", weather.hours[i].probability);
    lv_label_set_text_fmt(lbl_hourly_temp[i], "%d°", 23 - i);
    lv_image_set_src(img_hourly[i], manga_weather_image(
        state == "clear" ? 2 : i == 2 ? weather.hours[i].weather_code : 2, 1, false));
  }
  minimal_show_forecast(view == "daily");

  // Exercise the actual tap target, not a duplicate preview implementation.
  if (lv_obj_send_event(lbl_today_feels_like, LV_EVENT_CLICKED, nullptr) != LV_RESULT_OK
      || !lv_obj_has_flag(box_daily, LV_OBJ_FLAG_HIDDEN)
      || lv_obj_has_flag(box_hourly, LV_OBJ_FLAG_HIDDEN)) return 3;
  minimal_show_forecast(view == "daily");

  for (int language = LANG_EN; language <= LANG_IT; ++language) {
    for (bool h24 : {false, true}) {
      for (int offset : {0, 19800}) {
        for (int hour : {11, 15, 23}) {
          for (int weather_code : {2, 61, 73}) {
            auto translated_weather = weather;
            translated_weather.utc_offset_seconds = offset;
            translated_weather.valid_at = 1793019600 + (hour - 13) * 3600;
            for (auto &period : translated_weather.hours) {
              period.start = translated_weather.valid_at;
              period.end = period.start + 3600;
              period.weather_code = weather_code;
              period.probability = 100;
            }
            char translated[192];
            aura::format_outlook(translated, sizeof(translated), translated_weather,
                                  translated_weather.valid_at, 0, false, h24,
                                  static_cast<Language>(language));
            if (lv_text_get_width(translated, std::strlen(translated), get_font_12(), 0) > 216) {
              std::fprintf(stderr, "Outlook does not fit: %s\n", translated);
              return 4;
            }
          }
        }
      }
    }
  }
  lv_refr_now(display);
  FILE *file = std::fopen(argv[4], "wb");
  if (!file) return 5;
  std::fprintf(file, "P6\n240 320\n255\n");
  std::fwrite(pixels, 1, sizeof(pixels), file);
  std::fclose(file);
}

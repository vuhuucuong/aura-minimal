#pragma once
#include <lvgl.h>
#ifdef __cplusplus
extern "C" {
#endif
extern const lv_image_dsc_t manga_sun_20;
extern const lv_image_dsc_t manga_sun_48;
extern const lv_image_dsc_t manga_moon_20;
extern const lv_image_dsc_t manga_moon_48;
extern const lv_image_dsc_t manga_partly_20;
extern const lv_image_dsc_t manga_partly_48;
extern const lv_image_dsc_t manga_nightcloud_20;
extern const lv_image_dsc_t manga_nightcloud_48;
extern const lv_image_dsc_t manga_cloud_20;
extern const lv_image_dsc_t manga_cloud_48;
extern const lv_image_dsc_t manga_rain_20;
extern const lv_image_dsc_t manga_rain_48;
extern const lv_image_dsc_t manga_storm_20;
extern const lv_image_dsc_t manga_storm_48;
extern const lv_image_dsc_t manga_snow_20;
extern const lv_image_dsc_t manga_snow_48;
extern const lv_image_dsc_t manga_fog_20;
extern const lv_image_dsc_t manga_fog_48;
#ifdef __cplusplus
}
#endif

static inline const lv_image_dsc_t *manga_weather_image(int code, int day, bool large) {
#define MANGA_ICON(kind) (large ? &manga_##kind##_48 : &manga_##kind##_20)
  if (code == 0) return day ? MANGA_ICON(sun) : MANGA_ICON(moon);
  if (code == 1 || code == 2) return day ? MANGA_ICON(partly) : MANGA_ICON(nightcloud);
  if (code == 45 || code == 48) return MANGA_ICON(fog);
  if (code >= 95) return MANGA_ICON(storm);
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return MANGA_ICON(snow);
  if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) return MANGA_ICON(rain);
  return MANGA_ICON(cloud);
#undef MANGA_ICON
}

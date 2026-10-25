// Aura Minimal 1.0.0 — modified 2026-10-26.
// Based on Surrey-Homeware/Aura, commit ded60e3cd5c9fc41e2f049ff1da70c26820ee9e9.
// Application changes are licensed under GPL-3.0-only; see LICENSE.
// Hardware, location, weather provider, languages and saved preference keys follow Aura.
#include <Arduino.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <lvgl.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <Preferences.h>
#include "esp_system.h"
#include "translations.h"
#include "manga_icons.h"
#ifndef AURA_INVERT_COLORS
#define AURA_INVERT_COLORS 0
#endif

#define XPT2046_IRQ 36   // T_IRQ
#define XPT2046_MOSI 32  // T_DIN
#define XPT2046_MISO 39  // T_OUT
#define XPT2046_CLK 25   // T_CLK
#define XPT2046_CS 33    // T_CS
#define LCD_BACKLIGHT_PIN 21
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320
#define DRAW_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 10 * (LV_COLOR_DEPTH / 8))

#define LATITUDE_DEFAULT "51.5074"
#define LONGITUDE_DEFAULT "-0.1278"
#define LOCATION_DEFAULT "London"
#define DEFAULT_CAPTIVE_SSID "Aura"
#define UPDATE_INTERVAL 600000UL  // 10 minutes

// Night mode starts at 10pm and ends at 6am
#define NIGHT_MODE_START_HOUR 22
#define NIGHT_MODE_END_HOUR 6

LV_FONT_DECLARE(lv_font_montserrat_latin_12);
LV_FONT_DECLARE(lv_font_montserrat_latin_14);
LV_FONT_DECLARE(lv_font_montserrat_latin_16);
LV_FONT_DECLARE(lv_font_montserrat_latin_20);
LV_FONT_DECLARE(lv_font_montserrat_latin_42);

static Language current_language = LANG_EN;

// Font selection based on language
const lv_font_t* get_font_12() {
  return &lv_font_montserrat_latin_12;
}

const lv_font_t* get_font_14() {
  return &lv_font_montserrat_latin_14;
}

const lv_font_t* get_font_16() {
  return &lv_font_montserrat_latin_16;
}

const lv_font_t* get_font_20() {
  return &lv_font_montserrat_latin_20;
}

const lv_font_t* get_font_42() {
  return &lv_font_montserrat_latin_42;
}

SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);
uint32_t draw_buf[DRAW_BUF_SIZE / 4];
int x, y, z;

// Preferences
static Preferences prefs;
static bool use_fahrenheit = false;
static bool use_24_hour = false; 
static bool use_night_mode = false;
static char latitude[16] = LATITUDE_DEFAULT;
static char longitude[16] = LONGITUDE_DEFAULT;
static String location = String(LOCATION_DEFAULT);
static char dd_opts[512];
static DynamicJsonDocument geoDoc(8 * 1024);
static JsonArray geoResults;

// Screen dimming variables
static bool night_mode_active = false;
static bool temp_screen_wakeup_active = false;
static lv_timer_t *temp_screen_wakeup_timer = nullptr;

// UI components
static lv_obj_t *lbl_today_temp;
static lv_obj_t *lbl_today_feels_like;
static lv_obj_t *img_today_icon;
static lv_obj_t *lbl_forecast;
static lv_obj_t *box_daily;
static lv_obj_t *box_hourly;
static lv_obj_t *lbl_daily_day[7];
static lv_obj_t *lbl_daily_high[7];
static lv_obj_t *lbl_daily_low[7];
static lv_obj_t *img_daily[7];
static lv_obj_t *lbl_hourly[7];
static lv_obj_t *lbl_precipitation_probability[7];
static lv_obj_t *lbl_hourly_temp[7];
static lv_obj_t *img_hourly[7];
static lv_obj_t *lbl_loc;
static lv_obj_t *loc_ta;
static lv_obj_t *results_dd;
static lv_obj_t *btn_close_loc;
static lv_obj_t *btn_close_obj;
static lv_obj_t *kb;
static lv_obj_t *settings_win;
static lv_obj_t *location_win = nullptr;
static lv_obj_t *unit_switch;
static lv_obj_t *clock_24hr_switch;
static lv_obj_t *night_mode_switch;
static lv_obj_t *language_dropdown;
static lv_obj_t *lbl_clock;

void create_ui();
void fetch_and_update_weather();
void create_settings_window();
static void screen_event_cb(lv_event_t *e);
static void settings_event_handler(lv_event_t *e);
const lv_image_dsc_t *choose_icon(int wmo_code, int is_day);
const lv_image_dsc_t *choose_image(int wmo_code, int is_day);

// Screen dimming functions
bool night_mode_should_be_active();
void activate_night_mode();
void deactivate_night_mode();
void check_for_night_mode();
void handle_temp_screen_wakeup_timeout(lv_timer_t *timer);


int day_of_week(int y, int m, int d) {
  static const int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
  if (m < 3) y -= 1;
  return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

String hour_of_day(int hour) {
  const LocalizedStrings* strings = get_strings(current_language);
  if(hour < 0 || hour > 23) return String(strings->invalid_hour);

  if (use_24_hour) {
    if (hour < 10)
      return String("0") + String(hour);
    else
      return String(hour);
  } else {
    if(hour == 0)   return String("12") + strings->am;
    if(hour == 12)  return String(strings->noon);

    bool isMorning = (hour < 12);
    String suffix = isMorning ? strings->am : strings->pm;

    int displayHour = hour % 12;

    return String(displayHour) + suffix;
  }
}

String urlencode(const String &str) {
  String encoded = "";
  char buf[5];
  for (size_t i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    // Unreserved characters according to RFC 3986
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else {
      // Percent-encode others
      sprintf(buf, "%%%02X", (unsigned char)c);
      encoded += buf;
    }
  }
  return encoded;
}

static void update_clock(lv_timer_t *timer) {
  struct tm timeinfo;

  check_for_night_mode();

  if (!getLocalTime(&timeinfo, 0)) return;

  const LocalizedStrings* strings = get_strings(current_language);
  char buf[16];
  if (use_24_hour) {
    snprintf(buf, sizeof(buf), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  } else {
    int hour = timeinfo.tm_hour % 12;
    if(hour == 0) hour = 12;
    const char *ampm = (timeinfo.tm_hour < 12) ? strings->am : strings->pm;
    snprintf(buf, sizeof(buf), "%d:%02d%s", hour, timeinfo.tm_min, ampm);
  }
  lv_label_set_text(lbl_clock, buf);
}

static void ta_event_cb(lv_event_t *e) {
  lv_obj_t *ta = (lv_obj_t *)lv_event_get_target(e);
  lv_obj_t *kb = (lv_obj_t *)lv_event_get_user_data(e);

  // Show keyboard
  lv_keyboard_set_textarea(kb, ta);
  lv_obj_move_foreground(kb);
  lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
}

static void kb_event_cb(lv_event_t *e) {
  lv_obj_t *kb = static_cast<lv_obj_t *>(lv_event_get_target(e));
  lv_obj_add_flag((lv_obj_t *)lv_event_get_target(e), LV_OBJ_FLAG_HIDDEN);

  if (lv_event_get_code(e) == LV_EVENT_READY) {
    const char *loc = lv_textarea_get_text(loc_ta);
    if (strlen(loc) > 0) {
      do_geocode_query(loc);
    }
  }
}

static void ta_defocus_cb(lv_event_t *e) {
  lv_obj_add_flag((lv_obj_t *)lv_event_get_user_data(e), LV_OBJ_FLAG_HIDDEN);
}

void touchscreen_read(lv_indev_t *indev, lv_indev_data_t *data) {
  if (touchscreen.tirqTouched() && touchscreen.touched()) {
    TS_Point p = touchscreen.getPoint();

    x = map(p.x, 200, 3700, 1, SCREEN_WIDTH);
    y = map(p.y, 240, 3800, 1, SCREEN_HEIGHT);
    z = p.z;

    // Handle touch during dimmed screen
    if (night_mode_active) {
      // Temporarily wake the screen for 15 seconds
      analogWrite(LCD_BACKLIGHT_PIN, prefs.getUInt("brightness", 128));
    
      if (temp_screen_wakeup_timer) {
        lv_timer_del(temp_screen_wakeup_timer);
      }
      temp_screen_wakeup_timer = lv_timer_create(handle_temp_screen_wakeup_timeout, 15000, NULL);
      lv_timer_set_repeat_count(temp_screen_wakeup_timer, 1); // Run only once
      Serial.println("Woke up screen. Setting timer to turn of screen after 15 seconds of inactivity.");

      if (!temp_screen_wakeup_active) {
          // If this is the wake-up tap, don't pass this touch to the UI - just undim the screen
          temp_screen_wakeup_active = true;
          data->state = LV_INDEV_STATE_RELEASED;
          return;
      }

      temp_screen_wakeup_active = true;
    }

    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = x;
    data->point.y = y;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  TFT_eSPI tft = TFT_eSPI();
  tft.init();
  pinMode(LCD_BACKLIGHT_PIN, OUTPUT);

  lv_init();
  lv_tick_set_cb([]() -> uint32_t { return millis(); });

  // Init touchscreen
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(0);

  lv_display_t *disp = lv_tft_espi_create(SCREEN_WIDTH, SCREEN_HEIGHT, draw_buf, sizeof(draw_buf));
#if AURA_INVERT_COLORS
  tft.invertDisplay(true);
#endif
  lv_display_set_theme(disp, lv_theme_default_init(disp, lv_color_black(),
      lv_color_black(), false, get_font_14()));
  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touchscreen_read);

  // Load saved prefs
  prefs.begin("weather", false);
  String lat = prefs.getString("latitude", LATITUDE_DEFAULT);
  lat.toCharArray(latitude, sizeof(latitude));
  String lon = prefs.getString("longitude", LONGITUDE_DEFAULT);
  lon.toCharArray(longitude, sizeof(longitude));
  use_fahrenheit = prefs.getBool("useFahrenheit", false);
  location = prefs.getString("location", LOCATION_DEFAULT);
  use_night_mode = prefs.getBool("useNightMode", false);
  uint32_t brightness = prefs.getUInt("brightness", 255);
  use_24_hour = prefs.getBool("use24Hour", false);
  current_language = (Language)prefs.getUInt("language", LANG_EN);
  if (current_language > LANG_IT) current_language = LANG_EN;
  analogWrite(LCD_BACKLIGHT_PIN, brightness);

  // Check for Wi-Fi config and request it if not available
  WiFiManager wm;
  wm.setAPCallback(apModeCallback);
  wm.autoConnect(DEFAULT_CAPTIVE_SSID);

  lv_timer_create(update_clock, 1000, NULL);

  lv_obj_clean(lv_scr_act());
  create_ui();
  fetch_and_update_weather();
}

void flush_wifi_splashscreen(uint32_t ms = 200) {
  uint32_t start = millis();
  while (millis() - start < ms) {
    lv_timer_handler();
    delay(5);
  }
}

void apModeCallback(WiFiManager *mgr) {
  wifi_splash_screen();
  flush_wifi_splashscreen();
}

void loop() {
  lv_timer_handler();
  static uint32_t last = millis();

  if (millis() - last >= UPDATE_INTERVAL) {
    fetch_and_update_weather();
    last = millis();
  }

  delay(5);
}

#include "minimal_ui.h"

void wifi_splash_screen() {
  lv_obj_t *scr = lv_scr_act();
  lv_obj_clean(scr);
  minimal_surface(scr);
  const LocalizedStrings *strings = get_strings(current_language);
  lv_obj_t *lbl = minimal_label(scr, strings->wifi_config, get_font_16(), 12, 40, 216);
  lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(lbl);
}

void populate_results_dropdown() {
  dd_opts[0] = '\0';
  for (JsonObject item : geoResults) {
    const char *name = item["name"] | "";
    const char *admin = item["admin1"] | "";
    char row[128];
    snprintf(row, sizeof(row), "%s%s%s", name, *admin ? ", " : "", admin);
    // Stop at a whole entry so dropdown indices continue to match geoResults.
    size_t used = strlen(dd_opts), length = strlen(row);
    if (used + length + (used ? 1 : 0) >= sizeof(dd_opts)) break;
    if (used) strcat(dd_opts, "\n");
    strcat(dd_opts, row);
  }
  if (dd_opts[0]) {
    lv_dropdown_set_options(results_dd, dd_opts);
    lv_obj_add_flag(results_dd, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(btn_close_loc, lv_color_black(), 0);
    lv_obj_set_style_text_color(btn_close_loc, lv_color_white(), 0);
    lv_obj_add_flag(btn_close_loc, LV_OBJ_FLAG_CLICKABLE);
  }
}

static void location_save_event_cb(lv_event_t *e) {
  JsonArray *pres = static_cast<JsonArray *>(lv_event_get_user_data(e));
  uint16_t idx = lv_dropdown_get_selected(results_dd);

  JsonObject obj = (*pres)[idx];
  double lat = obj["latitude"].as<double>();
  double lon = obj["longitude"].as<double>();

  snprintf(latitude, sizeof(latitude), "%.6f", lat);
  snprintf(longitude, sizeof(longitude), "%.6f", lon);
  prefs.putString("latitude", latitude);
  prefs.putString("longitude", longitude);

  String opts;
  const char *name = obj["name"];
  const char *admin = obj["admin1"];
  const char *country = obj["country_code"];
  opts += name;
  if (admin) {
    opts += ", ";
    opts += admin;
  }

  prefs.putString("location", opts);
  location = prefs.getString("location");

  // Re‐fetch weather immediately
  lv_label_set_text(lbl_loc, opts.c_str());
  lv_label_set_text(lbl_home_location, opts.c_str());
  fetch_and_update_weather();

  lv_obj_del(location_win);
  location_win = nullptr;
}

static void location_cancel_event_cb(lv_event_t *e) {
  lv_obj_del(location_win);
  location_win = nullptr;
}

void screen_event_cb(lv_event_t *e) {
  create_settings_window();
}

void daily_cb(lv_event_t *e) { minimal_show_forecast(true); }
void hourly_cb(lv_event_t *e) { minimal_show_forecast(false); }



static void reset_wifi_event_handler(lv_event_t *e) {
  const LocalizedStrings* strings = get_strings(current_language);
  lv_obj_t *mbox = lv_msgbox_create(lv_scr_act());
  lv_obj_t *title = lv_msgbox_add_title(mbox, strings->reset);
  lv_obj_set_style_margin_left(title, 10, 0);
  lv_obj_set_style_text_font(title, get_font_16(), 0);

  lv_obj_t *text = lv_msgbox_add_text(mbox, strings->reset_confirmation);
  lv_obj_set_style_text_font(text, get_font_12(), 0);
  lv_msgbox_add_close_button(mbox);

  lv_obj_t *btn_no = lv_msgbox_add_footer_button(mbox, strings->cancel);
  lv_obj_set_style_text_font(btn_no, get_font_12(), 0);
  lv_obj_t *btn_yes = lv_msgbox_add_footer_button(mbox, strings->reset);
  lv_obj_set_style_text_font(btn_yes, get_font_12(), 0);

  lv_obj_set_style_bg_color(btn_yes, lv_color_black(), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(btn_yes, lv_color_hex(0x303030), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_text_color(btn_yes, lv_color_white(), LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_set_width(mbox, 230);
  lv_obj_center(mbox);

  lv_obj_set_style_border_width(mbox, 2, LV_PART_MAIN);
  lv_obj_set_style_border_color(mbox, lv_color_black(), LV_PART_MAIN);
  lv_obj_set_style_border_opa(mbox, LV_OPA_COVER,   LV_PART_MAIN);
  lv_obj_set_style_radius(mbox, 4, LV_PART_MAIN);

  lv_obj_add_event_cb(btn_yes, reset_confirm_yes_cb, LV_EVENT_CLICKED, mbox);
  lv_obj_add_event_cb(btn_no, reset_confirm_no_cb, LV_EVENT_CLICKED, mbox);
}

static void reset_confirm_yes_cb(lv_event_t *e) {
  lv_obj_t *mbox = (lv_obj_t *)lv_event_get_user_data(e);
  Serial.println("Clearing Wi-Fi creds and rebooting");
  WiFiManager wm;
  wm.resetSettings();
  delay(100);
  esp_restart();
}

static void reset_confirm_no_cb(lv_event_t *e) {
  lv_obj_t *mbox = (lv_obj_t *)lv_event_get_user_data(e);
  lv_obj_del(mbox);
}

static void change_location_event_cb(lv_event_t *e) {
  if (location_win) return;

  create_location_dialog();
}

void create_location_dialog() {
  const LocalizedStrings* strings = get_strings(current_language);
  location_win = lv_win_create(lv_scr_act());
  lv_obj_t *title = lv_win_add_title(location_win, strings->change_location);
  lv_obj_t *header = lv_win_get_header(location_win);
  lv_obj_set_style_height(header, 30, 0);
  lv_obj_set_style_text_font(title, get_font_16(), 0);
  lv_obj_set_style_margin_left(title, 10, 0);
  lv_obj_set_size(location_win, 240, 320);
  lv_obj_center(location_win);

  lv_obj_t *cont = lv_win_get_content(location_win);

  lv_obj_t *lbl = lv_label_create(cont);
  lv_label_set_text(lbl, strings->city);
  lv_obj_set_style_text_font(lbl, get_font_14(), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 5, 10);

  loc_ta = lv_textarea_create(cont);
  lv_textarea_set_one_line(loc_ta, true);
  lv_textarea_set_placeholder_text(loc_ta, strings->city_placeholder);
  lv_obj_set_width(loc_ta, 170);
  lv_obj_align_to(loc_ta, lbl, LV_ALIGN_OUT_RIGHT_MID, 5, 0);

  lv_obj_add_event_cb(loc_ta, ta_event_cb, LV_EVENT_CLICKED, kb);
  lv_obj_add_event_cb(loc_ta, ta_defocus_cb, LV_EVENT_DEFOCUSED, kb);

  lv_obj_t *lbl2 = lv_label_create(cont);
  lv_label_set_text(lbl2, strings->search_results);
  lv_obj_set_style_text_font(lbl2, get_font_14(), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_align(lbl2, LV_ALIGN_TOP_LEFT, 5, 50);

  results_dd = lv_dropdown_create(cont);
  lv_obj_set_width(results_dd, 200);
  lv_obj_align(results_dd, LV_ALIGN_TOP_LEFT, 5, 70);
  lv_obj_set_style_text_font(results_dd, get_font_14(), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_font(results_dd, get_font_14(), LV_PART_SELECTED | LV_STATE_DEFAULT);

  lv_obj_t *list = lv_dropdown_get_list(results_dd);
  lv_obj_set_style_text_font(list, get_font_14(), LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_dropdown_set_options(results_dd, "");
  lv_obj_clear_flag(results_dd, LV_OBJ_FLAG_CLICKABLE);

  btn_close_loc = lv_btn_create(cont);
  lv_obj_set_size(btn_close_loc, 80, 40);
  lv_obj_align(btn_close_loc, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

  lv_obj_add_event_cb(btn_close_loc, location_save_event_cb, LV_EVENT_CLICKED, &geoResults);
  lv_obj_set_style_bg_color(btn_close_loc, lv_color_hex(0xB0B0B0), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(btn_close_loc, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(btn_close_loc, lv_color_hex(0x808080), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_clear_flag(btn_close_loc, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t *lbl_close = lv_label_create(btn_close_loc);
  lv_label_set_text(lbl_close, strings->save);
  lv_obj_set_style_text_font(lbl_close, get_font_14(), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_center(lbl_close);

  lv_obj_t *btn_cancel_loc = lv_btn_create(cont);
  lv_obj_set_size(btn_cancel_loc, 80, 40);
  lv_obj_align_to(btn_cancel_loc, btn_close_loc, LV_ALIGN_OUT_LEFT_MID, -5, 0);
  lv_obj_add_event_cb(btn_cancel_loc, location_cancel_event_cb, LV_EVENT_CLICKED, &geoResults);

  lv_obj_t *lbl_cancel = lv_label_create(btn_cancel_loc);
  lv_label_set_text(lbl_cancel, strings->cancel);
  lv_obj_set_style_text_font(lbl_cancel, get_font_14(), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_center(lbl_cancel);
}

void create_settings_window() {
  if (settings_win) return;
  const LocalizedStrings *s = get_strings(current_language);
  settings_win = lv_win_create(lv_scr_act());
  lv_obj_set_size(settings_win, 240, 320);
  lv_obj_center(settings_win);
  lv_obj_t *header = lv_win_get_header(settings_win);
  lv_obj_set_height(header, 40);
  lv_obj_set_style_bg_color(header, lv_color_white(), 0);
  lv_obj_set_style_border_color(header, lv_color_black(), 0);
  lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_width(header, 2, 0);
  lv_obj_t *title = lv_win_add_title(settings_win, s->aura_settings);
  lv_obj_set_style_text_font(title, get_font_16(), 0);
  btn_close_obj = lv_win_add_button(settings_win, LV_SYMBOL_CLOSE, 40);
  minimal_button_style(btn_close_obj, false);
  lv_obj_add_event_cb(btn_close_obj, settings_event_handler, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *cont = lv_win_get_content(settings_win);
  lv_obj_set_style_bg_color(cont, lv_color_white(), 0);
  lv_obj_set_style_pad_all(cont, 12, 0);
  lv_obj_set_scroll_dir(cont, LV_DIR_VER);

  minimal_label(cont, s->brightness, get_font_16(), 0, 0, 200);
  lv_obj_t *slider = lv_slider_create(cont);
  lv_obj_set_pos(slider, 8, 34);
  lv_obj_set_size(slider, 188, 10);
  lv_obj_set_ext_click_area(slider, 12);
  lv_slider_set_range(slider, 1, 255);
  lv_slider_set_value(slider, prefs.getUInt("brightness", 255), LV_ANIM_OFF);
  lv_obj_set_style_bg_color(slider, lv_color_hex(0xD0D0D0), LV_PART_MAIN);
  lv_obj_set_style_bg_color(slider, lv_color_black(), LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(slider, lv_color_black(), LV_PART_KNOB);
  lv_obj_add_event_cb(slider, [](lv_event_t *e) {
    uint32_t value = lv_slider_get_value((lv_obj_t *)lv_event_get_target(e));
    analogWrite(LCD_BACKLIGHT_PIN, value);
    prefs.putUInt("brightness", value);
  }, LV_EVENT_VALUE_CHANGED, nullptr);
  night_mode_switch = minimal_switch(cont, s->use_night_mode, 62, use_night_mode);
  unit_switch = minimal_switch(cont, s->use_fahrenheit, 108, use_fahrenheit);
  clock_24hr_switch = minimal_switch(cont, s->use_24hr, 154, use_24_hour);
  minimal_label(cont, s->language_label, get_font_14(), 0, 210, 90);
  language_dropdown = lv_dropdown_create(cont);
  lv_obj_set_pos(language_dropdown, 90, 198);
  lv_obj_set_size(language_dropdown, 114, 40);
  lv_obj_set_style_text_font(language_dropdown, get_font_14(), 0);
  lv_dropdown_set_options(language_dropdown, "English\nEspañol\nDeutsch\nFrançais\nTürkçe\nSvenska\nItaliano");
  lv_dropdown_set_selected(language_dropdown, current_language);
  lv_obj_set_style_text_font(lv_dropdown_get_list(language_dropdown), get_font_14(), 0);
  lv_obj_add_event_cb(language_dropdown, settings_event_handler, LV_EVENT_VALUE_CHANGED, nullptr);
  lbl_loc = minimal_label(cont, location.c_str(), get_font_14(), 0, 252, 204);
  lv_label_set_long_mode(lbl_loc, LV_LABEL_LONG_DOT);
  lv_obj_set_height(lbl_loc, 20);
  lv_obj_t *loc_btn = minimal_button(cont, s->change_location, 0, 280, 204, 44);
  lv_obj_add_event_cb(loc_btn, change_location_event_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *reset_btn = minimal_button(cont, s->reset_wifi, 0, 336, 204, 44);
  lv_obj_add_event_cb(reset_btn, reset_wifi_event_handler, LV_EVENT_CLICKED, nullptr);
  if (!kb) {
    // Keep the keyboard outside the screen so language rebuilds cannot leave a stale pointer.
    kb = lv_keyboard_create(lv_layer_top());
    lv_obj_set_size(kb, 240, 144);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_READY, nullptr);
    lv_obj_add_event_cb(kb, kb_event_cb, LV_EVENT_CANCEL, nullptr);
  }
}

static void settings_event_handler(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *tgt = (lv_obj_t *)lv_event_get_target(e);

  if (tgt == unit_switch && code == LV_EVENT_VALUE_CHANGED) {
    use_fahrenheit = lv_obj_has_state(unit_switch, LV_STATE_CHECKED);
  }

  if (tgt == clock_24hr_switch && code == LV_EVENT_VALUE_CHANGED) {
    use_24_hour = lv_obj_has_state(clock_24hr_switch, LV_STATE_CHECKED);
  }

  if (tgt == night_mode_switch && code == LV_EVENT_VALUE_CHANGED) {
    use_night_mode = lv_obj_has_state(night_mode_switch, LV_STATE_CHECKED);
  }

  if (tgt == language_dropdown && code == LV_EVENT_VALUE_CHANGED) {
    current_language = (Language)lv_dropdown_get_selected(language_dropdown);
    // Update the UI immediately to reflect language change
    lv_obj_del(settings_win);
    settings_win = nullptr;
    
    // Save preferences and recreate UI with new language
    prefs.putBool("useFahrenheit", use_fahrenheit);
    prefs.putBool("use24Hour", use_24_hour);
    prefs.putBool("useNightMode", use_night_mode);
    prefs.putUInt("language", current_language);

    lv_keyboard_set_textarea(kb, nullptr);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    
    // Recreate the main UI with the new language
    lv_obj_clean(lv_scr_act());
    create_ui();
    fetch_and_update_weather();
    return;
  }

  if (tgt == btn_close_obj && code == LV_EVENT_CLICKED) {
    prefs.putBool("useFahrenheit", use_fahrenheit);
    prefs.putBool("use24Hour", use_24_hour);
    prefs.putBool("useNightMode", use_night_mode);
    prefs.putUInt("language", current_language);

    lv_keyboard_set_textarea(kb, nullptr);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);

    lv_obj_del(settings_win);
    settings_win = nullptr;

    fetch_and_update_weather();
  }
}

// Screen dimming functions implementation
bool night_mode_should_be_active() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 0)) return false;

  if (!use_night_mode) return false;
  
  int hour = timeinfo.tm_hour;
  return (hour >= NIGHT_MODE_START_HOUR || hour < NIGHT_MODE_END_HOUR);
}

void activate_night_mode() {
  analogWrite(LCD_BACKLIGHT_PIN, 0);
  night_mode_active = true;
}

void deactivate_night_mode() {
  analogWrite(LCD_BACKLIGHT_PIN, prefs.getUInt("brightness", 128));
  night_mode_active = false;
}

void check_for_night_mode() {
  bool night_mode_time = night_mode_should_be_active();

  if (night_mode_time && !night_mode_active && !temp_screen_wakeup_active) {
    activate_night_mode();
  } else if (!night_mode_time && night_mode_active) {
    deactivate_night_mode();
  }
}

void handle_temp_screen_wakeup_timeout(lv_timer_t *timer) {
  if (temp_screen_wakeup_active) {
    temp_screen_wakeup_active = false;

    if (night_mode_should_be_active()) {
      activate_night_mode();
    }
  }
  
  if (temp_screen_wakeup_timer) {
    lv_timer_del(temp_screen_wakeup_timer);
    temp_screen_wakeup_timer = nullptr;
  }
}

void do_geocode_query(const char *q) {
  geoDoc.clear();
  String url = String("https://geocoding-api.open-meteo.com/v1/search?name=") + urlencode(q) + "&count=15";

  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(5000);
  http.begin(url);
  if (http.GET() == HTTP_CODE_OK) {
    Serial.println("Completed location search at open-meteo: " + url);
    auto err = deserializeJson(geoDoc, http.getString());
    if (!err) {
      geoResults = geoDoc["results"].as<JsonArray>();
      populate_results_dropdown();
    } else {
        Serial.println("Failed to parse search response from open-meteo: " + url);
    }
  } else {
      Serial.println("Failed location search at open-meteo: " + url);
  }
  http.end();
}

void fetch_and_update_weather() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi no longer connected. Attempting to reconnect...");
    WiFi.disconnect();
    WiFiManager wm;  
    wm.autoConnect(DEFAULT_CAPTIVE_SSID);
    delay(1000);  
    if (WiFi.status() != WL_CONNECTED) { 
      Serial.println("WiFi connection still unavailable.");
      return;   
    }
    Serial.println("WiFi connection reestablished.");
  }


  String url = String("http://api.open-meteo.com/v1/forecast?latitude=")
               + latitude + "&longitude=" + longitude
               + "&current=temperature_2m,apparent_temperature,is_day,weather_code"
               + "&daily=temperature_2m_min,temperature_2m_max,weather_code"
               + "&hourly=temperature_2m,precipitation_probability,is_day,weather_code"
               + "&forecast_hours=7"
               + "&timezone=auto";

  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(5000);
  http.begin(url);

  if (http.GET() == HTTP_CODE_OK) {
    Serial.println("Updated weather from open-meteo: " + url);

    String payload = http.getString();
    DynamicJsonDocument doc(32 * 1024);

    if (deserializeJson(doc, payload) == DeserializationError::Ok) {
      bool complete = !doc["current"]["temperature_2m"].isNull()
          && !doc["current"]["apparent_temperature"].isNull();
      const char *daily_fields[] = {"time", "temperature_2m_min", "temperature_2m_max", "weather_code"};
      const char *hourly_fields[] = {"time", "temperature_2m", "precipitation_probability", "weather_code", "is_day"};
      for (const char *key : daily_fields) complete &= doc["daily"][key].size() >= 7;
      for (const char *key : hourly_fields) complete &= doc["hourly"][key].size() >= 7;
      for (int i = 0; complete && i < 7; ++i) {
        const char *day = doc["daily"]["time"][i];
        const char *hour = doc["hourly"]["time"][i];
        complete &= day && strlen(day) >= 10 && hour && strlen(hour) >= 16;
      }
      if (!complete) {
        Serial.println("Incomplete weather response; keeping previous readings.");
        http.end();
        return;
      }
      float t_now = doc["current"]["temperature_2m"].as<float>();
      float t_ap = doc["current"]["apparent_temperature"].as<float>();
      int code_now = doc["current"]["weather_code"].as<int>();
      int is_day = doc["current"]["is_day"].as<int>();

      if (use_fahrenheit) {
        t_now = t_now * 9.0 / 5.0 + 32.0;
        t_ap = t_ap * 9.0 / 5.0 + 32.0;
      }
      const LocalizedStrings* strings = get_strings(current_language);

      int utc_offset_seconds = doc["utc_offset_seconds"].as<int>();
      configTime(utc_offset_seconds, 0, "pool.ntp.org", "time.nist.gov");
      Serial.print("Updating time from NTP with UTC offset: ");
      Serial.println(utc_offset_seconds);

      char unit = use_fahrenheit ? 'F' : 'C';
      lv_label_set_text_fmt(lbl_today_temp, "%.0f°%c", t_now, unit);
      lv_label_set_text_fmt(lbl_today_feels_like, "%s %.0f°%c", strings->feels_like_temp, t_ap, unit);
      lv_img_set_src(img_today_icon, choose_image(code_now, is_day));

      JsonArray times = doc["daily"]["time"].as<JsonArray>();
      JsonArray tmin = doc["daily"]["temperature_2m_min"].as<JsonArray>();
      JsonArray tmax = doc["daily"]["temperature_2m_max"].as<JsonArray>();
      JsonArray weather_codes = doc["daily"]["weather_code"].as<JsonArray>();

      for (int i = 0; i < 7; i++) {
        const char *date = times[i];
        int year = atoi(date + 0);
        int mon = atoi(date + 5);
        int dayd = atoi(date + 8);
        int dow = day_of_week(year, mon, dayd);
        const char *dayStr = (i == 0 && current_language != LANG_FR) ? strings->today : strings->weekdays[dow];

        float mn = tmin[i].as<float>();
        float mx = tmax[i].as<float>();
        if (use_fahrenheit) {
          mn = mn * 9.0 / 5.0 + 32.0;
          mx = mx * 9.0 / 5.0 + 32.0;
        }

        lv_label_set_text_fmt(lbl_daily_day[i], "%s", dayStr);
        lv_label_set_text_fmt(lbl_daily_high[i], "%.0f°", mx);
        lv_label_set_text_fmt(lbl_daily_low[i], "%.0f°", mn);
        lv_img_set_src(img_daily[i], choose_icon(weather_codes[i].as<int>(), (i == 0) ? is_day : 1));
      }

      JsonArray hours = doc["hourly"]["time"].as<JsonArray>();
      JsonArray hourly_temps = doc["hourly"]["temperature_2m"].as<JsonArray>();
      JsonArray precipitation_probabilities = doc["hourly"]["precipitation_probability"].as<JsonArray>();
      JsonArray hourly_weather_codes = doc["hourly"]["weather_code"].as<JsonArray>();
      JsonArray hourly_is_day = doc["hourly"]["is_day"].as<JsonArray>();

      for (int i = 0; i < 7; i++) {
        const char *date = hours[i];  // "YYYY-MM-DD"
        int hour = atoi(date + 11);
        int minute = atoi(date + 14);
        String hour_name = hour_of_day(hour);

        float precipitation_probability = precipitation_probabilities[i].as<float>();
        float temp = hourly_temps[i].as<float>();
        if (use_fahrenheit) {
          temp = temp * 9.0 / 5.0 + 32.0;
        }

        if (i == 0 && current_language != LANG_FR) {
          lv_label_set_text(lbl_hourly[i], strings->now);
        } else {
          lv_label_set_text(lbl_hourly[i], hour_name.c_str());
        }
        lv_label_set_text_fmt(lbl_precipitation_probability[i], "%.0f%%", precipitation_probability);
        lv_label_set_text_fmt(lbl_hourly_temp[i], "%.0f°", temp);
        lv_img_set_src(img_hourly[i], choose_icon(hourly_weather_codes[i].as<int>(), hourly_is_day[i].as<int>()));
      }


    } else {
      Serial.println("JSON parse failed on result from " + url);
    }
  } else {
    Serial.println("HTTP GET failed at " + url);
  }
  http.end();
}

const lv_image_dsc_t *choose_image(int code, int is_day) {
  return manga_weather_image(code, is_day, true);
}

const lv_image_dsc_t *choose_icon(int code, int is_day) {
  return manga_weather_image(code, is_day, false);
}

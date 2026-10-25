// Layout for the 240 x 320 ILI9341 portrait panel. All coordinates are pixels.
// Copyright 2026. GPL-3.0-only, as a modification of Aura.
static lv_obj_t *lbl_home_location;
static lv_obj_t *lbl_col_mid;
static lv_obj_t *lbl_col_right;
static lv_obj_t *btn_daily;
static lv_obj_t *btn_hourly;
static bool minimal_daily = true;

void daily_cb(lv_event_t *e);
void hourly_cb(lv_event_t *e);
static void change_location_event_cb(lv_event_t *e);
static void reset_wifi_event_handler(lv_event_t *e);

static void minimal_surface(lv_obj_t *obj) {
  lv_obj_set_style_bg_color(obj, lv_color_white(), 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_NONE, 0);
  lv_obj_set_style_text_color(obj, lv_color_black(), 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_set_style_radius(obj, 0, 0);
  lv_obj_set_style_shadow_width(obj, 0, 0);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

static lv_obj_t *minimal_label(lv_obj_t *parent, const char *value,
    const lv_font_t *font, int x, int y, int width) {
  lv_obj_t *label = lv_label_create(parent);
  lv_label_set_text(label, value);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, lv_color_black(), 0);
  lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
  lv_obj_set_pos(label, x, y);
  lv_obj_set_width(label, width);
  lv_obj_remove_flag(label, LV_OBJ_FLAG_CLICKABLE);
  return label;
}

static void minimal_rule(lv_obj_t *parent, int y, int width = 216) {
  lv_obj_t *rule = lv_obj_create(parent);
  minimal_surface(rule);
  lv_obj_set_style_bg_color(rule, lv_color_hex(0xD0D0D0), 0);
  lv_obj_set_size(rule, width, 1);
  lv_obj_set_pos(rule, 0, y);
  lv_obj_remove_flag(rule, LV_OBJ_FLAG_CLICKABLE);
}

static void minimal_button_style(lv_obj_t *button, bool selected) {
  lv_obj_set_style_bg_color(button, selected ? lv_color_black() : lv_color_white(), 0);
  lv_obj_set_style_bg_color(button, lv_color_hex(0xD9D9D9), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(button, selected ? lv_color_white() : lv_color_black(), 0);
  lv_obj_set_style_text_color(button, lv_color_black(), LV_STATE_PRESSED);
  lv_obj_set_style_border_width(button, 1, 0);
  lv_obj_set_style_border_color(button, lv_color_black(), 0);
  lv_obj_set_style_radius(button, 3, 0);
  lv_obj_set_style_pad_all(button, 0, 0);
  lv_obj_set_style_shadow_width(button, 0, 0);
}

static lv_obj_t *minimal_button(lv_obj_t *parent, const char *text,
    int x, int y, int width, int height) {
  lv_obj_t *button = lv_button_create(parent);
  lv_obj_set_pos(button, x, y);
  lv_obj_set_size(button, width, height);
  minimal_button_style(button, false);
  lv_obj_t *label = lv_label_create(button);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, get_font_14(), 0);
  lv_obj_center(label);
  return button;
}

static const char *minimal_days_text() {
  const char *text[] = {"7 days", "7 días", "7 Tage", "7 jours", "7 gün", "7 dagar", "7 giorni"};
  return text[current_language];
}
static const char *minimal_hours_text() {
  const char *text[] = {"Hourly", "Horas", "Stunden", "Heures", "Saatlik", "Timmar", "Oraria"};
  return text[current_language];
}

static void minimal_show_forecast(bool daily) {
  minimal_daily = daily;
  if (daily) {
    lv_obj_remove_flag(box_daily, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(box_hourly, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(box_daily, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(box_hourly, LV_OBJ_FLAG_HIDDEN);
  }
  const LocalizedStrings *s = get_strings(current_language);
  lv_label_set_text(lbl_forecast, daily ? s->seven_day_forecast : s->hourly_forecast);
  lv_label_set_text(lbl_col_mid, daily ? "min" : "%");
  lv_label_set_text(lbl_col_right, daily ? "max" : (use_fahrenheit ? "°F" : "°C"));
  minimal_button_style(btn_daily, daily);
  minimal_button_style(btn_hourly, !daily);
}

void create_ui() {
  const LocalizedStrings *s = get_strings(current_language);
  lv_obj_t *scr = lv_scr_act();
  minimal_surface(scr);
  lbl_home_location = minimal_label(scr, location.c_str(), get_font_14(), 12, 9, 126);
  lv_obj_set_height(lbl_home_location, 19);
  lv_label_set_long_mode(lbl_home_location, LV_LABEL_LONG_DOT);
  lbl_clock = minimal_label(scr, "--:--", get_font_16(), 142, 8, 86);
  lv_obj_set_style_text_align(lbl_clock, LV_TEXT_ALIGN_RIGHT, 0);

  lbl_today_temp = minimal_label(scr, s->temp_placeholder, &lv_font_montserrat_48, 10, 38, 170);
  lv_label_set_long_mode(lbl_today_temp, LV_LABEL_LONG_CLIP);
  img_today_icon = lv_image_create(scr);
  lv_image_set_src(img_today_icon, manga_weather_image(2, 1, true));
  lv_obj_set_pos(img_today_icon, 180, 45);
  lbl_today_feels_like = minimal_label(scr, s->feels_like_temp, get_font_14(), 12, 99, 216);
  lv_obj_set_height(lbl_today_feels_like, 20);
  lv_label_set_long_mode(lbl_today_feels_like, LV_LABEL_LONG_DOT);

  lbl_forecast = minimal_label(scr, s->seven_day_forecast, get_font_12(), 12, 124, 140);
  lv_obj_set_height(lbl_forecast, 16);
  lv_label_set_long_mode(lbl_forecast, LV_LABEL_LONG_DOT);
  lbl_col_mid = minimal_label(scr, "min", get_font_12(), 151, 124, 34);
  lbl_col_right = minimal_label(scr, "max", get_font_12(), 190, 124, 38);
  lv_obj_set_style_text_align(lbl_col_mid, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_style_text_align(lbl_col_right, LV_TEXT_ALIGN_RIGHT, 0);

  box_daily = lv_obj_create(scr);
  box_hourly = lv_obj_create(scr);
  for (lv_obj_t *box : {box_daily, box_hourly}) {
    minimal_surface(box);
    lv_obj_set_pos(box, 12, 142);
    lv_obj_set_size(box, 216, 140);
  }
  lv_obj_add_event_cb(box_daily, daily_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(box_hourly, hourly_cb, LV_EVENT_CLICKED, nullptr);
  for (int i = 0; i < 7; ++i) {
    int y = i * 20;
    lbl_daily_day[i] = minimal_label(box_daily, "--", get_font_16(), 0, y, 74);
    lbl_daily_low[i] = minimal_label(box_daily, "--°", get_font_16(), 112, y, 61);
    lbl_daily_high[i] = minimal_label(box_daily, "--°", get_font_16(), 173, y, 43);
    lv_obj_set_style_text_color(lbl_daily_low[i], lv_color_hex(0x454545), 0);
    lv_obj_set_style_text_align(lbl_daily_low[i], LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_align(lbl_daily_high[i], LV_TEXT_ALIGN_RIGHT, 0);
    img_daily[i] = lv_image_create(box_daily);
    lv_image_set_src(img_daily[i], manga_weather_image(2, 1, false));
    lv_obj_set_pos(img_daily[i], 80, y - 1);

    lbl_hourly[i] = minimal_label(box_hourly, "--", get_font_16(), 0, y, 74);
    lbl_precipitation_probability[i] = minimal_label(box_hourly, "--%", get_font_16(), 112, y, 61);
    lbl_hourly_temp[i] = minimal_label(box_hourly, "--°", get_font_16(), 173, y, 43);
    lv_obj_set_style_text_color(lbl_precipitation_probability[i], lv_color_hex(0x454545), 0);
    lv_obj_set_style_text_align(lbl_precipitation_probability[i], LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_align(lbl_hourly_temp[i], LV_TEXT_ALIGN_RIGHT, 0);
    img_hourly[i] = lv_image_create(box_hourly);
    lv_image_set_src(img_hourly[i], manga_weather_image(2, 1, false));
    lv_obj_set_pos(img_hourly[i], 80, y - 1);
  }
  btn_daily = minimal_button(scr, minimal_days_text(), 8, 284, 80, 32);
  btn_hourly = minimal_button(scr, minimal_hours_text(), 92, 284, 88, 32);
  lv_obj_add_event_cb(btn_daily, [](lv_event_t *) { minimal_show_forecast(true); }, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(btn_hourly, [](lv_event_t *) { minimal_show_forecast(false); }, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *settings = minimal_button(scr, LV_SYMBOL_SETTINGS, 188, 284, 44, 32);
  lv_obj_set_style_text_font(lv_obj_get_child(settings, 0), &lv_font_montserrat_20, 0);
  lv_obj_add_event_cb(settings, screen_event_cb, LV_EVENT_CLICKED, nullptr);
  minimal_show_forecast(minimal_daily);
}

static lv_obj_t *minimal_switch(lv_obj_t *parent, const char *text, int y, bool checked) {
  lv_obj_t *label = minimal_label(parent, text, get_font_14(), 0, y + 4, 150);
  lv_obj_set_height(label, 34);
  lv_obj_t *sw = lv_switch_create(parent);
  lv_obj_set_pos(sw, 158, y);
  lv_obj_set_size(sw, 46, 28);
  lv_obj_set_ext_click_area(sw, 6);
  lv_obj_set_style_bg_color(sw, lv_color_hex(0xB0B0B0), LV_PART_MAIN);
  lv_obj_set_style_bg_color(sw, lv_color_black(), LV_PART_INDICATOR | LV_STATE_CHECKED);
  lv_obj_set_style_bg_color(sw, lv_color_white(), LV_PART_KNOB);
  if (checked) lv_obj_add_state(sw, LV_STATE_CHECKED);
  lv_obj_add_event_cb(sw, settings_event_handler, LV_EVENT_VALUE_CHANGED, nullptr);
  return sw;
}

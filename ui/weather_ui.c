#include <string.h>
#include <stdio.h>
#include <math.h>

#include "weather_ui.h"
#include "screens.h"
#include "actions.h"
#include "ui.h"

#if !LV_FONT_MONTSERRAT_12 || !LV_FONT_MONTSERRAT_16 || !LV_FONT_MONTSERRAT_20 || !LV_FONT_MONTSERRAT_48
#error "Weather UI needs LV_FONT_MONTSERRAT_12, _16, _20 and _48 enabled in lv_conf.h"
#endif
#if !LV_USE_CHART
#error "Weather UI needs LV_USE_CHART enabled in lv_conf.h"
#endif

//
// Configuration
//

#define WX_HISTORY_POINTS  48        // samples kept on the trend chart
#define WX_STALE_MS        60000UL   // no packet for 1 min  -> STALE
#define WX_OFFLINE_MS      300000UL  // no packet for 5 min  -> OFFLINE

#define WX_COLOR_TEXT_MUTED  0x6B7A94
#define WX_COLOR_TEMP        0xFF8A3D
#define WX_COLOR_HUMIDITY    0x38BDF8
#define WX_COLOR_GOOD        0x22C55E
#define WX_COLOR_WARN        0xF59E0B
#define WX_COLOR_BAD         0xEF4444
#define WX_COLOR_COLD        0x60A5FA

//
// State
//

typedef enum { WX_CLEAR, WX_PARTLY, WX_CLOUDY, WX_RAIN } wx_condition_t;
typedef enum { LINK_WAITING, LINK_LIVE, LINK_STALE, LINK_OFFLINE } link_state_t;

static struct {
    bool has_data;
    float temp;
    float hum;
    float t_min;
    float t_max;
    uint32_t last_rx_ms;
} wx;

static float hist_temp[WX_HISTORY_POINTS];
static float hist_hum[WX_HISTORY_POINTS];
static uint16_t hist_count = 0;
static uint16_t hist_head = 0;

static lv_chart_series_t *ser_temp;
static lv_chart_series_t *ser_hum;

//
// Weather math
//

// Dew point, Magnus formula
static float wx_dew_point(float t, float h) {
    if (h < 1.0f) h = 1.0f;
    const float a = 17.62f, b = 243.12f;
    float g = logf(h / 100.0f) + a * t / (b + t);
    return b * g / (a - g);
}

// "Feels like": NOAA heat index (only meaningful when warm and humid)
static float wx_feels_like(float t, float h) {
    if (t < 26.7f || h < 40.0f) return t;
    float f = t * 1.8f + 32.0f;
    float hi = -42.379f + 2.04901523f * f + 10.14333127f * h
             - 0.22475541f * f * h - 0.00683783f * f * f
             - 0.05481717f * h * h + 0.00122874f * f * f * h
             + 0.00085282f * f * h * h - 0.00000199f * f * f * h * h;
    return (hi - 32.0f) / 1.8f;
}

// Sky estimate from humidity only (the sensor has no pressure / light input)
static wx_condition_t wx_classify(float t, float h, const char **text) {
    if (h >= 85.0f) { *text = "Rain Likely";   return WX_RAIN; }
    if (h >= 70.0f) { *text = "Cloudy";        return WX_CLOUDY; }
    if (h >= 55.0f) { *text = "Partly Cloudy"; return WX_PARTLY; }
    *text = (t >= 30.0f) ? "Hot & Clear" : "Clear";
    return WX_CLEAR;
}

static const char *wx_comfort(float t, float h, float feels, uint32_t *color) {
    if (feels >= 40.0f) { *color = WX_COLOR_BAD;   return "Danger"; }
    if (feels >= 32.0f) { *color = WX_COLOR_BAD;   return "Hot"; }
    if (t >= 28.0f)     { *color = WX_COLOR_WARN;  return "Warm"; }
    if (t < 5.0f)       { *color = WX_COLOR_COLD;  return "Cold"; }
    if (t < 16.0f)      { *color = WX_COLOR_COLD;  return "Cool"; }
    if (h >= 70.0f)     { *color = WX_COLOR_WARN;  return "Humid"; }
    if (h < 30.0f)      { *color = WX_COLOR_WARN;  return "Dry"; }
    *color = WX_COLOR_GOOD;
    return "Ideal";
}

static const char *wx_dew_desc(float dp) {
    if (dp < 10.0f) return "Dry air";
    if (dp < 16.0f) return "Pleasant";
    if (dp < 18.0f) return "Humid";
    if (dp < 21.0f) return "Muggy";
    return "Oppressive";
}

static void wx_format_age(char *buf, size_t n, uint32_t ms) {
    unsigned long s = ms / 1000;
    if (s < 5)         snprintf(buf, n, "now");
    else if (s < 60)   snprintf(buf, n, "%lus ago", s);
    else if (s < 3600) snprintf(buf, n, "%lum ago", s / 60);
    else               snprintf(buf, n, "%luh ago", s / 3600);
}

//
// Animations
//

static void wx_anim_translate_x(void *obj, int32_t v) {
    lv_obj_set_style_translate_x((lv_obj_t *)obj, v, 0);
}

static void wx_anim_drop(void *obj, int32_t v) {
    lv_obj_set_style_translate_y((lv_obj_t *)obj, v, 0);
    lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)(255 - v * 22), 0);
}

static void wx_anim_opa(void *obj, int32_t v) {
    lv_obj_set_style_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
}

static void wx_anim_start(lv_obj_t *obj, lv_anim_exec_xcb_t cb, int32_t from, int32_t to,
                          uint32_t duration, uint32_t reverse, uint32_t delay) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, cb);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_duration(&a, duration);
    lv_anim_set_reverse_duration(&a, reverse);
    lv_anim_set_delay(&a, delay);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

//
// Weather icon (sun / clouds / drops are panels defined in EEZ)
//

static void wx_cloud_set_color(lv_obj_t *cloud, uint32_t color) {
    uint32_t n = lv_obj_get_child_count(cloud);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_set_style_bg_color(lv_obj_get_child(cloud, i), lv_color_hex(color), 0);
    }
}

static void wx_set_visible(lv_obj_t *obj, bool visible) {
    if (visible) lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

static void wx_icon_set(wx_condition_t c) {
    lv_obj_t *drops[] = { objects.icon_drop_1, objects.icon_drop_2, objects.icon_drop_3 };

    wx_set_visible(objects.icon_sun, c == WX_CLEAR || c == WX_PARTLY);
    wx_set_visible(objects.icon_cloud_back, c == WX_CLOUDY || c == WX_RAIN);
    wx_set_visible(objects.icon_cloud_front, c != WX_CLEAR);
    for (int i = 0; i < 3; i++) wx_set_visible(drops[i], c == WX_RAIN);

    switch (c) {
    case WX_CLEAR:
        lv_obj_set_pos(objects.icon_sun, 18, 18);
        break;
    case WX_PARTLY:
        lv_obj_set_pos(objects.icon_sun, 30, 6);
        wx_cloud_set_color(objects.icon_cloud_front, 0xF1F5F9);
        lv_obj_set_pos(objects.icon_cloud_front, 4, 34);
        break;
    case WX_CLOUDY:
        wx_cloud_set_color(objects.icon_cloud_back, 0x94A3B8);
        lv_obj_set_pos(objects.icon_cloud_back, 16, 12);
        wx_cloud_set_color(objects.icon_cloud_front, 0xE2E8F0);
        lv_obj_set_pos(objects.icon_cloud_front, 2, 34);
        break;
    case WX_RAIN:
        wx_cloud_set_color(objects.icon_cloud_back, 0x64748B);
        lv_obj_set_pos(objects.icon_cloud_back, 16, 4);
        wx_cloud_set_color(objects.icon_cloud_front, 0xCBD5E1);
        lv_obj_set_pos(objects.icon_cloud_front, 4, 20);
        break;
    }
}

static void wx_hero_set_gradient(wx_condition_t c) {
    static const uint32_t grad[][2] = {
        { 0x3B82F6, 0x1E40AF },  // clear
        { 0x4A78BD, 0x27416E },  // partly cloudy
        { 0x4E5D75, 0x2A3546 },  // cloudy
        { 0x3A4A66, 0x1B2536 },  // rain
    };
    lv_obj_set_style_bg_color(objects.hero_card, lv_color_hex(grad[c][0]), 0);
    lv_obj_set_style_bg_grad_color(objects.hero_card, lv_color_hex(grad[c][1]), 0);
}

//
// Refresh logic
//

static link_state_t wx_link_state(uint32_t now) {
    if (!wx.has_data) return LINK_WAITING;
    uint32_t age = now - wx.last_rx_ms;
    if (age >= WX_OFFLINE_MS) return LINK_OFFLINE;
    if (age >= WX_STALE_MS) return LINK_STALE;
    return LINK_LIVE;
}

static void wx_refresh_times(uint32_t now) {
    static const char *link_text[] = { "WAITING", "LIVE", "STALE", "OFFLINE" };
    static const uint32_t link_color[] = { WX_COLOR_TEXT_MUTED, WX_COLOR_GOOD, WX_COLOR_WARN, WX_COLOR_BAD };
    lv_obj_t *dots[] = { objects.status_dot_now, objects.status_dot_lights, objects.status_dot_trends, objects.status_dot_devices, objects.status_dot_menu };
    lv_obj_t *labels[] = { objects.status_label_now, objects.status_label_lights, objects.status_label_trends, objects.status_label_devices, objects.status_label_menu };
    char buf[32];

    link_state_t st = wx_link_state(now);
    lv_color_t color = lv_color_hex(link_color[st]);
    for (int i = 0; i < 5; i++) {
        lv_obj_set_style_bg_color(dots[i], color, 0);
        lv_obj_set_style_text_color(labels[i], color, 0);
        lv_label_set_text(labels[i], link_text[st]);
    }

    if (wx.has_data) {
        char age[16];
        wx_format_age(age, sizeof(age), now - wx.last_rx_ms);
        snprintf(buf, sizeof(buf), LV_SYMBOL_REFRESH " %s", age);
        lv_label_set_text(objects.hero_updated, buf);
    }
}

static void wx_refresh_now(void) {
    char buf[48];
    float t = wx.temp, h = wx.hum;

    snprintf(buf, sizeof(buf), "%.1f", t);
    lv_label_set_text(objects.temperature, buf);

    const char *cond_text;
    wx_condition_t cond = wx_classify(t, h, &cond_text);
    lv_label_set_text(objects.condition, cond_text);
    wx_icon_set(cond);
    wx_hero_set_gradient(cond);

    snprintf(buf, sizeof(buf), LV_SYMBOL_UP " %.1f\xC2\xB0   " LV_SYMBOL_DOWN " %.1f\xC2\xB0", wx.t_max, wx.t_min);
    lv_label_set_text(objects.temp_high_low, buf);

    snprintf(buf, sizeof(buf), "%.0f%%", h);
    lv_label_set_text(objects.humidity, buf);
    lv_bar_set_value(objects.humidity_bar, (int32_t)(h + 0.5f), LV_ANIM_ON);

    float dp = wx_dew_point(t, h);
    snprintf(buf, sizeof(buf), "%.1f\xC2\xB0", dp);
    lv_label_set_text(objects.dew_point, buf);
    lv_label_set_text(objects.dew_point_desc, wx_dew_desc(dp));

    float feels = wx_feels_like(t, h);
    uint32_t comfort_color;
    lv_label_set_text(objects.comfort, wx_comfort(t, h, feels, &comfort_color));
    lv_obj_set_style_text_color(objects.comfort, lv_color_hex(comfort_color), 0);
    snprintf(buf, sizeof(buf), "Feels %.1f\xC2\xB0", feels);
    lv_label_set_text(objects.feels_like, buf);
}

static void wx_refresh_trend(void) {
    char buf[64];
    float tmin = hist_temp[0], tmax = hist_temp[0];
    float hmin = hist_hum[0], hmax = hist_hum[0];
    for (uint16_t i = 1; i < hist_count; i++) {
        if (hist_temp[i] < tmin) tmin = hist_temp[i];
        if (hist_temp[i] > tmax) tmax = hist_temp[i];
        if (hist_hum[i] < hmin) hmin = hist_hum[i];
        if (hist_hum[i] > hmax) hmax = hist_hum[i];
    }

    // Auto-scale temperature axis (values are stored in tenths of a degree)
    int32_t lo = (int32_t)floorf(tmin - 1.0f) * 10;
    int32_t hi = (int32_t)ceilf(tmax + 1.0f) * 10;
    lv_chart_set_axis_range(objects.chart, LV_CHART_AXIS_PRIMARY_Y, lo, hi);
    lv_chart_refresh(objects.chart);

    snprintf(buf, sizeof(buf),
             "Temp %.1f-%.1f\xC2\xB0  " LV_SYMBOL_BULLET "  Hum %.0f-%.0f%%  " LV_SYMBOL_BULLET "  %u pts",
             tmin, tmax, hmin, hmax, (unsigned)hist_count);
    lv_label_set_text(objects.trend_summary, buf);
}

//
// Public API
//

void weather_ui_init(void) {
    // Chart: series and axes are runtime-only (EEZ has no chart properties)
    lv_obj_t *chart = objects.chart;
    lv_obj_remove_flag(chart, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(chart, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_line_width(chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_size(chart, 0, 0, LV_PART_INDICATOR);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart, WX_HISTORY_POINTS);
    lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_div_line_count(chart, 5, 7);
    lv_chart_set_axis_range(chart, LV_CHART_AXIS_PRIMARY_Y, 150, 300);
    lv_chart_set_axis_range(chart, LV_CHART_AXIS_SECONDARY_Y, 0, 1000);
    // Humidity first so the temperature line is drawn on top
    ser_hum = lv_chart_add_series(chart, lv_color_hex(WX_COLOR_HUMIDITY), LV_CHART_AXIS_SECONDARY_Y);
    ser_temp = lv_chart_add_series(chart, lv_color_hex(WX_COLOR_TEMP), LV_CHART_AXIS_PRIMARY_Y);

    // Ambient animations
    wx_anim_start(objects.icon_cloud_front, wx_anim_translate_x, -3, 3, 2600, 2600, 0);
    lv_obj_t *drops[] = { objects.icon_drop_1, objects.icon_drop_2, objects.icon_drop_3 };
    for (int i = 0; i < 3; i++) {
        wx_anim_start(drops[i], wx_anim_drop, 0, 8, 700, 0, i * 230);
    }
    lv_obj_t *dots[] = { objects.status_dot_now, objects.status_dot_lights, objects.status_dot_trends, objects.status_dot_devices, objects.status_dot_menu };
    for (int i = 0; i < 5; i++) {
        wx_anim_start(dots[i], wx_anim_opa, LV_OPA_COVER, 90, 900, 900, 0);
    }

    lv_obj_set_style_anim_duration(objects.humidity_bar, 500, LV_PART_MAIN);
    wx_refresh_times(lv_tick_get());
}

void weather_ui_tick(void) {
    static uint32_t last_refresh = 0;
    uint32_t now = lv_tick_get();
    if (now - last_refresh < 1000) return;
    last_refresh = now;
    wx_refresh_times(now);
}

void weather_ui_set_data(float temperature, float humidity) {
    if (temperature != temperature || humidity != humidity) return;  // NaN guard
    if (humidity < 0.0f) humidity = 0.0f;
    if (humidity > 100.0f) humidity = 100.0f;

    if (!wx.has_data) {
        wx.t_min = wx.t_max = temperature;
    } else {
        if (temperature < wx.t_min) wx.t_min = temperature;
        if (temperature > wx.t_max) wx.t_max = temperature;
    }
    wx.has_data = true;
    wx.temp = temperature;
    wx.hum = humidity;
    wx.last_rx_ms = lv_tick_get();

    hist_temp[hist_head] = temperature;
    hist_hum[hist_head] = humidity;
    hist_head = (hist_head + 1) % WX_HISTORY_POINTS;
    if (hist_count < WX_HISTORY_POINTS) hist_count++;

    lv_chart_set_next_value(objects.chart, ser_temp, (int32_t)lroundf(temperature * 10.0f));
    lv_chart_set_next_value(objects.chart, ser_hum, (int32_t)lroundf(humidity * 10.0f));

    wx_refresh_now();
    wx_refresh_trend();
    wx_refresh_times(wx.last_rx_ms);
}

void weather_ui_set_indoor(bool has_data, float temperature, float humidity) {
    char buf[24];
    if (!has_data) {
        lv_label_set_text(objects.indoor_temp, "--");
        lv_label_set_text(objects.indoor_hum, "--");
        return;
    }
    snprintf(buf, sizeof(buf), "%.1fÂ°", temperature);
    lv_label_set_text(objects.indoor_temp, buf);
    snprintf(buf, sizeof(buf), "%.0f%% RH", humidity);
    lv_label_set_text(objects.indoor_hum, buf);
}

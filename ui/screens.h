#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Screens

enum ScreensEnum {
    _SCREEN_ID_FIRST = 1,
    SCREEN_ID_MAIN = 1,
    SCREEN_ID_LIGHTS = 2,
    SCREEN_ID_TRENDS = 3,
    SCREEN_ID_DEVICES = 4,
    SCREEN_ID_MENU = 5,
    _SCREEN_ID_LAST = 5
};

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *lights;
    lv_obj_t *trends;
    lv_obj_t *devices;
    lv_obj_t *menu;
    lv_obj_t *status_pill_now;
    lv_obj_t *status_dot_now;
    lv_obj_t *status_label_now;
    lv_obj_t *hero_card;
    lv_obj_t *weather_icon;
    lv_obj_t *icon_sun;
    lv_obj_t *icon_cloud_back;
    lv_obj_t *icon_cloud_front;
    lv_obj_t *icon_drop_1;
    lv_obj_t *icon_drop_2;
    lv_obj_t *icon_drop_3;
    lv_obj_t *temp_row;
    lv_obj_t *temperature;
    lv_obj_t *temperature_unit;
    lv_obj_t *indoor_panel;
    lv_obj_t *indoor_temp;
    lv_obj_t *indoor_hum;
    lv_obj_t *condition;
    lv_obj_t *temp_high_low;
    lv_obj_t *hero_updated;
    lv_obj_t *humidity;
    lv_obj_t *humidity_bar;
    lv_obj_t *dew_point;
    lv_obj_t *dew_point_desc;
    lv_obj_t *comfort;
    lv_obj_t *feels_like;
    lv_obj_t *home_touch_area;
    lv_obj_t *status_pill_lights;
    lv_obj_t *status_dot_lights;
    lv_obj_t *status_label_lights;
    lv_obj_t *lights_room;
    lv_obj_t *lights_all_on;
    lv_obj_t *lights_all_off;
    lv_obj_t *lamp_btn_1;
    lv_obj_t *lamp_icon_1;
    lv_obj_t *lamp_name_1;
    lv_obj_t *lamp_state_1;
    lv_obj_t *lamp_btn_2;
    lv_obj_t *lamp_icon_2;
    lv_obj_t *lamp_name_2;
    lv_obj_t *lamp_state_2;
    lv_obj_t *lamp_btn_3;
    lv_obj_t *lamp_icon_3;
    lv_obj_t *lamp_name_3;
    lv_obj_t *lamp_state_3;
    lv_obj_t *lights_status;
    lv_obj_t *status_pill_trends;
    lv_obj_t *status_dot_trends;
    lv_obj_t *status_label_trends;
    lv_obj_t *chart;
    lv_obj_t *trend_summary;
    lv_obj_t *status_pill_devices;
    lv_obj_t *status_dot_devices;
    lv_obj_t *status_label_devices;
    lv_obj_t *devices_count;
    lv_obj_t *pair_button;
    lv_obj_t *devices_empty;
    lv_obj_t *pairing_banner;
    lv_obj_t *pairing_label;
    lv_obj_t *device_list;
    lv_obj_t *gateway_info;
    lv_obj_t *device_dialog;
    lv_obj_t *dialog_title;
    lv_obj_t *dialog_info;
    lv_obj_t *role_btn_1;
    lv_obj_t *role_btn_2;
    lv_obj_t *role_btn_3;
    lv_obj_t *role_btn_4;
    lv_obj_t *role_btn_5;
    lv_obj_t *role_btn_6;
    lv_obj_t *role_btn_7;
    lv_obj_t *role_btn_8;
    lv_obj_t *dialog_forget;
    lv_obj_t *dialog_forget_label;
    lv_obj_t *status_pill_menu;
    lv_obj_t *status_dot_menu;
    lv_obj_t *status_label_menu;
} objects_t;

extern objects_t objects;

void create_screen_main();
void tick_screen_main();

void create_screen_lights();
void tick_screen_lights();

void create_screen_trends();
void tick_screen_trends();

void create_screen_devices();
void tick_screen_devices();

void create_screen_menu();
void tick_screen_menu();

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/

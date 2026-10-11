#ifndef EEZ_LVGL_UI_EVENTS_H
#define EEZ_LVGL_UI_EVENTS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void action_scan(lv_event_t * e);
extern void action_pair_device(lv_event_t * e);
extern void action_set_device_role(lv_event_t * e);
extern void action_forget_device(lv_event_t * e);
extern void action_close_device_dialog(lv_event_t * e);
extern void action_toggle_lamp(lv_event_t * e);
extern void action_lights_all(lv_event_t * e);
extern void action_open_menu(lv_event_t * e);
extern void action_menu_back(lv_event_t * e);
extern void action_navigate(lv_event_t * e);
extern void action_go_lights(lv_event_t * e);

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_EVENTS_H*/
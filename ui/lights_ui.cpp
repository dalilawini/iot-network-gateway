#include <stdio.h>
#include <string.h>

#include "lights_ui.h"
#include "screens.h"
#include "actions.h"
#include "DeviceRegistry.h"
#include "EspNowManager.h"

#define LT_LAMPS             3
#define LT_ACK_TIMEOUT_MS    1500   // no delivery report after this -> failed

#define LT_COLOR_DIVIDER     0x22324F
#define LT_COLOR_TEXT        0xF1F5F9
#define LT_COLOR_TEXT_SECOND 0xA9B6CC
#define LT_COLOR_TEXT_MUTED  0x6B7A94
#define LT_COLOR_LAMP_ON     0xFBBF24
#define LT_COLOR_LAMP_ON_TXT 0x1F2937
#define LT_COLOR_GOOD        0x22C55E
#define LT_COLOR_WARN        0xF59E0B
#define LT_COLOR_BAD         0xEF4444

static DeviceRegistry* lt_reg = nullptr;
static EspNowManager* lt_esp = nullptr;

// Command waiting for its ESP-NOW delivery report
static struct {
    bool active;
    uint8_t mac[6];
    uint8_t prev_mask;
    uint32_t sent_ms;
    char done_text[40];
} lt_pending;

static char lt_status_override[64];   // last command result, shown instead of the hint
static uint32_t lt_status_override_ms;

//
// Helpers
//

// The light page drives the switch device set to "Bedroom", else the first one found
static int lt_find_switch() {
    int first = -1;
    for (int i = 0; i < lt_reg->count(); i++) {
        const Device& d = lt_reg->get(i);
        if (d.type != DEVICE_SWITCH) continue;
        if (d.role == ROLE_BEDROOM) return i;
        if (first < 0) first = i;
    }
    return first;
}

static int lt_channels(const Device& d) {
    if (d.channels == 0) return LT_LAMPS;               // not announced yet
    return d.channels < LT_LAMPS ? d.channels : LT_LAMPS;
}

static void lt_set_status(const char* text, uint32_t color) {
    lv_label_set_text(objects.lights_status, text);
    lv_obj_set_style_text_color(objects.lights_status, lv_color_hex(color), 0);
}

static void lt_show_result(const char* text) {
    strncpy(lt_status_override, text, sizeof(lt_status_override) - 1);
    lt_status_override[sizeof(lt_status_override) - 1] = '\0';
    lt_status_override_ms = lv_tick_get();
}

static void lt_style_lamp(int n, bool on, bool visible, bool enabled) {
    lv_obj_t* btns[]   = { objects.lamp_btn_1, objects.lamp_btn_2, objects.lamp_btn_3 };
    lv_obj_t* icons[]  = { objects.lamp_icon_1, objects.lamp_icon_2, objects.lamp_icon_3 };
    lv_obj_t* states[] = { objects.lamp_state_1, objects.lamp_state_2, objects.lamp_state_3 };
    lv_obj_t* btn = btns[n], *icon = icons[n], *state = states[n];

    if (visible) lv_obj_remove_flag(btn, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_add_flag(btn, LV_OBJ_FLAG_HIDDEN);

    if (enabled) lv_obj_remove_state(btn, LV_STATE_DISABLED);
    else         lv_obj_add_state(btn, LV_STATE_DISABLED);

    if (on) {
        lv_obj_add_state(btn, LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(icon, lv_color_hex(LT_COLOR_LAMP_ON), 0);
        lv_obj_set_style_text_color(icon, lv_color_hex(LT_COLOR_LAMP_ON_TXT), 0);
        lv_obj_set_style_shadow_color(icon, lv_color_hex(LT_COLOR_LAMP_ON), 0);
        lv_obj_set_style_shadow_width(icon, 16, 0);
        lv_obj_set_style_shadow_opa(icon, LV_OPA_50, 0);
        lv_label_set_text(state, "ON");
        lv_obj_set_style_text_color(state, lv_color_hex(LT_COLOR_LAMP_ON), 0);
    } else {
        lv_obj_remove_state(btn, LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(icon, lv_color_hex(LT_COLOR_DIVIDER), 0);
        lv_obj_set_style_text_color(icon, lv_color_hex(LT_COLOR_TEXT_MUTED), 0);
        lv_obj_set_style_shadow_width(icon, 0, 0);
        lv_label_set_text(state, "OFF");
        lv_obj_set_style_text_color(state, lv_color_hex(LT_COLOR_TEXT_MUTED), 0);
    }
}

//
// Refresh
//

void lights_ui_refresh() {
    if (!lt_reg) return;
    uint32_t now = lv_tick_get();
    int idx = lt_find_switch();
    bool pairing = lt_esp->isPairing();

    if (idx < 0) {
        lv_label_set_text(objects.lights_room, "Bedroom");
        for (int n = 0; n < LT_LAMPS; n++) lt_style_lamp(n, false, true, false);
        lv_obj_add_state(objects.lights_all_on, LV_STATE_DISABLED);
        lv_obj_add_state(objects.lights_all_off, LV_STATE_DISABLED);
        lt_set_status("No light switch yet. Pair it from the Devices screen.", LT_COLOR_TEXT_SECOND);
        return;
    }

    const Device& d = lt_reg->get(idx);
    bool online = lt_reg->isOnline(idx, now);
    bool enabled = !pairing && !lt_pending.active;
    int channels = lt_channels(d);

    char buf[64];
    snprintf(buf, sizeof(buf), "%s  " LV_SYMBOL_BULLET "  %d lamp%s  " LV_SYMBOL_BULLET "  %s",
             d.role == ROLE_NONE ? "Light switch" : DeviceRegistry::roleName(d.role),
             channels, channels == 1 ? "" : "s", online ? "online" : "offline");
    lv_label_set_text(objects.lights_room, buf);

    for (int n = 0; n < LT_LAMPS; n++) {
        lt_style_lamp(n, d.switchMask & (1 << n), n < channels, enabled);
    }
    if (enabled) {
        lv_obj_remove_state(objects.lights_all_on, LV_STATE_DISABLED);
        lv_obj_remove_state(objects.lights_all_off, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(objects.lights_all_on, LV_STATE_DISABLED);
        lv_obj_add_state(objects.lights_all_off, LV_STATE_DISABLED);
    }

    // Status line: command in flight > last result (5 s) > general hint
    if (lt_pending.active) {
        lt_set_status(LV_SYMBOL_REFRESH "  Sending...", LT_COLOR_TEXT_SECOND);
    } else if (lt_status_override[0] && now - lt_status_override_ms < 5000) {
        bool ok = strncmp(lt_status_override, LV_SYMBOL_OK, strlen(LV_SYMBOL_OK)) == 0;
        lt_set_status(lt_status_override, ok ? LT_COLOR_GOOD : LT_COLOR_BAD);
    } else if (pairing) {
        lt_set_status("Hub is pairing, lights are unavailable for a moment.", LT_COLOR_WARN);
    } else if (!online && d.packets > 0) {
        lt_set_status(LV_SYMBOL_WARNING "  Light switch offline, commands may not arrive.", LT_COLOR_WARN);
    } else {
        lt_set_status("Tap a lamp to switch it on or off.", LT_COLOR_TEXT_MUTED);
    }
}

//
// Commands
//

// ch = 1..3 for one lamp, 0 for all lamps
static void lt_send(int ch, bool on, uint8_t new_mask, const char* done_text) {
    int idx = lt_find_switch();
    if (idx < 0 || lt_pending.active) return;
    const Device& d = lt_reg->get(idx);

    char json[48];
    snprintf(json, sizeof(json), "{\"cmd\":\"set\",\"ch\":%d,\"on\":%s}", ch, on ? "true" : "false");

    if (!lt_esp->sendJson(d.mac, json)) {
        lt_show_result(LV_SYMBOL_CLOSE "  Could not send, the hub is busy.");
        lights_ui_refresh();
        return;
    }

    // Show the new state right away; reverted if the board doesn't acknowledge
    lt_pending.active = true;
    memcpy(lt_pending.mac, d.mac, 6);
    lt_pending.prev_mask = d.switchMask;
    lt_pending.sent_ms = lv_tick_get();
    strncpy(lt_pending.done_text, done_text, sizeof(lt_pending.done_text) - 1);
    lt_pending.done_text[sizeof(lt_pending.done_text) - 1] = '\0';
    lt_reg->setSwitchMask(idx, new_mask);
    lights_ui_refresh();
}

void action_toggle_lamp(lv_event_t* e) {
    int ch = (int)(intptr_t)lv_event_get_user_data(e);
    int idx = lt_find_switch();
    if (idx < 0 || ch < 1 || ch > LT_LAMPS) return;

    uint8_t mask = lt_reg->get(idx).switchMask;
    bool on = !(mask & (1 << (ch - 1)));
    char text[40];
    snprintf(text, sizeof(text), "Lamp %d %s", ch, on ? "on" : "off");
    lt_send(ch, on, mask ^ (1 << (ch - 1)), text);
}

void action_lights_all(lv_event_t* e) {
    bool on = (int)(intptr_t)lv_event_get_user_data(e) != 0;
    int idx = lt_find_switch();
    if (idx < 0) return;
    uint8_t all = (uint8_t)((1 << lt_channels(lt_reg->get(idx))) - 1);
    lt_send(0, on, on ? all : 0, on ? "All lamps on" : "All lamps off");
}

//
// Public API
//

void lights_ui_init(DeviceRegistry& registry, EspNowManager& espNow) {
    lt_reg = &registry;
    lt_esp = &espNow;
    lights_ui_refresh();
}

void lights_ui_tick() {
    static uint32_t last_refresh = 0;
    uint32_t now = lv_tick_get();

    if (lt_pending.active) {
        EspNowManager::SendResult result = lt_esp->lastSendResult();
        bool timed_out = now - lt_pending.sent_ms > LT_ACK_TIMEOUT_MS;
        if (result == EspNowManager::SEND_OK || result == EspNowManager::SEND_FAILED || timed_out) {
            lt_pending.active = false;
            char text[64];
            if (result == EspNowManager::SEND_OK) {
                snprintf(text, sizeof(text), LV_SYMBOL_OK "  %s", lt_pending.done_text);
            } else {
                int idx = lt_reg->find(lt_pending.mac);
                if (idx >= 0) lt_reg->setSwitchMask(idx, lt_pending.prev_mask);
                snprintf(text, sizeof(text), LV_SYMBOL_CLOSE "  Light switch did not answer.");
            }
            lt_show_result(text);
            lights_ui_refresh();
            return;
        }
    }

    if (now - last_refresh < 1000) return;
    last_refresh = now;
    lights_ui_refresh();
}

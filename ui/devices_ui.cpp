#include <stdio.h>
#include <string.h>

#include "devices_ui.h"
#include "screens.h"
#include "actions.h"
#include "weather_ui.h"
#include "DeviceRegistry.h"
#include "EspNowManager.h"

#define DV_COLOR_CARD        0x18253F
#define DV_COLOR_CARD_PRESS  0x22324F
#define DV_COLOR_CARD_BORDER 0x26375A
#define DV_COLOR_TEXT        0xF1F5F9
#define DV_COLOR_TEXT_SECOND 0xA9B6CC
#define DV_COLOR_TEXT_MUTED  0x6B7A94
#define DV_COLOR_TEMP        0xFF8A3D
#define DV_COLOR_HUMIDITY    0x38BDF8
#define DV_COLOR_DEW         0x2DD4BF
#define DV_COLOR_GOOD        0x22C55E
#define DV_COLOR_BAD         0xEF4444
#define DV_COLOR_WARN        0xF59E0B

struct DeviceRow {
    lv_obj_t* title;
    lv_obj_t* badge;
    lv_obj_t* sub;
    lv_obj_t* value;
    lv_obj_t* state;
};

static DeviceRegistry* reg = nullptr;
static EspNowManager* esp = nullptr;
static char hub_mac[20] = "--";

static DeviceRow rows[DeviceRegistry::MAX_DEVICES];
static uint32_t shown_revision = 0xFFFFFFFF;
static bool shown_pairing = false;

static uint8_t dialog_mac[6];
static bool forget_armed = false;

//
// Helpers
//

static uint32_t dv_role_color(uint8_t role) {
    switch (role) {
    case ROLE_NONE:    return DV_COLOR_TEXT_MUTED;
    case ROLE_OUTDOOR: return DV_COLOR_TEMP;
    case ROLE_INDOOR:  return DV_COLOR_HUMIDITY;
    default:           return DV_COLOR_DEW;
    }
}

static const char* dv_type_symbol(uint8_t type) {
    switch (type) {
    case DEVICE_TEMP_HUM: return LV_SYMBOL_TINT;
    case DEVICE_SWITCH:   return LV_SYMBOL_POWER;
    default:              return LV_SYMBOL_WIFI;
    }
}

static void dv_format_age(char* buf, size_t n, uint32_t ms) {
    unsigned long s = ms / 1000;
    if (s < 5)         snprintf(buf, n, "now");
    else if (s < 60)   snprintf(buf, n, "%lus", s);
    else if (s < 3600) snprintf(buf, n, "%lum", s / 60);
    else               snprintf(buf, n, "%luh", s / 3600);
}

static lv_obj_t* dv_label(lv_obj_t* parent, const lv_font_t* font, uint32_t color, const char* text) {
    lv_obj_t* obj = lv_label_create(parent);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    lv_label_set_text(obj, text);
    return obj;
}

// Indoor tile on the weather screen follows whichever device has the Indoor role
static void dv_refresh_indoor() {
    int i = reg->findByRole(ROLE_INDOOR);
    if (i >= 0 && reg->get(i).hasData) {
        weather_ui_set_indoor(true, reg->get(i).temperature, reg->get(i).humidity);
    } else {
        weather_ui_set_indoor(false, 0, 0);
    }
}

//
// Device dialog
//

static void dv_dialog_refresh() {
    int i = reg->find(dialog_mac);
    if (i < 0) return;
    const Device& d = reg->get(i);

    char mac[20], buf[64];
    DeviceRegistry::formatMac(d.mac, mac, sizeof(mac));
    lv_label_set_text(objects.dialog_title, DeviceRegistry::roleName(d.role));
    snprintf(buf, sizeof(buf), "%s  " LV_SYMBOL_BULLET "  %s", mac, DeviceRegistry::typeName(d.type));
    lv_label_set_text(objects.dialog_info, buf);

    lv_obj_t* buttons[] = {
        objects.role_btn_1, objects.role_btn_2, objects.role_btn_3, objects.role_btn_4,
        objects.role_btn_5, objects.role_btn_6, objects.role_btn_7, objects.role_btn_8,
    };
    for (int r = 0; r < 8; r++) {
        if (d.role == r + 1) lv_obj_add_state(buttons[r], LV_STATE_CHECKED);
        else                 lv_obj_remove_state(buttons[r], LV_STATE_CHECKED);
    }
}

static void dv_dialog_close() {
    lv_obj_add_flag(objects.device_dialog, LV_OBJ_FLAG_HIDDEN);
}

static void dv_row_clicked(lv_event_t* e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);
    if (i < 0 || i >= reg->count()) return;
    memcpy(dialog_mac, reg->get(i).mac, 6);
    forget_armed = false;
    lv_label_set_text(objects.dialog_forget_label, "Forget");
    dv_dialog_refresh();
    lv_obj_remove_flag(objects.device_dialog, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(objects.device_dialog);
}

void action_set_device_role(lv_event_t* e) {
    int role = (int)(intptr_t)lv_event_get_user_data(e);
    int i = reg->find(dialog_mac);
    if (i < 0) return;
    reg->setRole(i, (DeviceRole)role);
    dv_dialog_refresh();
    dv_refresh_indoor();
}

void action_forget_device(lv_event_t* e) {
    LV_UNUSED(e);
    if (!forget_armed) {
        forget_armed = true;
        lv_label_set_text(objects.dialog_forget_label, "Tap to confirm");
        return;
    }
    reg->forget(reg->find(dialog_mac));
    dv_dialog_close();
    dv_refresh_indoor();
}

void action_close_device_dialog(lv_event_t* e) {
    LV_UNUSED(e);
    dv_dialog_close();
}

//
// Pairing
//

static void dv_refresh_pairing() {
    bool pairing = esp->isPairing();
    if (pairing) {
        char buf[64];
        snprintf(buf, sizeof(buf), LV_SYMBOL_WIFI "  Pairing  %lus  " LV_SYMBOL_BULLET "  power on the new device",
                 (unsigned long)((esp->pairingTimeLeft() + 999) / 1000));
        lv_label_set_text(objects.pairing_label, buf);
    }
    if (pairing == shown_pairing) return;
    shown_pairing = pairing;

    // Banner pushes the list down while visible
    if (pairing) {
        lv_obj_remove_flag(objects.pairing_banner, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(objects.device_list, 8, 96);
        lv_obj_set_height(objects.device_list, 112);
        lv_obj_add_state(objects.pair_button, LV_STATE_DISABLED);
    } else {
        lv_obj_add_flag(objects.pairing_banner, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(objects.device_list, 8, 60);
        lv_obj_set_height(objects.device_list, 148);
        lv_obj_remove_state(objects.pair_button, LV_STATE_DISABLED);
    }
}

void action_pair_device(lv_event_t* e) {
    LV_UNUSED(e);
    esp->requestPairing();   // EspNowManager::update() runs startAP() on the next loop
    dv_refresh_pairing();
}

//
// Device list
//

static void dv_rebuild_list() {
    lv_obj_clean(objects.device_list);

    for (int i = 0; i < reg->count(); i++) {
        const Device& d = reg->get(i);
        DeviceRow& r = rows[i];

        lv_obj_t* row = lv_obj_create(objects.device_list);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, LV_PCT(100), 44);
        lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(row, lv_color_hex(DV_COLOR_CARD), 0);
        lv_obj_set_style_bg_color(row, lv_color_hex(DV_COLOR_CARD_PRESS), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(row, 12, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(DV_COLOR_CARD_BORDER), 0);
        lv_obj_add_event_cb(row, dv_row_clicked, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        lv_obj_t* icon = lv_obj_create(row);
        lv_obj_remove_style_all(icon);
        lv_obj_remove_flag(icon, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(icon, 28, 28);
        lv_obj_align(icon, LV_ALIGN_LEFT_MID, 8, 0);
        lv_obj_set_style_radius(icon, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(icon, lv_color_hex(dv_role_color(d.role)), 0);
        lv_obj_set_style_bg_opa(icon, LV_OPA_30, 0);
        lv_obj_t* sym = dv_label(icon, &lv_font_montserrat_14, dv_role_color(d.role), dv_type_symbol(d.type));
        lv_obj_center(sym);

        lv_obj_t* head = lv_obj_create(row);
        lv_obj_remove_style_all(head);
        lv_obj_remove_flag(head, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(head, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_pos(head, 44, 5);
        lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(head, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(head, 6, 0);

        r.title = dv_label(head, &lv_font_montserrat_14, DV_COLOR_TEXT, DeviceRegistry::roleName(d.role));
        r.badge = dv_label(head, &lv_font_montserrat_12, 0x0B1220, "NEW");
        lv_obj_set_style_bg_color(r.badge, lv_color_hex(DV_COLOR_WARN), 0);
        lv_obj_set_style_bg_opa(r.badge, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(r.badge, 4, 0);
        lv_obj_set_style_pad_hor(r.badge, 4, 0);
        if (d.role != ROLE_NONE) lv_obj_add_flag(r.badge, LV_OBJ_FLAG_HIDDEN);

        r.sub = dv_label(row, &lv_font_montserrat_12, DV_COLOR_TEXT_MUTED, "");
        lv_obj_set_pos(r.sub, 44, 24);

        r.value = dv_label(row, &lv_font_montserrat_14, DV_COLOR_TEXT, "--");
        lv_obj_align(r.value, LV_ALIGN_TOP_RIGHT, -10, 5);

        r.state = dv_label(row, &lv_font_montserrat_12, DV_COLOR_TEXT_MUTED, "");
        lv_obj_align(r.state, LV_ALIGN_BOTTOM_RIGHT, -10, -5);
    }

    if (reg->count() > 0) lv_obj_add_flag(objects.devices_empty, LV_OBJ_FLAG_HIDDEN);
    else                  lv_obj_remove_flag(objects.devices_empty, LV_OBJ_FLAG_HIDDEN);

    shown_revision = reg->revision();
}

static void dv_refresh_rows(uint32_t now) {
    if (reg->revision() != shown_revision) dv_rebuild_list();

    int online = 0;
    char buf[48], age[12];
    for (int i = 0; i < reg->count(); i++) {
        const Device& d = reg->get(i);
        DeviceRow& r = rows[i];
        bool is_online = reg->isOnline(i, now);
        if (is_online) online++;

        if (d.packets > 0) {
            snprintf(buf, sizeof(buf), "%02X:%02X:%02X  %d dBm", d.mac[3], d.mac[4], d.mac[5], d.rssi);
        } else {
            snprintf(buf, sizeof(buf), "%02X:%02X:%02X  waiting", d.mac[3], d.mac[4], d.mac[5]);
        }
        lv_label_set_text(r.sub, buf);

        if (d.type == DEVICE_TEMP_HUM && d.hasData) {
            snprintf(buf, sizeof(buf), "%.1f\xC2\xB0  %.0f%%", d.temperature, d.humidity);
            lv_label_set_text(r.value, buf);
        } else if (d.type == DEVICE_SWITCH) {
            int on = 0;
            for (int c = 0; c < d.channels; c++) on += (d.switchMask >> c) & 1;
            snprintf(buf, sizeof(buf), "%d/%d on", on, d.channels);
            lv_label_set_text(r.value, buf);
        } else {
            lv_label_set_text(r.value, "--");
        }

        if (d.packets == 0) {
            lv_label_set_text(r.state, "Not seen yet");
            lv_obj_set_style_text_color(r.state, lv_color_hex(DV_COLOR_TEXT_MUTED), 0);
        } else {
            dv_format_age(age, sizeof(age), now - d.lastSeenMs);
            snprintf(buf, sizeof(buf), LV_SYMBOL_BULLET " %s  %s", is_online ? "Online" : "Offline", age);
            lv_label_set_text(r.state, buf);
            lv_obj_set_style_text_color(r.state, lv_color_hex(is_online ? DV_COLOR_GOOD : DV_COLOR_BAD), 0);
        }
    }

    snprintf(buf, sizeof(buf), "%d device%s  " LV_SYMBOL_BULLET "  %d online",
             reg->count(), reg->count() == 1 ? "" : "s", online);
    lv_label_set_text(objects.devices_count, buf);
}

static void dv_refresh_gateway(uint32_t now) {
    char buf[64];
    unsigned long s = now / 1000;
    snprintf(buf, sizeof(buf), "Hub %s  " LV_SYMBOL_BULLET "  Up %02lu:%02lu:%02lu",
             hub_mac, s / 3600, (s / 60) % 60, s % 60);
    lv_label_set_text(objects.gateway_info, buf);
}

//
// Public API
//

void devices_ui_init(DeviceRegistry& registry, EspNowManager& espNow, const char* hubMac) {
    reg = &registry;
    esp = &espNow;
    if (hubMac) {
        strncpy(hub_mac, hubMac, sizeof(hub_mac) - 1);
        hub_mac[sizeof(hub_mac) - 1] = '\0';
    }

    lv_obj_set_scroll_dir(objects.device_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(objects.device_list, LV_SCROLLBAR_MODE_ACTIVE);

    uint32_t now = lv_tick_get();
    dv_refresh_rows(now);
    dv_refresh_gateway(now);
    dv_refresh_indoor();
}

void devices_ui_tick() {
    static uint32_t last_refresh = 0;
    uint32_t now = lv_tick_get();

    // Pairing state can change any loop (button, AP timeout, device joined)
    if (esp->isPairing() != shown_pairing) dv_refresh_pairing();

    if (now - last_refresh < 1000) return;
    last_refresh = now;
    dv_refresh_pairing();
    dv_refresh_rows(now);
    dv_refresh_gateway(now);
}

void devices_ui_on_packet(int index) {
    const Device& d = reg->get(index);

    if (d.type == DEVICE_TEMP_HUM) {
        if (d.role == ROLE_INDOOR) {
            weather_ui_set_indoor(true, d.temperature, d.humidity);
        } else if (d.role == ROLE_OUTDOOR ||
                   (d.role == ROLE_NONE && reg->findByRole(ROLE_OUTDOOR) < 0)) {
            // Until a device is set to Outdoor, any new sensor feeds the big card
            weather_ui_set_data(d.temperature, d.humidity);
        }
    }

    dv_refresh_rows(lv_tick_get());
}

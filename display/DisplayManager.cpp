#include "DisplayManager.h"

// Global pointer (bridge for static callbacks)
static DisplayManager* instance = nullptr;

DisplayManager::DisplayManager() {
    instance = this;
}

void DisplayManager::setup()
{
    Serial.println("LVGL Init...");
    lv_init();

    // Logging
    lv_log_register_print_cb(log_print);

    // SPI
    touchscreenSpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);

    // Touch
    touchscreen.begin(touchscreenSpi);
    touchscreen.setRotation(2);

    // Display buffer
    draw_buf = new uint8_t[DRAW_BUF_SIZE];

    disp = lv_tft_espi_create(SCREEN_WIDTH, SCREEN_HEIGHT, draw_buf, DRAW_BUF_SIZE);
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_270);

    // Input device
    indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touchscreen_read);

    ui_init();

    Serial.println("Display ready");
}

void DisplayManager::update()
{
    lv_timer_handler();
    lv_tick_inc(5);
    delay(5);
}

void DisplayManager::log_print(lv_log_level_t level, const char *buf)
{
    LV_UNUSED(level);
    Serial.println(buf);
}

void DisplayManager::touchscreen_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    if (!instance) return;

    if (instance->touchscreen.tirqTouched() && instance->touchscreen.touched())
    {
        TS_Point p = instance->touchscreen.getPoint();

        instance->x = map(p.x, 200, 3700, 1, SCREEN_WIDTH);
        instance->y = map(p.y, 240, 3800, 1, SCREEN_HEIGHT);

        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = instance->x;
        data->point.y = instance->y;
    }
    else
    {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void DisplayManager::action_scan(lv_event_t * e) {
    Serial.println("Scan button pressed");

    // TODO: your logic here
}
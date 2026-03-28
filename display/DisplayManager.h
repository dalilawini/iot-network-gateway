#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <lvgl.h>
#include "ui.h"
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>
#include "actions.h"

// Pins
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320
#define DRAW_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 10 * (LV_COLOR_DEPTH / 8))

class DisplayManager {
public:
    DisplayManager();  // ✅ FIXED

    void setup();
    void update();

    // MUST be static (LVGL requirement)
    static void log_print(lv_log_level_t level, const char * buf);
    static void touchscreen_read(lv_indev_t * indev, lv_indev_data_t * data);
    void action_scan(lv_event_t * e);

private:
    SPIClass touchscreenSpi = SPIClass(VSPI);
    XPT2046_Touchscreen touchscreen = XPT2046_Touchscreen(XPT2046_CS, XPT2046_IRQ);

    uint16_t touchScreenMinimumX = 200, touchScreenMaximumX = 3700;
    uint16_t touchScreenMinimumY = 240, touchScreenMaximumY = 3800;

    int x, y, z;

    lv_indev_t *indev;
    uint8_t *draw_buf;
    lv_display_t *disp;
};

#endif
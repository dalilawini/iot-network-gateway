// TFT_eSPI configuration for this project's display
// (ESP32-2432S028R "Cheap Yellow Display": ILI9341 320x240, XPT2046 touch).
//
// TFT_eSPI reads its setup from its own library folder, so copy this file to
//   <Arduino>/libraries/TFT_eSPI/User_Setup.h
// The CI workflow (.github/workflows/build.yml) does the same.

#define USER_SETUP_INFO "iot-network-gateway"

// Driver
#define ILI9341_2_DRIVER     // Alternative ILI9341 driver, see https://github.com/Bodmer/TFT_eSPI/issues/1172
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// Backlight
#define TFT_BL   21
#define TFT_BACKLIGHT_ON HIGH

// Display SPI pins (HSPI)
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1          // reset tied to the ESP32 reset
#define USE_HSPI_PORT

// Touch chip select (the touch itself is driven by XPT2046_Touchscreen on VSPI)
#define TOUCH_CS 33

// Built-in TFT_eSPI fonts (LVGL draws its own text; kept as in the original setup)
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

// SPI speeds
#define SPI_FREQUENCY        55000000
#define SPI_READ_FREQUENCY   20000000
#define SPI_TOUCH_FREQUENCY   2500000

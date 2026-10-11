#include "EspNowManager.cpp"
#include "DeviceRegistry.cpp"
#include "sensorData.cpp"
#include "DisplayManager.cpp"
#include "images.c"
#include "screens.c"
#include "styles.c"
#include "ui.c"
#include "weather_ui.c"
#include "devices_ui.cpp"
#include "lights_ui.cpp"
#include "navigation.c"
#include "ui_image_sun2.c"


#include "EspNowManager.h"
#include "SensorData.h"
#include "DeviceRegistry.h"

#include "DisplayManager.h"

EspNowManager espManager(0);
SensorData sensorData;
DeviceRegistry devices;
DisplayManager displayManager(sensorData, devices, espManager);
void setup() {
 espManager.begin();
 devices.begin();
 displayManager.setup();
}

void loop() {
  displayManager.update();
  espManager.update(sensorData);



}
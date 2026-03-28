#include "EspNowManager.cpp"
#include "DisplayManager.cpp"
#include "images.c"
#include "screens.c"
#include "styles.c"
#include "ui.c"


#include "EspNowManager.h"
#include "DisplayManager.h"

//EspNowManager espManager(0);
DisplayManager displayManager;
void setup() {

 // espManager.begin();
 displayManager.setup();
}

void loop() {
  displayManager.update();
  //espManager.update();

}
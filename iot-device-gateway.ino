#include "EspNowManager.cpp"

#include "EspNowManager.h"

EspNowManager espManager(0);

void setup() {

  espManager.begin();

}

void loop() {

  espManager.update();

}
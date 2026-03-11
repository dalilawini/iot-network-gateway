#ifndef ESPNOWMANAGER_H
#define ESPNOWMANAGER_H

#include <ESP8266WiFi.h>

extern "C" {
  #include <espnow.h>
}

class EspNowManager {

  public:
    EspNowManager(int buttonPin);

    void begin();
    void update();

  private:
    int _buttonPin;

    bool apMode = false;
    unsigned long apStartTime = 0;
    const unsigned long apDuration = 60000;

    int LED_PIN = 2;          // GPIO2 / D4 on WeMos D1 Mini
    int INTERVAL = 500;      // Toggle interval in milliseconds

    unsigned long previousMillis = 0;  // Store last toggle time
    bool ledState = false;             // Current LED state

    static void onStationConnected(const WiFiEventSoftAPModeStationConnected& evt);
    static void onDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len);

    void startAP();
    void startEspNow();
    void toggleLed();
};

#endif
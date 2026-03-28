#ifndef ESPNOWMANAGER_H
#define ESPNOWMANAGER_H

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h> 
#include <ArduinoJson.h>
#define MAX_JSON_SIZE 128

class EspNowManager {

  public:
    EspNowManager(int buttonPin);
    static EspNowManager* instance;

    void begin();
    void update();

  private:
    int _buttonPin;

    bool apMode = false;
    unsigned long apStartTime = 0;
    const unsigned long apDuration = 60000;

    int LED_PIN = 17;          // GPIO2 / D4 on WeMos D1 Mini
    int INTERVAL = 500;      // Toggle interval in milliseconds

    unsigned long previousMillis = 0;  // Store last toggle time
    bool ledState = false;             // Current LED state

    static uint8_t json[MAX_JSON_SIZE];
    static uint8_t dataLen;
    static bool dataReady;

    static void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info);
    static void onDataRecv(const esp_now_recv_info *info, const uint8_t *data, int len);

    void startAP();
    void startEspNow();
    void toggleLed();

};

#endif
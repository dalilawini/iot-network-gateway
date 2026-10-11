#ifndef ESPNOWMANAGER_H
#define ESPNOWMANAGER_H

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h> 
#include "SensorData.h"

#define MAX_JSON_SIZE 128

class EspNowManager {

  public:
    EspNowManager(int buttonPin);
    static EspNowManager* instance;

    void begin();
    void update(SensorData& sensorData);

    // Pairing from the UI: runs the same startAP() as the physical button,
    // but from update() (never inside an LVGL callback).
    void requestPairing() { pairRequested = true; }
    bool isPairing() const { return apMode || pairRequested; }
    uint32_t pairingTimeLeft() const;

    // Send a JSON command to a device (e.g. a light switch). Returns false if it
    // could not be queued (hub is pairing, peer error...). The delivery result
    // (device acknowledged or not) arrives later: see lastSendResult().
    enum SendResult : uint8_t { SEND_NONE, SEND_PENDING, SEND_OK, SEND_FAILED };
    bool sendJson(const uint8_t mac[6], const char* json);
    SendResult lastSendResult() const { return (SendResult)sendResult; }

  private:
    int _buttonPin;

    bool apMode = false;
    unsigned long apStartTime = 0;
    const unsigned long apDuration = 60000;

    int LED_PIN = 17;          // GPIO2 / D4 on WeMos D1 Mini
    int INTERVAL = 500;      // Toggle interval in milliseconds

    unsigned long previousMillis = 0;  // Store last toggle time
    bool ledState = false;             // Current LED state

    volatile bool pairRequested = false;

    static volatile uint8_t sendResult;

    static uint8_t lastMac[6];   // sender of the latest packet
    static int8_t lastRssi;

    static uint8_t json[MAX_JSON_SIZE];
    static uint8_t dataLen;
    static bool dataReady;

    static void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info);
    static void onDataRecv(const esp_now_recv_info *info, const uint8_t *data, int len);
    static void onDataSent(const esp_now_send_info_t *info, esp_now_send_status_t status);

    void startAP();
    void startEspNow();
    void toggleLed();

};

#endif
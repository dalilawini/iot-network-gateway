#ifndef SENSOR_DATA_H
#define SENSOR_DATA_H

#include <Arduino.h>

class SensorData
{
public:
    bool setData(const uint8_t* data, size_t len, bool dataReady);
    void setSource(const uint8_t mac[6], int8_t rssi);

    float getTemperature() const;
    float getHumidity() const;
    uint32_t getKey() const;
    bool isDataReady() const;
    bool hasTempHum() const;
    uint8_t getSwitchChannels() const;   // > 0 when the packet is a switch announcement
    const uint8_t* getMac() const;
    int8_t getRssi() const;

private:
    float temperature = 0.0f;
    float humidity = 0.0f;
    uint32_t key = 0;
    bool dataReady=false;
    bool tempHum = false;
    uint8_t switchChannels = 0;
    uint8_t mac[6] = {0};
    int8_t rssi = 0;
};

#endif
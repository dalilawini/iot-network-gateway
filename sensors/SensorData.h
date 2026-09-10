#ifndef SENSOR_DATA_H
#define SENSOR_DATA_H

#include <Arduino.h>

class SensorData
{
public:
    bool setData(const uint8_t* data, size_t len);

    float getTemperature() const;
    float getHumidity() const;
    uint32_t getKey() const;

private:
    float temperature = 0.0f;
    float humidity = 0.0f;
    uint32_t key = 0;
};

#endif
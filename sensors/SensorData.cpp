#include "SensorData.h"
#include <ArduinoJson.h>

bool SensorData::setData(const uint8_t* data, size_t len)
{
    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, data, len);

    if (error)
    {
        return false;
    }

    temperature = doc["Tmp"] | 0.0f;
    humidity    = doc["Hum"] | 0.0f;
    key         = doc["key"] | 0;

    return true;
}

float SensorData::getTemperature() const
{
    return temperature;
}

float SensorData::getHumidity() const
{
    return humidity;
}

uint32_t SensorData::getKey() const
{
    return key;
}
#include "SensorData.h"
#include <ArduinoJson.h>

bool SensorData::setData(const uint8_t* data, size_t len,bool dataReady)
{
    JsonDocument doc;
    SensorData::dataReady = dataReady;
    DeserializationError error = deserializeJson(doc, data, len);
    
    if (error)
    {
        SensorData::dataReady = false;   // don't publish a packet we couldn't read
        return false;
    }

    tempHum = doc["Tmp"].is<float>() && doc["Hum"].is<float>();

    // Light switch announcement: {"type":"switch","ch":3}
    switchChannels = 0;
    if (doc["type"] == "switch") {
        switchChannels = doc["ch"] | 1;
        if (switchChannels < 1) switchChannels = 1;
        if (switchChannels > 8) switchChannels = 8;
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

bool SensorData::isDataReady() const
{
    return dataReady;
}


uint32_t SensorData::getKey() const
{
    return key;
}

void SensorData::setSource(const uint8_t mac[6], int8_t rssi)
{
    memcpy(this->mac, mac, 6);
    this->rssi = rssi;
}

bool SensorData::hasTempHum() const
{
    return tempHum;
}

uint8_t SensorData::getSwitchChannels() const
{
    return switchChannels;
}

const uint8_t* SensorData::getMac() const
{
    return mac;
}

int8_t SensorData::getRssi() const
{
    return rssi;
}

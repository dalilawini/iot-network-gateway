#include "DeviceRegistry.h"
#include <Preferences.h>

static const char* NVS_NAMESPACE = "devreg";

// NVS key holding a switch device's channel count and state: "s" + MAC in hex
static void switchKey(const uint8_t mac[6], char* key)
{
    snprintf(key, 16, "s%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// Layout stored in NVS (one entry per device)
struct StoredDevice {
    uint8_t mac[6];
    uint8_t role;
    uint8_t type;
};

void DeviceRegistry::begin()
{
    StoredDevice stored[MAX_DEVICES];
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true);
    size_t bytes = prefs.getBytes("devices", stored, sizeof(stored));
    prefs.end();

    _count = 0;
    for (size_t i = 0; i < bytes / sizeof(StoredDevice) && _count < MAX_DEVICES; i++) {
        Device& d = _devices[_count++];
        memset(&d, 0, sizeof(d));
        memcpy(d.mac, stored[i].mac, 6);
        d.role = stored[i].role < ROLE_COUNT ? stored[i].role : ROLE_NONE;
        d.type = stored[i].type;
        if (d.type == DEVICE_SWITCH) loadSwitch(_count - 1);
    }
    _revision++;
    Serial.printf("DeviceRegistry: %d device(s) loaded\n", _count);
}

void DeviceRegistry::save()
{
    StoredDevice stored[MAX_DEVICES];
    for (int i = 0; i < _count; i++) {
        memcpy(stored[i].mac, _devices[i].mac, 6);
        stored[i].role = _devices[i].role;
        stored[i].type = _devices[i].type;
    }
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    if (_count > 0) prefs.putBytes("devices", stored, _count * sizeof(StoredDevice));
    else            prefs.remove("devices");
    prefs.end();
}

void DeviceRegistry::saveSwitch(int index)
{
    char key[16];
    const Device& d = _devices[index];
    uint8_t value[2] = { d.channels, d.switchMask };
    switchKey(d.mac, key);
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putBytes(key, value, sizeof(value));
    prefs.end();
}

void DeviceRegistry::loadSwitch(int index)
{
    char key[16];
    Device& d = _devices[index];
    uint8_t value[2] = { 0, 0 };
    switchKey(d.mac, key);
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true);
    if (prefs.isKey(key)) prefs.getBytes(key, value, sizeof(value));
    prefs.end();
    d.channels = value[0];
    d.switchMask = value[1];
}

void DeviceRegistry::setSwitchMask(int index, uint8_t mask)
{
    if (index < 0 || index >= _count) return;
    _devices[index].switchMask = mask;
    saveSwitch(index);
}

int DeviceRegistry::find(const uint8_t mac[6]) const
{
    for (int i = 0; i < _count; i++) {
        if (memcmp(_devices[i].mac, mac, 6) == 0) return i;
    }
    return -1;
}

int DeviceRegistry::findByRole(DeviceRole role) const
{
    for (int i = 0; i < _count; i++) {
        if (_devices[i].role == role) return i;
    }
    return -1;
}

bool DeviceRegistry::isOnline(int index, uint32_t nowMs) const
{
    const Device& d = _devices[index];
    return d.packets > 0 && nowMs - d.lastSeenMs < OFFLINE_MS;
}

int DeviceRegistry::onPacket(const uint8_t mac[6], int8_t rssi, DeviceType type,
                             float temperature, float humidity, uint8_t channels)
{
    int index = find(mac);
    bool changed = false;

    if (index < 0) {
        if (_count >= MAX_DEVICES) {
            Serial.println("DeviceRegistry: full, packet ignored");
            return -1;
        }
        index = _count++;
        Device& d = _devices[index];
        memset(&d, 0, sizeof(d));
        memcpy(d.mac, mac, 6);
        d.role = ROLE_NONE;
        changed = true;
        Serial.println("DeviceRegistry: new device");
    }

    Device& d = _devices[index];
    if (d.type != type) {
        d.type = type;
        changed = true;
    }
    d.rssi = rssi;
    d.lastSeenMs = millis();
    d.packets++;
    if (type == DEVICE_TEMP_HUM) {
        d.hasData = true;
        d.temperature = temperature;
        d.humidity = humidity;
    }
    if (type == DEVICE_SWITCH && d.channels != channels) {
        d.channels = channels;
        saveSwitch(index);
        changed = true;
    }

    if (changed) {
        save();
        _revision++;
    }
    return index;
}

void DeviceRegistry::setRole(int index, DeviceRole role)
{
    if (index < 0 || index >= _count || role >= ROLE_COUNT) return;
    if (role == ROLE_OUTDOOR || role == ROLE_INDOOR) {
        for (int i = 0; i < _count; i++) {
            if (i != index && _devices[i].role == role) _devices[i].role = ROLE_NONE;
        }
    }
    _devices[index].role = role;
    save();
    _revision++;
}

void DeviceRegistry::forget(int index)
{
    if (index < 0 || index >= _count) return;
    if (_devices[index].type == DEVICE_SWITCH) {
        char key[16];
        switchKey(_devices[index].mac, key);
        Preferences prefs;
        prefs.begin(NVS_NAMESPACE, false);
        prefs.remove(key);
        prefs.end();
    }
    for (int i = index; i < _count - 1; i++) _devices[i] = _devices[i + 1];
    _count--;
    save();
    _revision++;
}

const char* DeviceRegistry::roleName(uint8_t role)
{
    static const char* names[ROLE_COUNT] = {
        "New device", "Outdoor", "Indoor", "Living", "Bedroom", "Kitchen", "Garage", "Office", "Other"
    };
    return role < ROLE_COUNT ? names[role] : "Device";
}

const char* DeviceRegistry::typeName(uint8_t type)
{
    switch (type) {
    case DEVICE_TEMP_HUM: return "Temp & Humidity";
    case DEVICE_SWITCH:   return "Light switch";
    default:              return "Unknown";
    }
}

void DeviceRegistry::formatMac(const uint8_t mac[6], char* out, size_t len)
{
    snprintf(out, len, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

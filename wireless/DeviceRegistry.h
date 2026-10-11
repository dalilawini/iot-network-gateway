#ifndef DEVICE_REGISTRY_H
#define DEVICE_REGISTRY_H

#include <Arduino.h>

// Roles a device can be given from the Devices screen. The order matches the
// role buttons in the EEZ project (role_btn_1 .. role_btn_8 -> ROLE_OUTDOOR ..).
enum DeviceRole : uint8_t {
    ROLE_NONE = 0,
    ROLE_OUTDOOR,
    ROLE_INDOOR,
    ROLE_LIVING,
    ROLE_BEDROOM,
    ROLE_KITCHEN,
    ROLE_GARAGE,
    ROLE_OFFICE,
    ROLE_OTHER,
    ROLE_COUNT
};

// What a device sends. Add new kinds here (e.g. a light switch) as they come.
enum DeviceType : uint8_t {
    DEVICE_UNKNOWN = 0,
    DEVICE_TEMP_HUM,
    DEVICE_SWITCH,      // relay board, see docs/espnow-protocol.md
};

struct Device {
    // Saved in flash
    uint8_t mac[6];
    uint8_t role;
    uint8_t type;

    // Live (since boot)
    int8_t rssi;
    uint32_t lastSeenMs;
    uint32_t packets;
    bool hasData;
    float temperature;
    float humidity;

    // DEVICE_SWITCH: channel count and last commanded state (bit n = channel n+1).
    // Saved in flash so the screen still shows it after a hub reboot.
    uint8_t channels;
    uint8_t switchMask;
};

// Remembers every ESP-NOW device that has talked to the hub (by MAC address),
// its role and its latest values. Roles survive reboots (NVS / Preferences).
class DeviceRegistry {
public:
    static const int MAX_DEVICES = 10;
    static const uint32_t OFFLINE_MS = 300000UL;  // no packet for 5 min -> offline

    void begin();

    // Record a packet. Unknown MACs are added as new devices (ROLE_NONE).
    // Returns the device index, or -1 if the registry is full.
    int onPacket(const uint8_t mac[6], int8_t rssi, DeviceType type, float temperature, float humidity,
                 uint8_t channels = 0);

    int count() const { return _count; }
    const Device& get(int index) const { return _devices[index]; }
    int find(const uint8_t mac[6]) const;
    int findByRole(DeviceRole role) const;
    bool isOnline(int index, uint32_t nowMs) const;

    // Outdoor and Indoor are unique: giving one to a device takes it from the other.
    void setRole(int index, DeviceRole role);
    void forget(int index);

    // Remember the state last sent to a switch device
    void setSwitchMask(int index, uint8_t mask);

    // Increments whenever devices are added, removed or renamed (UI rebuilds its list).
    uint32_t revision() const { return _revision; }

    static const char* roleName(uint8_t role);
    static const char* typeName(uint8_t type);
    static void formatMac(const uint8_t mac[6], char* out, size_t len);

private:
    Device _devices[MAX_DEVICES];
    int _count = 0;
    uint32_t _revision = 0;

    void save();
    void saveSwitch(int index);
    void loadSwitch(int index);
};

#endif

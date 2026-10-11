# ESP-NOW protocol (hub ⇄ devices)

All messages are JSON text in one ESP-NOW packet, **at most 127 bytes**.
Pairing is unchanged: the device joins the hub's `ESP-NOW-GATEWAY` access point,
learns the hub's MAC, and from then on talks ESP-NOW on **channel 1**.

The hub identifies every device by its **MAC address**, so a device only has to
send something once to show up on the hub's **Devices** screen.

## Temperature / humidity sensor → hub

```json
{"Tmp": 23.4, "Hum": 56, "key": 1}
```

Send periodically. `key` is optional.

## Light switch (relay board)

### Device → hub: announce

```json
{"type": "switch", "ch": 3}
```

- `ch` is the number of relay channels (1–8; the Lights screen shows the first 3).
- Send it **once at boot**, then **every 60 s** as a heartbeat. The hub shows a
  device as *offline* after 5 minutes without any packet.

### Hub → device: command

```json
{"cmd": "set", "ch": 2, "on": true}
```

| field | value |
|-------|-------|
| `cmd` | always `"set"` |
| `ch`  | `1`–`3` = that channel, `0` = **all** channels |
| `on`  | `true` = relay on, `false` = relay off |

The device does **not** need to reply. The hub uses the ESP-NOW delivery report
(the radio-level acknowledgement) to know whether the board received the
command; if not, the Lights screen reverts the button and shows
"Light switch did not answer".

### Minimal receive handler for the lamp board

```cpp
#include <ArduinoJson.h>

const int RELAY_PINS[3] = {16, 17, 18};   // adapt to your board
const bool RELAY_ACTIVE_LOW = false;      // true for most relay modules with opto-couplers

void setRelay(int ch, bool on) {          // ch: 1..3
  digitalWrite(RELAY_PINS[ch - 1], (on != RELAY_ACTIVE_LOW) ? HIGH : LOW);
}

void onDataRecv(const esp_now_recv_info *info, const uint8_t *data, int len) {
  JsonDocument doc;
  if (deserializeJson(doc, data, len)) return;
  if (doc["cmd"] != "set") return;

  int ch = doc["ch"] | -1;
  bool on = doc["on"] | false;
  if (ch == 0) {
    for (int i = 1; i <= 3; i++) setRelay(i, on);
  } else if (ch >= 1 && ch <= 3) {
    setRelay(ch, on);
  }
}
```

Register it with `esp_now_register_recv_cb(onDataRecv);` after `esp_now_init()`,
and send the announce to the hub's MAC with `esp_now_send()`.

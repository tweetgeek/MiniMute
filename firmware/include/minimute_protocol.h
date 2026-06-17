#pragma once
#include <cstdint>
// MiniMute BLE/GATT contract. Single source of truth shared by firmware and the
// companion app. Working UUIDs — freeze before first external release.

#define MM_SERVICE_UUID        "6E6D0001-B5A3-F393-E0A9-E50E24DCCA9E"
#define MM_CHAR_BUTTON_UUID    "6E6D0002-B5A3-F393-E0A9-E50E24DCCA9E"  // Notify  (device->app)
#define MM_CHAR_INDICATOR_UUID "6E6D0003-B5A3-F393-E0A9-E50E24DCCA9E"  // Write   (app->device)
#define MM_CHAR_STATUS_UUID    "6E6D0004-B5A3-F393-E0A9-E50E24DCCA9E"  // Read+Notify (device->app)
#define MM_CHAR_CONTROL_UUID   "6E6D0005-B5A3-F393-E0A9-E50E24DCCA9E"  // Write   (app->device)

// Button Event char, byte[0] = code, byte[1] = sequence number.
enum mm_button_code : uint8_t {
  MM_BTN_SHORT  = 0x01,
  MM_BTN_LONG   = 0x02,
  MM_BTN_DOUBLE = 0x03,  // reserved; not emitted in v1
};

// Indicator State char, byte[0] = flags (app -> device).
enum mm_indicator_flags : uint8_t {
  MM_IND_MUTED      = 1u << 0,
  MM_IND_CAMERA_OFF = 1u << 1,
  MM_IND_IN_CALL    = 1u << 2,
};

// Device Status char, byte[0] = flags (device -> app). power_state in bits 2-3.
enum mm_status_flags : uint8_t {
  MM_ST_CHARGING    = 1u << 0,
  MM_ST_LOW_BATTERY = 1u << 1,
  MM_ST_POWER_SHIFT = 2,
  MM_ST_POWER_MASK  = 0x0Cu,
};
// power_state values: 0 = active, 1 = connected-idle.

// Device Control char, byte[0] = command (app -> device).
enum mm_control_cmd : uint8_t {
  MM_CTL_IDENTIFY    = 0x01,
  MM_CTL_FORGET_BOND = 0x02,
};

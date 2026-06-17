#pragma once
#include "LedPattern.h"

namespace mm {

struct LedInputs {
  bool connected       = false;  // BLE link up
  bool inCall          = false;  // app: in_call flag
  bool muted           = false;  // app: muted flag
  bool charging        = false;  // charger active
  bool charged         = false;  // VBUS present and charge complete
  bool lowBattery      = false;  // gauge low (informational)
  bool lowBatteryFlash = false;  // caller-gated brief warning window
  bool identify        = false;  // IDENTIFY command active
  bool deepSleep       = false;  // entering/in deep sleep
};

LedPattern resolveLed(const LedInputs& in);

}  // namespace mm

#pragma once
#include <cstdint>
#include "ButtonEvent.h"

namespace mm {

struct IndicatorState {
  bool muted     = false;
  bool cameraOff = false;
  bool inCall    = false;
};

// Maps an internal event to its wire code. Returns false for events that are not
// sent to the app (None, VeryLong).
bool encodeButtonEvent(ButtonEvent e, uint8_t& outByte);

// Decodes the Indicator State flags byte received from the app.
IndicatorState decodeIndicator(uint8_t flags);

// Builds the Device Status flags byte. powerState: 0 active, 1 connected-idle.
uint8_t encodeStatus(bool charging, bool lowBattery, uint8_t powerState);

}  // namespace mm

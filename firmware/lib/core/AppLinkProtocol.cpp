#include "AppLinkProtocol.h"
#include "minimute_protocol.h"

namespace mm {

bool encodeButtonEvent(ButtonEvent e, uint8_t& outByte) {
  switch (e) {
    case ButtonEvent::Short:  outByte = MM_BTN_SHORT;  return true;
    case ButtonEvent::Long:   outByte = MM_BTN_LONG;   return true;
    case ButtonEvent::Double: outByte = MM_BTN_DOUBLE; return true;
    default:                  return false;  // None, VeryLong are local-only
  }
}

IndicatorState decodeIndicator(uint8_t flags) {
  IndicatorState s;
  s.muted     = flags & MM_IND_MUTED;
  s.cameraOff = flags & MM_IND_CAMERA_OFF;
  s.inCall    = flags & MM_IND_IN_CALL;
  return s;
}

uint8_t encodeStatus(bool charging, bool lowBattery, uint8_t powerState) {
  uint8_t b = 0;
  if (charging)   b |= MM_ST_CHARGING;
  if (lowBattery) b |= MM_ST_LOW_BATTERY;
  b |= (powerState << MM_ST_POWER_SHIFT) & MM_ST_POWER_MASK;
  return b;
}

}  // namespace mm

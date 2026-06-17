#include "LedStateResolver.h"

namespace mm {

LedPattern resolveLed(const LedInputs& in) {
  if (in.deepSleep)        return {LedColor::Off,   LedMode::Solid};
  if (in.identify)         return {LedColor::White, LedMode::FastBlink};
  if (!in.connected)       return {LedColor::Blue,  LedMode::SlowBlink};
  if (in.lowBatteryFlash)  return {LedColor::Red,   LedMode::Solid};   // brief overlay window
  if (in.inCall)           return in.muted ? LedPattern{LedColor::Red,   LedMode::Solid}
                                           : LedPattern{LedColor::Green, LedMode::Solid};
  if (in.charging)         return {LedColor::Amber, LedMode::Breathe};
  if (in.charged)          return {LedColor::Green, LedMode::Solid};
  return {LedColor::Off, LedMode::Solid};  // idle on battery
}

}  // namespace mm

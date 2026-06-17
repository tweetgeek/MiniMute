#pragma once
#include <cstdint>
#include "LedPattern.h"

namespace mm {

struct IButton {
  virtual ~IButton() = default;
  virtual bool isPressed() = 0;          // true while physically pressed
};

struct IRgbLed {
  virtual ~IRgbLed() = default;
  virtual void show(const LedPattern& p, uint32_t nowMs) = 0;  // render incl. blink/breathe
};

struct IBatterySensor {
  virtual ~IBatterySensor() = default;
  virtual uint16_t readMilliVolts() = 0;
  virtual bool charging() = 0;
  virtual bool vbusPresent() = 0;
};

}  // namespace mm

#pragma once
#include <Arduino.h>
#include "ports.h"

namespace mm {

// Drives a common-anode (active-LOW) RGB LED on three PWM-capable pins.
class NrfRgbLed : public IRgbLed {
 public:
  NrfRgbLed(uint32_t rPin, uint32_t gPin, uint32_t bPin, bool activeLow = true)
      : rPin_(rPin), gPin_(gPin), bPin_(bPin), activeLow_(activeLow) {}
  void begin();
  void show(const LedPattern& p, uint32_t nowMs) override;

 private:
  void writeRgb(uint8_t r, uint8_t g, uint8_t b);   // 0..255 logical brightness
  uint32_t rPin_, gPin_, bPin_;
  bool activeLow_;
};

}  // namespace mm

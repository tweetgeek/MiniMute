#pragma once
#include <Arduino.h>
#include "ports.h"

namespace mm {

class NrfButton : public IButton {
 public:
  explicit NrfButton(uint32_t pin) : pin_(pin) {}
  void begin() { pinMode(pin_, INPUT_PULLUP); }
  bool isPressed() override { return digitalRead(pin_) == LOW; }  // active-low to GND
  uint32_t pin() const { return pin_; }

 private:
  uint32_t pin_;
};
}  // namespace mm

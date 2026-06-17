#pragma once
#include <Arduino.h>
#include "ports.h"

namespace mm {

class NrfBatterySensor : public IBatterySensor {
 public:
  void begin();
  uint16_t readMilliVolts() override;
  bool charging() override;
  bool vbusPresent() override;
};
}  // namespace mm

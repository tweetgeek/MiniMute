#pragma once
#include <cstdint>

namespace mm {

class BatteryGauge {
 public:
  struct Config {
    uint16_t lowEnterMv = 3500;  // assert low below this
    uint16_t lowExitMv  = 3650;  // deassert low above this (hysteresis)
    float    emaAlpha   = 0.2f;
  };

  // Two out-of-line constructors (see toolchain note); defined in .cpp.
  BatteryGauge();
  explicit BatteryGauge(const Config& cfg);

  void addSample(uint16_t milliVolts);
  uint16_t smoothedMilliVolts() const { return smoothed_; }
  uint8_t percent() const;
  bool lowBattery() const { return low_; }
  bool hasSample() const { return seeded_; }

 private:
  Config   cfg_;
  bool     seeded_   = false;
  bool     low_      = false;
  float    smoothed_ = 0.0f;
};

}  // namespace mm

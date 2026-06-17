#include "BatteryGauge.h"

namespace mm {

BatteryGauge::BatteryGauge() : cfg_() {}

BatteryGauge::BatteryGauge(const Config& cfg) : cfg_(cfg) {}

namespace {
struct Point { uint16_t mv; uint8_t pct; };
// LiPo discharge curve (descending mV). Interpolated linearly between points.
constexpr Point kCurve[] = {
  {4200, 100}, {4100, 90}, {4000, 80}, {3900, 70}, {3800, 60},
  {3700, 45},  {3600, 30}, {3500, 15}, {3400, 5},  {3300, 0},
};
constexpr int kN = sizeof(kCurve) / sizeof(kCurve[0]);
}  // namespace

void BatteryGauge::addSample(uint16_t milliVolts) {
  if (!seeded_) {
    smoothed_ = static_cast<float>(milliVolts);
    seeded_ = true;
  } else {
    smoothed_ = cfg_.emaAlpha * milliVolts + (1.0f - cfg_.emaAlpha) * smoothed_;
  }
  uint16_t mv = smoothedMilliVolts();
  if (mv < cfg_.lowEnterMv) low_ = true;
  else if (mv > cfg_.lowExitMv) low_ = false;
}

uint8_t BatteryGauge::percent() const {
  uint16_t mv = smoothedMilliVolts();
  if (mv >= kCurve[0].mv) return 100;
  if (mv <= kCurve[kN - 1].mv) return 0;
  for (int i = 0; i < kN - 1; ++i) {
    const Point& hi = kCurve[i];
    const Point& lo = kCurve[i + 1];
    if (mv <= hi.mv && mv >= lo.mv) {
      float frac = static_cast<float>(mv - lo.mv) / (hi.mv - lo.mv);
      return static_cast<uint8_t>(lo.pct + frac * (hi.pct - lo.pct) + 0.5f);
    }
  }
  return 0;
}

}  // namespace mm

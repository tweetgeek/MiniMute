#include "NrfRgbLed.h"

namespace mm {

namespace {
constexpr uint8_t kBrightness = 70;     // ~27% of 255, power/comfort cap

struct Rgb { uint8_t r, g, b; };
Rgb colorToRgb(LedColor c) {
  switch (c) {
    case LedColor::Red:   return {kBrightness, 0, 0};
    case LedColor::Green: return {0, kBrightness, 0};
    case LedColor::Blue:  return {0, 0, kBrightness};
    case LedColor::Amber: return {kBrightness, (uint8_t)(kBrightness / 2), 0};
    case LedColor::White: return {kBrightness, kBrightness, kBrightness};
    case LedColor::Off:
    default:              return {0, 0, 0};
  }
}

// Returns a 0..255 intensity multiplier for the mode at time nowMs.
uint8_t modeScale(LedMode m, uint32_t nowMs) {
  switch (m) {
    case LedMode::Solid:     return 255;
    case LedMode::SlowBlink: return (nowMs % 1000) < 500 ? 255 : 0;     // 1 Hz
    case LedMode::FastBlink: return (nowMs % 200)  < 100 ? 255 : 0;     // 5 Hz
    case LedMode::Breathe: {
      uint32_t t = nowMs % 2000;                                       // triangle 0..255..0
      uint32_t up = t < 1000 ? t : (2000 - t);
      return static_cast<uint8_t>((up * 255) / 1000);
    }
  }
  return 255;
}
}  // namespace

void NrfRgbLed::begin() {
  pinMode(rPin_, OUTPUT);
  pinMode(gPin_, OUTPUT);
  pinMode(bPin_, OUTPUT);
  writeRgb(0, 0, 0);
}

void NrfRgbLed::writeRgb(uint8_t r, uint8_t g, uint8_t b) {
  if (activeLow_) { r = 255 - r; g = 255 - g; b = 255 - b; }
  analogWrite(rPin_, r);
  analogWrite(gPin_, g);
  analogWrite(bPin_, b);
}

void NrfRgbLed::show(const LedPattern& p, uint32_t nowMs) {
  Rgb base = colorToRgb(p.color);
  uint16_t s = modeScale(p.mode, nowMs);
  writeRgb((base.r * s) / 255, (base.g * s) / 255, (base.b * s) / 255);
}

}  // namespace mm

#pragma once
#include <cstdint>

namespace mm {

enum class LedColor : uint8_t { Off, Red, Green, Blue, Amber, White };
enum class LedMode  : uint8_t { Solid, SlowBlink, FastBlink, Breathe };

struct LedPattern {
  LedColor color = LedColor::Off;
  LedMode  mode  = LedMode::Solid;

  bool operator==(const LedPattern& o) const { return color == o.color && mode == o.mode; }
  bool operator!=(const LedPattern& o) const { return !(*this == o); }
};

}  // namespace mm

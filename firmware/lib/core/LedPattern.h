#pragma once
#include <cstdint>

namespace mm {

enum class LedColor : uint8_t { Off, Red, Green, Blue, Amber, White };
enum class LedMode  : uint8_t { Solid, SlowBlink, FastBlink, Breathe };

struct LedPattern {
  LedColor color = LedColor::Off;
  LedMode  mode  = LedMode::Solid;

  // Default-constructible and brace-init under gnu++11 (gcc 7.2 on nRF52 toolchain).
  constexpr LedPattern() = default;
  constexpr LedPattern(LedColor c, LedMode m) : color(c), mode(m) {}

  bool operator==(const LedPattern& o) const { return color == o.color && mode == o.mode; }
  bool operator!=(const LedPattern& o) const { return !(*this == o); }
};

}  // namespace mm

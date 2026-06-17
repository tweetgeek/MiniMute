#pragma once
#include <cstdint>
#include "ButtonEvent.h"

namespace mm {

class GestureRecognizer {
 public:
  struct Config {
    uint32_t debounceMs = 20;
    uint32_t longMs     = 600;
    uint32_t veryLongMs = 3000;
  };

  // Two constructors instead of a `Config cfg = {}` default arg: Clang rejects a
  // `{}` default argument whose nested-type (Config) default member initializers
  // are needed within the enclosing class definition. Defined out-of-line in .cpp.
  GestureRecognizer();
  explicit GestureRecognizer(const Config& cfg);

  // Feed raw button level (true = pressed) at time `now` (ms). Call frequently.
  // Returns the event produced on this call, or None.
  ButtonEvent update(bool rawPressed, uint32_t now);

 private:
  enum class State : uint8_t { Idle, Down, VeryLongLatched };

  Config   cfg_;
  bool     stable_      = false;
  bool     candidate_   = false;
  uint32_t candAt_      = 0;
  State    state_       = State::Idle;
  uint32_t pressStart_  = 0;
};

}  // namespace mm

#pragma once
#include <cstdint>

namespace mm {

enum class ButtonEvent : uint8_t {
  None     = 0,
  Short    = 1,
  Long     = 2,
  Double   = 3,  // reserved; not emitted in v1
  VeryLong = 4,  // local-only (triggers re-pair); never sent to app
};

}  // namespace mm

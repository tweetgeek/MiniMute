#include "GestureRecognizer.h"

namespace mm {

GestureRecognizer::GestureRecognizer() : cfg_() {}

GestureRecognizer::GestureRecognizer(const Config& cfg) : cfg_(cfg) {}

ButtonEvent GestureRecognizer::update(bool rawPressed, uint32_t now) {
  // --- debounce: commit a level only after it is stable for debounceMs ---
  if (rawPressed != candidate_) {
    candidate_ = rawPressed;
    candAt_ = now;
  }
  if (candidate_ != stable_ && (now - candAt_) >= cfg_.debounceMs) {
    stable_ = candidate_;
    if (stable_) {                       // committed press (rising edge)
      state_ = State::Down;
      pressStart_ = now;
      return ButtonEvent::None;
    }
    // committed release (falling edge)
    if (state_ == State::Down) {
      uint32_t held = now - pressStart_;
      state_ = State::Idle;
      if (held >= cfg_.veryLongMs) return ButtonEvent::VeryLong;
      if (held >= cfg_.longMs)     return ButtonEvent::Long;
      return ButtonEvent::Short;
    }
    state_ = State::Idle;                 // release after VeryLong already latched
    return ButtonEvent::None;
  }

  // --- held timing: latch VeryLong immediately for responsive feedback ---
  if (state_ == State::Down && (now - pressStart_) >= cfg_.veryLongMs) {
    state_ = State::VeryLongLatched;
    return ButtonEvent::VeryLong;
  }
  return ButtonEvent::None;
}

}  // namespace mm

#pragma once
#include <cstdint>

namespace mm {

enum class PowerState : uint8_t { Active, ConnectedIdle, DeepSleep };

class PowerManager {
 public:
  struct Config {
    uint32_t activeTimeoutMs       = 45000;    // -> ConnectedIdle
    uint32_t deepSleepTimeoutMs    = 1800000;  // 30 min total inactivity (connected)
    uint32_t disconnectedTimeoutMs = 180000;   // 3 min disconnected -> DeepSleep
  };

  // Two out-of-line constructors (see toolchain note); defined in .cpp.
  PowerManager();
  explicit PowerManager(const Config& cfg);

  void noteActivity(uint32_t now);
  void setConnected(bool connected, uint32_t now);
  void setInCall(bool inCall) { inCall_ = inCall; }

  PowerState update(uint32_t now);
  PowerState state() const { return state_; }

 private:
  Config     cfg_;
  PowerState state_         = PowerState::Active;
  bool       connected_     = false;
  bool       inCall_        = false;
  uint32_t   lastActivity_  = 0;
  uint32_t   disconnectAt_  = 0;
};

}  // namespace mm

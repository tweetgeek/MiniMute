#include "PowerManager.h"

namespace mm {

PowerManager::PowerManager() : cfg_() {}

PowerManager::PowerManager(const Config& cfg) : cfg_(cfg) {}

void PowerManager::noteActivity(uint32_t now) {
  lastActivity_ = now;
  if (state_ != PowerState::Active) state_ = PowerState::Active;
}

void PowerManager::setConnected(bool connected, uint32_t now) {
  if (connected && !connected_) {        // just connected counts as activity
    lastActivity_ = now;
    state_ = PowerState::Active;
  }
  if (!connected && connected_) {        // just disconnected
    disconnectAt_ = now;
  }
  connected_ = connected;
}

PowerState PowerManager::update(uint32_t now) {
  if (!connected_) {
    if (!inCall_ && (now - disconnectAt_) >= cfg_.disconnectedTimeoutMs) {
      state_ = PowerState::DeepSleep;
    } else {
      state_ = PowerState::Active;        // advertising / searching
    }
    return state_;
  }

  uint32_t idle = now - lastActivity_;
  if (!inCall_ && idle >= cfg_.deepSleepTimeoutMs) {
    state_ = PowerState::DeepSleep;
  } else if (idle >= cfg_.activeTimeoutMs) {
    state_ = PowerState::ConnectedIdle;
  } else {
    state_ = PowerState::Active;
  }
  return state_;
}

}  // namespace mm

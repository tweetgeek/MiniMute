#pragma once
#include <bluefruit.h>
#include <cstdint>
#include "AppLinkProtocol.h"

namespace mm {

// Callbacks into the application layer (set by main).
struct BleCallbacks {
  void (*onIndicator)(IndicatorState) = nullptr;  // app wrote Indicator State
  void (*onIdentify)() = nullptr;                  // Control: IDENTIFY
  void (*onForgetBond)() = nullptr;                // Control: FORGET_BOND
  void (*onConnected)(bool) = nullptr;             // connect/disconnect
};

class BleLink {
 public:
  void begin(const char* deviceName, const BleCallbacks& cb);
  void startAdvertising();

  bool connected() const;
  void notifyButton(uint8_t code, uint8_t seq);          // Button Event notify
  void updateBattery(uint8_t percent);                   // BAS
  void updateStatus(uint8_t statusFlags);                // Device Status notify
  void clearBondAndRepair();                             // FORGET_BOND path

  void requestFastParams();
  void requestSlowParams();
};

}  // namespace mm

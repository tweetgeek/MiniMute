# MiniMute Firmware Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build prototype firmware for a BLE meeting-mute button on the Seeed XIAO nRF52840: button gestures → BLE events to a companion app, app-driven RGB indicator, battery/charge reporting, two-stage sleep, bonding to one host.

**Architecture:** Modular core (Approach A) — hardware-independent logic in `lib/core/` (unit-tested natively with Unity), nRF52/Bluefruit adapters behind interfaces in `lib/hal/`, composed in `src/main.cpp`. The BLE/GATT contract lives in a shared header (`include/minimute_protocol.h`) reused later by the companion app.

**Tech Stack:** PlatformIO, Arduino framework (Adafruit nRF52 BSP / Bluefruit), Unity test framework, C++17. Target board `seeed-xiao-afruitnrf52-nrf52840`. Host `native` env for unit tests.

**Spec:** `firmware/docs/superpowers/specs/2026-06-17-minimute-firmware-design.md`

**Working directory:** all paths are relative to `firmware/` (the PlatformIO project root inside the monorepo). Run all `pio` / `git` commands from there unless noted.

**Commit convention:** every commit message ends with the trailer:
```
Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>
```
(Shown in full in Task 1; abbreviated as `<trailer>` in later commit steps — always include it.)

---

## File Structure

```
firmware/
├── platformio.ini                      # add [env:native] test env
├── include/
│   └── minimute_protocol.h             # shared BLE/GATT contract (UUIDs, codes, flags)
├── lib/
│   ├── core/                           # platform-independent logic (native-testable)
│   │   ├── ButtonEvent.h
│   │   ├── GestureRecognizer.h / .cpp
│   │   ├── LedPattern.h
│   │   ├── LedStateResolver.h / .cpp
│   │   ├── PowerManager.h / .cpp
│   │   ├── BatteryGauge.h / .cpp
│   │   └── AppLinkProtocol.h / .cpp
│   └── hal/                            # nRF52 adapters (compile on device env only)
│       ├── ports.h                     # IButton, IRgbLed, IBatterySensor interfaces
│       ├── NrfButton.h / .cpp
│       ├── NrfRgbLed.h / .cpp
│       ├── NrfBatterySensor.h / .cpp
│       └── BleLink.h / .cpp
├── src/
│   └── main.cpp                        # composition + scheduler
└── test/
    ├── test_gesture/test_gesture.cpp
    ├── test_led_resolver/test_led_resolver.cpp
    ├── test_power/test_power.cpp
    ├── test_battery/test_battery.cpp
    └── test_protocol/test_protocol.cpp
```

**Responsibilities:**
- `lib/core/` — pure logic, no `Arduino.h`, all time passed in as `uint32_t` milliseconds. Deterministic, unit-tested.
- `lib/hal/` — thin wrappers over GPIO/ADC/PWM/Bluefruit, implementing `ports.h` interfaces. Not unit-tested; verified on hardware.
- `include/minimute_protocol.h` — single source of truth for the wire format; copied into the companion-app repo later.
- `src/main.cpp` — wiring + the cooperative loop.

---

## Task 1: Native test environment

**Files:**
- Modify: `platformio.ini`
- Test: `test/test_sanity/test_sanity.cpp`

- [ ] **Step 1: Add the native test env to `platformio.ini`**

Append below the existing `[env:seeed-xiao-afruitnrf52-nrf52840]` block:

```ini
[env:native]
platform = native
test_framework = unity
build_flags = -std=gnu++17 -Wall
test_build_src = no
```

- [ ] **Step 2: Write a sanity test**

Create `test/test_sanity/test_sanity.cpp`:

```cpp
#include <unity.h>

void test_toolchain_runs(void) {
  TEST_ASSERT_EQUAL_INT(4, 2 + 2);
}

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv) {
  (void)argc; (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_toolchain_runs);
  return UNITY_END();
}
```

- [ ] **Step 3: Run the native test**

Run: `pio test -e native -f test_sanity`
Expected: `test_toolchain_runs PASSED` and `1 Tests 0 Failures`.

- [ ] **Step 4: Commit**

```bash
git add platformio.ini test/test_sanity
git commit -m "$(cat <<'EOF'
chore(firmware): add native unity test environment

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: GestureRecognizer

Recognizes SHORT / LONG / VERY_LONG from a polled, debounced button signal. DOUBLE is reserved (not emitted in v1) so SHORT fires immediately on release. To avoid a LONG firing alongside a VERY_LONG, LONG is decided on release; VERY_LONG latches while held (also handled on release for sparse polling).

**Files:**
- Create: `lib/core/ButtonEvent.h`, `lib/core/GestureRecognizer.h`, `lib/core/GestureRecognizer.cpp`
- Test: `test/test_gesture/test_gesture.cpp`

- [ ] **Step 1: Define the event type**

Create `lib/core/ButtonEvent.h`:

```cpp
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
```

- [ ] **Step 2: Write the failing tests**

Create `test/test_gesture/test_gesture.cpp`:

```cpp
#include <unity.h>
#include "GestureRecognizer.h"

using mm::ButtonEvent;
using mm::GestureRecognizer;

// Helper: feed a stable level until the debounce window passes, return last event.
static ButtonEvent settle(GestureRecognizer& g, bool pressed, uint32_t& t) {
  ButtonEvent ev = g.update(pressed, t);  // edge candidate starts here
  t += 25;                                // > debounce (20ms)
  ButtonEvent ev2 = g.update(pressed, t); // edge commits here
  return (ev2 != ButtonEvent::None) ? ev2 : ev;
}

void test_short_press(void) {
  GestureRecognizer g;
  uint32_t t = 0;
  TEST_ASSERT_EQUAL(ButtonEvent::None, settle(g, true, t));   // press
  t += 200;                                                   // hold 200ms < long
  TEST_ASSERT_EQUAL(ButtonEvent::Short, settle(g, false, t)); // release -> SHORT
}

void test_long_press(void) {
  GestureRecognizer g;
  uint32_t t = 0;
  settle(g, true, t);                                         // press
  t += 700;                                                   // hold 700ms >= long, < verylong
  TEST_ASSERT_EQUAL(ButtonEvent::Long, settle(g, false, t));  // release -> LONG
}

void test_very_long_latches_while_held(void) {
  GestureRecognizer g;
  uint32_t t = 0;
  settle(g, true, t);                                         // press
  t += 3100;                                                  // exceed verylong (3000ms)
  TEST_ASSERT_EQUAL(ButtonEvent::VeryLong, g.update(true, t));// fires while still held
  t += 500;
  TEST_ASSERT_EQUAL(ButtonEvent::None, settle(g, false, t));  // release after latch -> nothing
}

void test_glitch_shorter_than_debounce_ignored(void) {
  GestureRecognizer g;
  uint32_t t = 0;
  TEST_ASSERT_EQUAL(ButtonEvent::None, g.update(true, t));    // bounce up
  t += 5;
  TEST_ASSERT_EQUAL(ButtonEvent::None, g.update(false, t));   // bounce down before 20ms
  t += 50;
  TEST_ASSERT_EQUAL(ButtonEvent::None, g.update(false, t));   // no event emitted
}

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv) {
  (void)argc; (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_short_press);
  RUN_TEST(test_long_press);
  RUN_TEST(test_very_long_latches_while_held);
  RUN_TEST(test_glitch_shorter_than_debounce_ignored);
  return UNITY_END();
}
```

- [ ] **Step 3: Run to verify it fails**

Run: `pio test -e native -f test_gesture`
Expected: FAIL — `GestureRecognizer.h` not found / undefined.

- [ ] **Step 4: Write the header**

Create `lib/core/GestureRecognizer.h`:

```cpp
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

  explicit GestureRecognizer(Config cfg = {}) : cfg_(cfg) {}

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
```

- [ ] **Step 5: Write the implementation**

Create `lib/core/GestureRecognizer.cpp`:

```cpp
#include "GestureRecognizer.h"

namespace mm {

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
```

- [ ] **Step 6: Run to verify it passes**

Run: `pio test -e native -f test_gesture`
Expected: 4 tests, 0 failures.

- [ ] **Step 7: Commit**

```bash
git add lib/core/ButtonEvent.h lib/core/GestureRecognizer.h lib/core/GestureRecognizer.cpp test/test_gesture
git commit -m "feat(firmware): gesture recognizer (short/long/very-long)

<trailer>"
```

---

## Task 3: LedStateResolver

Pure function mapping current indicator/device inputs to a single `LedPattern`. Strict priority — one steady descriptor; blink/breathe cadence is rendered by the LED driver (Task 8). The low-battery warning is delivered as a brief `lowBatteryFlash` window that the caller toggles (kept pure here).

**Files:**
- Create: `lib/core/LedPattern.h`, `lib/core/LedStateResolver.h`, `lib/core/LedStateResolver.cpp`
- Test: `test/test_led_resolver/test_led_resolver.cpp`

- [ ] **Step 1: Define the pattern type**

Create `lib/core/LedPattern.h`:

```cpp
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
```

- [ ] **Step 2: Write the failing tests**

Create `test/test_led_resolver/test_led_resolver.cpp`:

```cpp
#include <unity.h>
#include "LedStateResolver.h"

using namespace mm;

static LedPattern P(LedColor c, LedMode m) { return LedPattern{c, m}; }

void test_deep_sleep_off(void) {
  LedInputs in; in.deepSleep = true; in.connected = false;
  TEST_ASSERT_TRUE(P(LedColor::Off, LedMode::Solid) == resolveLed(in));
}

void test_searching_blue_slow_blink(void) {
  LedInputs in; in.connected = false;
  TEST_ASSERT_TRUE(P(LedColor::Blue, LedMode::SlowBlink) == resolveLed(in));
}

void test_in_call_muted_red(void) {
  LedInputs in; in.connected = true; in.inCall = true; in.muted = true;
  TEST_ASSERT_TRUE(P(LedColor::Red, LedMode::Solid) == resolveLed(in));
}

void test_in_call_live_green(void) {
  LedInputs in; in.connected = true; in.inCall = true; in.muted = false;
  TEST_ASSERT_TRUE(P(LedColor::Green, LedMode::Solid) == resolveLed(in));
}

void test_charging_amber_breathe(void) {
  LedInputs in; in.connected = true; in.inCall = false; in.charging = true;
  TEST_ASSERT_TRUE(P(LedColor::Amber, LedMode::Breathe) == resolveLed(in));
}

void test_idle_off(void) {
  LedInputs in; in.connected = true; in.inCall = false;
  TEST_ASSERT_TRUE(P(LedColor::Off, LedMode::Solid) == resolveLed(in));
}

void test_identify_overrides_everything(void) {
  LedInputs in; in.connected = true; in.inCall = true; in.muted = true; in.identify = true;
  TEST_ASSERT_TRUE(P(LedColor::White, LedMode::FastBlink) == resolveLed(in));
}

void test_low_battery_flash_overrides_base(void) {
  LedInputs in; in.connected = true; in.inCall = true; in.muted = true; in.lowBatteryFlash = true;
  TEST_ASSERT_TRUE(P(LedColor::Red, LedMode::Solid) == resolveLed(in));  // flash window
}

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv) {
  (void)argc; (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_deep_sleep_off);
  RUN_TEST(test_searching_blue_slow_blink);
  RUN_TEST(test_in_call_muted_red);
  RUN_TEST(test_in_call_live_green);
  RUN_TEST(test_charging_amber_breathe);
  RUN_TEST(test_idle_off);
  RUN_TEST(test_identify_overrides_everything);
  RUN_TEST(test_low_battery_flash_overrides_base);
  return UNITY_END();
}
```

- [ ] **Step 3: Run to verify it fails**

Run: `pio test -e native -f test_led_resolver`
Expected: FAIL — `LedStateResolver.h` not found.

- [ ] **Step 4: Write the header**

Create `lib/core/LedStateResolver.h`:

```cpp
#pragma once
#include "LedPattern.h"

namespace mm {

struct LedInputs {
  bool connected       = false;  // BLE link up
  bool inCall          = false;  // app: in_call flag
  bool muted           = false;  // app: muted flag
  bool charging        = false;  // charger active
  bool charged         = false;  // VBUS present and charge complete
  bool lowBattery      = false;  // gauge low (informational)
  bool lowBatteryFlash = false;  // caller-gated brief warning window
  bool identify        = false;  // IDENTIFY command active
  bool deepSleep       = false;  // entering/in deep sleep
};

LedPattern resolveLed(const LedInputs& in);

}  // namespace mm
```

- [ ] **Step 5: Write the implementation**

Create `lib/core/LedStateResolver.cpp`:

```cpp
#include "LedStateResolver.h"

namespace mm {

LedPattern resolveLed(const LedInputs& in) {
  if (in.deepSleep)        return {LedColor::Off,   LedMode::Solid};
  if (in.identify)         return {LedColor::White, LedMode::FastBlink};
  if (in.lowBatteryFlash)  return {LedColor::Red,   LedMode::Solid};   // brief overlay window
  if (!in.connected)       return {LedColor::Blue,  LedMode::SlowBlink};
  if (in.inCall)           return in.muted ? LedPattern{LedColor::Red,   LedMode::Solid}
                                           : LedPattern{LedColor::Green, LedMode::Solid};
  if (in.charging)         return {LedColor::Amber, LedMode::Breathe};
  if (in.charged)          return {LedColor::Green, LedMode::Solid};
  return {LedColor::Off, LedMode::Solid};  // idle on battery
}

}  // namespace mm
```

- [ ] **Step 6: Run to verify it passes**

Run: `pio test -e native -f test_led_resolver`
Expected: 8 tests, 0 failures.

- [ ] **Step 7: Commit**

```bash
git add lib/core/LedPattern.h lib/core/LedStateResolver.h lib/core/LedStateResolver.cpp test/test_led_resolver
git commit -m "feat(firmware): LED state resolver with priority

<trailer>"
```

---

## Task 4: PowerManager

State machine: Active → ConnectedIdle (light sleep) → DeepSleep. `inCall` blocks DeepSleep. Time is injected.

**Files:**
- Create: `lib/core/PowerManager.h`, `lib/core/PowerManager.cpp`
- Test: `test/test_power/test_power.cpp`

- [ ] **Step 1: Write the failing tests**

Create `test/test_power/test_power.cpp`:

```cpp
#include <unity.h>
#include "PowerManager.h"

using namespace mm;

static PowerManager::Config fastCfg() {
  PowerManager::Config c;
  c.activeTimeoutMs       = 1000;
  c.deepSleepTimeoutMs    = 5000;
  c.disconnectedTimeoutMs = 2000;
  return c;
}

void test_starts_active(void) {
  PowerManager pm(fastCfg());
  TEST_ASSERT_EQUAL(PowerState::Active, pm.update(0));
}

void test_connected_idle_after_timeout(void) {
  PowerManager pm(fastCfg());
  pm.setConnected(true, 0);
  pm.noteActivity(0);
  TEST_ASSERT_EQUAL(PowerState::Active, pm.update(500));
  TEST_ASSERT_EQUAL(PowerState::ConnectedIdle, pm.update(1500));  // > activeTimeout
}

void test_activity_wakes_from_idle(void) {
  PowerManager pm(fastCfg());
  pm.setConnected(true, 0);
  pm.update(1500);                       // ConnectedIdle
  pm.noteActivity(1600);
  TEST_ASSERT_EQUAL(PowerState::Active, pm.update(1600));
}

void test_deep_sleep_after_long_inactivity(void) {
  PowerManager pm(fastCfg());
  pm.setConnected(true, 0);
  pm.noteActivity(0);
  TEST_ASSERT_EQUAL(PowerState::DeepSleep, pm.update(6000));  // > deepSleepTimeout
}

void test_in_call_blocks_deep_sleep(void) {
  PowerManager pm(fastCfg());
  pm.setConnected(true, 0);
  pm.noteActivity(0);
  pm.setInCall(true);
  TEST_ASSERT_EQUAL(PowerState::ConnectedIdle, pm.update(6000));  // not DeepSleep
}

void test_disconnected_deep_sleep(void) {
  PowerManager pm(fastCfg());
  pm.setConnected(false, 0);
  TEST_ASSERT_EQUAL(PowerState::DeepSleep, pm.update(2500));  // > disconnectedTimeout
}

void test_disconnected_but_in_call_no_deep_sleep(void) {
  PowerManager pm(fastCfg());
  pm.setInCall(true);
  pm.setConnected(false, 0);
  TEST_ASSERT_NOT_EQUAL(PowerState::DeepSleep, pm.update(2500));
}

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv) {
  (void)argc; (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_starts_active);
  RUN_TEST(test_connected_idle_after_timeout);
  RUN_TEST(test_activity_wakes_from_idle);
  RUN_TEST(test_deep_sleep_after_long_inactivity);
  RUN_TEST(test_in_call_blocks_deep_sleep);
  RUN_TEST(test_disconnected_deep_sleep);
  RUN_TEST(test_disconnected_but_in_call_no_deep_sleep);
  return UNITY_END();
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `pio test -e native -f test_power`
Expected: FAIL — `PowerManager.h` not found.

- [ ] **Step 3: Write the header**

Create `lib/core/PowerManager.h`:

```cpp
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

  explicit PowerManager(Config cfg = {}) : cfg_(cfg) {}

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
```

- [ ] **Step 4: Write the implementation**

Create `lib/core/PowerManager.cpp`:

```cpp
#include "PowerManager.h"

namespace mm {

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
```

- [ ] **Step 5: Run to verify it passes**

Run: `pio test -e native -f test_power`
Expected: 7 tests, 0 failures.

- [ ] **Step 6: Commit**

```bash
git add lib/core/PowerManager.h lib/core/PowerManager.cpp test/test_power
git commit -m "feat(firmware): two-stage power manager

<trailer>"
```

---

## Task 5: BatteryGauge

Smooths raw VBAT samples (EMA), maps mV→% via a LiPo lookup, and reports a low-battery flag with hysteresis.

**Files:**
- Create: `lib/core/BatteryGauge.h`, `lib/core/BatteryGauge.cpp`
- Test: `test/test_battery/test_battery.cpp`

- [ ] **Step 1: Write the failing tests**

Create `test/test_battery/test_battery.cpp`:

```cpp
#include <unity.h>
#include "BatteryGauge.h"

using mm::BatteryGauge;

void test_percent_endpoints(void) {
  BatteryGauge g;
  g.addSample(4200);
  TEST_ASSERT_EQUAL_UINT8(100, g.percent());
  BatteryGauge g2;
  g2.addSample(3250);                    // below table min -> clamp 0
  TEST_ASSERT_EQUAL_UINT8(0, g2.percent());
  BatteryGauge g3;
  g3.addSample(4500);                    // above max -> clamp 100
  TEST_ASSERT_EQUAL_UINT8(100, g3.percent());
}

void test_percent_midpoint_interpolates(void) {
  BatteryGauge g;
  g.addSample(3750);                     // between 3700(45%) and 3800(60%) -> ~52%
  uint8_t p = g.percent();
  TEST_ASSERT_TRUE(p >= 50 && p <= 55);
}

void test_ema_smooths(void) {
  BatteryGauge g;                        // default alpha 0.2
  g.addSample(4000);                     // seeds smoothed = 4000
  g.addSample(3000);                     // spike down
  uint16_t mv = g.smoothedMilliVolts();
  TEST_ASSERT_TRUE(mv > 3700 && mv < 3900);  // 0.2*3000 + 0.8*4000 = 3800
}

void test_low_battery_hysteresis(void) {
  BatteryGauge g;                        // lowEnter 3500, lowExit 3650
  g.addSample(4000);
  TEST_ASSERT_FALSE(g.lowBattery());
  for (int i = 0; i < 20; ++i) g.addSample(3400);   // settle below enter
  TEST_ASSERT_TRUE(g.lowBattery());
  for (int i = 0; i < 5; ++i) g.addSample(3600);    // between exit and enter
  TEST_ASSERT_TRUE(g.lowBattery());                 // stays low (hysteresis)
  for (int i = 0; i < 20; ++i) g.addSample(3700);   // above exit
  TEST_ASSERT_FALSE(g.lowBattery());
}

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv) {
  (void)argc; (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_percent_endpoints);
  RUN_TEST(test_percent_midpoint_interpolates);
  RUN_TEST(test_ema_smooths);
  RUN_TEST(test_low_battery_hysteresis);
  return UNITY_END();
}
```

- [ ] **Step 2: Run to verify it fails**

Run: `pio test -e native -f test_battery`
Expected: FAIL — `BatteryGauge.h` not found.

- [ ] **Step 3: Write the header**

Create `lib/core/BatteryGauge.h`:

```cpp
#pragma once
#include <cstdint>

namespace mm {

class BatteryGauge {
 public:
  struct Config {
    uint16_t lowEnterMv = 3500;  // assert low below this
    uint16_t lowExitMv  = 3650;  // deassert low above this (hysteresis)
    float    emaAlpha   = 0.2f;
  };

  explicit BatteryGauge(Config cfg = {}) : cfg_(cfg) {}

  void addSample(uint16_t milliVolts);
  uint16_t smoothedMilliVolts() const { return smoothed_; }
  uint8_t percent() const;
  bool lowBattery() const { return low_; }
  bool hasSample() const { return seeded_; }

 private:
  Config   cfg_;
  bool     seeded_   = false;
  bool     low_      = false;
  float    smoothed_ = 0.0f;
};

}  // namespace mm
```

- [ ] **Step 4: Write the implementation**

Create `lib/core/BatteryGauge.cpp`:

```cpp
#include "BatteryGauge.h"

namespace mm {

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
```

Note: `smoothedMilliVolts()` returns the rounded float via the header's `uint16_t` cast; rounding to nearest is fine for percent mapping. If exactness matters in tests, the EMA assertions use ranges.

- [ ] **Step 5: Run to verify it passes**

Run: `pio test -e native -f test_battery`
Expected: 4 tests, 0 failures.

- [ ] **Step 6: Commit**

```bash
git add lib/core/BatteryGauge.h lib/core/BatteryGauge.cpp test/test_battery
git commit -m "feat(firmware): battery gauge with LiPo curve and hysteresis

<trailer>"
```

---

## Task 6: Protocol header + AppLinkProtocol

Defines the shared GATT wire format and the encode/decode helpers. Tests pin the byte layout so firmware and the companion app cannot drift.

**Files:**
- Create: `include/minimute_protocol.h`, `lib/core/AppLinkProtocol.h`, `lib/core/AppLinkProtocol.cpp`
- Test: `test/test_protocol/test_protocol.cpp`

- [ ] **Step 1: Write the shared contract header**

Create `include/minimute_protocol.h`:

```cpp
#pragma once
#include <cstdint>
// MiniMute BLE/GATT contract. Single source of truth shared by firmware and the
// companion app. Working UUIDs — freeze before first external release.

#define MM_SERVICE_UUID        "6E6D0001-B5A3-F393-E0A9-E50E24DCCA9E"
#define MM_CHAR_BUTTON_UUID    "6E6D0002-B5A3-F393-E0A9-E50E24DCCA9E"  // Notify  (device->app)
#define MM_CHAR_INDICATOR_UUID "6E6D0003-B5A3-F393-E0A9-E50E24DCCA9E"  // Write   (app->device)
#define MM_CHAR_STATUS_UUID    "6E6D0004-B5A3-F393-E0A9-E50E24DCCA9E"  // Read+Notify (device->app)
#define MM_CHAR_CONTROL_UUID   "6E6D0005-B5A3-F393-E0A9-E50E24DCCA9E"  // Write   (app->device)

// Button Event char, byte[0] = code, byte[1] = sequence number.
enum mm_button_code : uint8_t {
  MM_BTN_SHORT  = 0x01,
  MM_BTN_LONG   = 0x02,
  MM_BTN_DOUBLE = 0x03,  // reserved; not emitted in v1
};

// Indicator State char, byte[0] = flags (app -> device).
enum mm_indicator_flags : uint8_t {
  MM_IND_MUTED      = 1u << 0,
  MM_IND_CAMERA_OFF = 1u << 1,
  MM_IND_IN_CALL    = 1u << 2,
};

// Device Status char, byte[0] = flags (device -> app). power_state in bits 2-3.
enum mm_status_flags : uint8_t {
  MM_ST_CHARGING    = 1u << 0,
  MM_ST_LOW_BATTERY = 1u << 1,
  MM_ST_POWER_SHIFT = 2,
  MM_ST_POWER_MASK  = 0x0Cu,
};
// power_state values: 0 = active, 1 = connected-idle.

// Device Control char, byte[0] = command (app -> device).
enum mm_control_cmd : uint8_t {
  MM_CTL_IDENTIFY    = 0x01,
  MM_CTL_FORGET_BOND = 0x02,
};
```

- [ ] **Step 2: Write the failing tests**

Create `test/test_protocol/test_protocol.cpp`:

```cpp
#include <unity.h>
#include "AppLinkProtocol.h"
#include "minimute_protocol.h"

using namespace mm;

void test_encode_button_event(void) {
  uint8_t b = 0xFF;
  TEST_ASSERT_TRUE(encodeButtonEvent(ButtonEvent::Short, b));  TEST_ASSERT_EQUAL_UINT8(MM_BTN_SHORT, b);
  TEST_ASSERT_TRUE(encodeButtonEvent(ButtonEvent::Long, b));   TEST_ASSERT_EQUAL_UINT8(MM_BTN_LONG, b);
  TEST_ASSERT_TRUE(encodeButtonEvent(ButtonEvent::Double, b)); TEST_ASSERT_EQUAL_UINT8(MM_BTN_DOUBLE, b);
}

void test_very_long_and_none_not_sent(void) {
  uint8_t b = 0;
  TEST_ASSERT_FALSE(encodeButtonEvent(ButtonEvent::VeryLong, b));
  TEST_ASSERT_FALSE(encodeButtonEvent(ButtonEvent::None, b));
}

void test_decode_indicator(void) {
  IndicatorState s = decodeIndicator(MM_IND_MUTED | MM_IND_IN_CALL);
  TEST_ASSERT_TRUE(s.muted);
  TEST_ASSERT_TRUE(s.inCall);
  TEST_ASSERT_FALSE(s.cameraOff);
}

void test_encode_status(void) {
  uint8_t s = encodeStatus(/*charging*/true, /*low*/false, /*power*/1);
  TEST_ASSERT_TRUE(s & MM_ST_CHARGING);
  TEST_ASSERT_FALSE(s & MM_ST_LOW_BATTERY);
  TEST_ASSERT_EQUAL_UINT8(1, (s & MM_ST_POWER_MASK) >> MM_ST_POWER_SHIFT);
}

void setUp(void) {}
void tearDown(void) {}

int main(int argc, char **argv) {
  (void)argc; (void)argv;
  UNITY_BEGIN();
  RUN_TEST(test_encode_button_event);
  RUN_TEST(test_very_long_and_none_not_sent);
  RUN_TEST(test_decode_indicator);
  RUN_TEST(test_encode_status);
  return UNITY_END();
}
```

- [ ] **Step 3: Run to verify it fails**

Run: `pio test -e native -f test_protocol`
Expected: FAIL — `AppLinkProtocol.h` not found.

- [ ] **Step 4: Write the header**

Create `lib/core/AppLinkProtocol.h`:

```cpp
#pragma once
#include <cstdint>
#include "ButtonEvent.h"

namespace mm {

struct IndicatorState {
  bool muted     = false;
  bool cameraOff = false;
  bool inCall    = false;
};

// Maps an internal event to its wire code. Returns false for events that are not
// sent to the app (None, VeryLong).
bool encodeButtonEvent(ButtonEvent e, uint8_t& outByte);

// Decodes the Indicator State flags byte received from the app.
IndicatorState decodeIndicator(uint8_t flags);

// Builds the Device Status flags byte. powerState: 0 active, 1 connected-idle.
uint8_t encodeStatus(bool charging, bool lowBattery, uint8_t powerState);

}  // namespace mm
```

- [ ] **Step 5: Write the implementation**

Create `lib/core/AppLinkProtocol.cpp`:

```cpp
#include "AppLinkProtocol.h"
#include "minimute_protocol.h"

namespace mm {

bool encodeButtonEvent(ButtonEvent e, uint8_t& outByte) {
  switch (e) {
    case ButtonEvent::Short:  outByte = MM_BTN_SHORT;  return true;
    case ButtonEvent::Long:   outByte = MM_BTN_LONG;   return true;
    case ButtonEvent::Double: outByte = MM_BTN_DOUBLE; return true;
    default:                  return false;  // None, VeryLong are local-only
  }
}

IndicatorState decodeIndicator(uint8_t flags) {
  IndicatorState s;
  s.muted     = flags & MM_IND_MUTED;
  s.cameraOff = flags & MM_IND_CAMERA_OFF;
  s.inCall    = flags & MM_IND_IN_CALL;
  return s;
}

uint8_t encodeStatus(bool charging, bool lowBattery, uint8_t powerState) {
  uint8_t b = 0;
  if (charging)   b |= MM_ST_CHARGING;
  if (lowBattery) b |= MM_ST_LOW_BATTERY;
  b |= (powerState << MM_ST_POWER_SHIFT) & MM_ST_POWER_MASK;
  return b;
}

}  // namespace mm
```

- [ ] **Step 6: Run to verify it passes**

Run: `pio test -e native -f test_protocol`
Expected: 4 tests, 0 failures.

- [ ] **Step 7: Run the full native suite (regression)**

Run: `pio test -e native`
Expected: all suites (sanity, gesture, led_resolver, power, battery, protocol) pass.

- [ ] **Step 8: Commit**

```bash
git add include/minimute_protocol.h lib/core/AppLinkProtocol.h lib/core/AppLinkProtocol.cpp test/test_protocol
git commit -m "feat(firmware): shared GATT protocol header and codec

<trailer>"
```

---

## Task 7: HAL ports + button & battery sensors

From here tasks build for the device (`-e seeed-xiao-afruitnrf52-nrf52840`) and are verified on hardware, not via unit tests. Define the interfaces the core depends on, then the nRF adapters.

> **Hardware to verify:** XIAO nRF52840 pin macros. Confirm against the BSP variant before flashing: onboard RGB `LED_RED`/`LED_GREEN`/`LED_BLUE` (active-LOW); battery read enable `PIN_VBAT_ENABLE`/`P0.14`, VBAT analog `PIN_VBAT`/`P0.31`; charge-status `P0.17`. Pick a free GPIO for the prototype button (e.g. `D1`) wired to GND with `INPUT_PULLUP`.

**Files:**
- Create: `lib/hal/ports.h`, `lib/hal/NrfButton.h`, `lib/hal/NrfButton.cpp`, `lib/hal/NrfBatterySensor.h`, `lib/hal/NrfBatterySensor.cpp`

- [ ] **Step 1: Define the ports**

Create `lib/hal/ports.h`:

```cpp
#pragma once
#include <cstdint>
#include "LedPattern.h"

namespace mm {

struct IButton {
  virtual ~IButton() = default;
  virtual bool isPressed() = 0;          // true while physically pressed
};

struct IRgbLed {
  virtual ~IRgbLed() = default;
  virtual void show(const LedPattern& p, uint32_t nowMs) = 0;  // render incl. blink/breathe
};

struct IBatterySensor {
  virtual ~IBatterySensor() = default;
  virtual uint16_t readMilliVolts() = 0;
  virtual bool charging() = 0;
  virtual bool vbusPresent() = 0;
};

}  // namespace mm
```

- [ ] **Step 2: Implement the button adapter**

Create `lib/hal/NrfButton.h`:

```cpp
#pragma once
#include <Arduino.h>
#include "ports.h"

namespace mm {

class NrfButton : public IButton {
 public:
  explicit NrfButton(uint32_t pin) : pin_(pin) {}
  void begin() { pinMode(pin_, INPUT_PULLUP); }
  bool isPressed() override { return digitalRead(pin_) == LOW; }  // active-low to GND
  uint32_t pin() const { return pin_; }

 private:
  uint32_t pin_;
};

}  // namespace mm
```

Create `lib/hal/NrfButton.cpp`:

```cpp
#include "NrfButton.h"
// Header-only logic; translation unit kept so PlatformIO LDF builds the library.
```

- [ ] **Step 3: Implement the battery sensor adapter**

Create `lib/hal/NrfBatterySensor.h`:

```cpp
#pragma once
#include <Arduino.h>
#include "ports.h"

namespace mm {

// Reads VBAT via the on-board divider. Divider ratio and ADC reference are
// board-specific; constants below match the XIAO nRF52840 (verify on hardware).
class NrfBatterySensor : public IBatterySensor {
 public:
  void begin();
  uint16_t readMilliVolts() override;
  bool charging() override;
  bool vbusPresent() override;
};

}  // namespace mm
```

Create `lib/hal/NrfBatterySensor.cpp`:

```cpp
#include "NrfBatterySensor.h"

namespace mm {

// XIAO nRF52840: VBAT enable on P0.14 (drive LOW to read), VBAT analog on P0.31,
// charge-status on P0.17 (LOW = charging). Divider ~ (1M + 0.51M)/0.51M ≈ 2.96.
namespace {
constexpr uint32_t kVbatEnablePin = PIN_VBAT_ENABLE;  // verify macro exists in BSP
constexpr uint32_t kVbatAdcPin    = PIN_VBAT;         // verify macro exists in BSP
constexpr uint32_t kChargePin     = 23;               // P0.17 -> Dxx; verify mapping
constexpr float    kDivider       = 2.96f;
constexpr float    kAdcRefVolts   = 3.6f;             // default nRF analog reference
constexpr float    kAdcMax        = 1023.0f;          // default 10-bit resolution
}  // namespace

void NrfBatterySensor::begin() {
  pinMode(kVbatEnablePin, OUTPUT);
  digitalWrite(kVbatEnablePin, HIGH);   // disabled until a read
  pinMode(kChargePin, INPUT_PULLUP);
}

uint16_t NrfBatterySensor::readMilliVolts() {
  digitalWrite(kVbatEnablePin, LOW);    // enable divider
  delay(5);
  int raw = analogRead(kVbatAdcPin);
  digitalWrite(kVbatEnablePin, HIGH);   // disable to save power
  float pinV = (raw / kAdcMax) * kAdcRefVolts;
  return static_cast<uint16_t>(pinV * kDivider * 1000.0f);
}

bool NrfBatterySensor::charging() {
  return digitalRead(kChargePin) == LOW;
}

bool NrfBatterySensor::vbusPresent() {
  // USB regulator status indicates VBUS. Verify against nrf_power HAL on this BSP.
  return (NRF_POWER->USBREGSTATUS & POWER_USBREGSTATUS_VBUSDETECT_Msk) != 0;
}

}  // namespace mm
```

- [ ] **Step 4: Compile for the device**

Run: `pio run -e seeed-xiao-afruitnrf52-nrf52840`
Expected: build succeeds. If a pin macro is missing, fix the constant per the BSP variant header and rebuild.

- [ ] **Step 5: Commit**

```bash
git add lib/hal/ports.h lib/hal/NrfButton.* lib/hal/NrfBatterySensor.*
git commit -m "feat(firmware): HAL ports, button and battery adapters

<trailer>"
```

---

## Task 8: RGB LED driver

Renders a `LedPattern` to the three LED channels, handling blink/breathe timing from `nowMs` and active-LOW polarity.

**Files:**
- Create: `lib/hal/NrfRgbLed.h`, `lib/hal/NrfRgbLed.cpp`

- [ ] **Step 1: Write the header**

Create `lib/hal/NrfRgbLed.h`:

```cpp
#pragma once
#include <Arduino.h>
#include "ports.h"

namespace mm {

// Drives a common-anode (active-LOW) RGB LED on three PWM-capable pins.
class NrfRgbLed : public IRgbLed {
 public:
  NrfRgbLed(uint32_t rPin, uint32_t gPin, uint32_t bPin, bool activeLow = true)
      : rPin_(rPin), gPin_(gPin), bPin_(bPin), activeLow_(activeLow) {}
  void begin();
  void show(const LedPattern& p, uint32_t nowMs) override;

 private:
  void writeRgb(uint8_t r, uint8_t g, uint8_t b);   // 0..255 logical brightness
  uint32_t rPin_, gPin_, bPin_;
  bool activeLow_;
};

}  // namespace mm
```

- [ ] **Step 2: Write the implementation**

Create `lib/hal/NrfRgbLed.cpp`:

```cpp
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
```

- [ ] **Step 3: Compile for the device**

Run: `pio run -e seeed-xiao-afruitnrf52-nrf52840`
Expected: build succeeds.

- [ ] **Step 4: On-device smoke (manual)**

Temporarily drive each pattern in a scratch sketch or via Task 9's main; confirm visually: solid red/green/blue, amber breathe, white fast blink, blue slow blink. Adjust `kBrightness`/pin polarity if colors look wrong.

- [ ] **Step 5: Commit**

```bash
git add lib/hal/NrfRgbLed.*
git commit -m "feat(firmware): RGB LED driver with blink/breathe rendering

<trailer>"
```

---

## Task 9: BleLink (Bluefruit wiring)

Wraps Bluefruit: custom service + characteristics, standard DIS/BAS, LESC Just Works bonding, advertising, connect/disconnect and write callbacks, notifications.

> **Verify against the Adafruit nRF52 BSP** (`Adafruit_Bluefruit52Lib`): API names (`Bluefruit.begin`, `BLEService`, `BLECharacteristic`, `Bluefruit.Periph.setConnectCallback`, `Bluefruit.Security`/`requestPairing`, `Bluefruit.Advertising`). Bond persistence is handled by the library's bond store; `Bluefruit.clearBonds()` (or equivalent) backs FORGET_BOND. Adjust signatures as the compiler requires.

**Files:**
- Create: `lib/hal/BleLink.h`, `lib/hal/BleLink.cpp`

- [ ] **Step 1: Write the header**

Create `lib/hal/BleLink.h`:

```cpp
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

  // Connection parameter hints for power management.
  void requestFastParams();
  void requestSlowParams();
};

}  // namespace mm
```

- [ ] **Step 2: Write the implementation**

Create `lib/hal/BleLink.cpp`:

```cpp
#include "BleLink.h"
#include "minimute_protocol.h"

namespace mm {

namespace {
BLEService        gSvc(MM_SERVICE_UUID);
BLECharacteristic gButton(MM_CHAR_BUTTON_UUID);
BLECharacteristic gIndicator(MM_CHAR_INDICATOR_UUID);
BLECharacteristic gStatus(MM_CHAR_STATUS_UUID);
BLECharacteristic gControl(MM_CHAR_CONTROL_UUID);
BLEDis  gDis;
BLEBas  gBas;
BleCallbacks gCb;

void onIndicatorWrite(uint16_t, BLECharacteristic*, uint8_t* data, uint16_t len) {
  if (len >= 1 && gCb.onIndicator) gCb.onIndicator(decodeIndicator(data[0]));
}
void onControlWrite(uint16_t, BLECharacteristic*, uint8_t* data, uint16_t len) {
  if (len < 1) return;
  if (data[0] == MM_CTL_IDENTIFY && gCb.onIdentify) gCb.onIdentify();
  if (data[0] == MM_CTL_FORGET_BOND && gCb.onForgetBond) gCb.onForgetBond();
}
void onConnect(uint16_t) { if (gCb.onConnected) gCb.onConnected(true); }
void onDisconnect(uint16_t, uint8_t) { if (gCb.onConnected) gCb.onConnected(false); }
}  // namespace

void BleLink::begin(const char* deviceName, const BleCallbacks& cb) {
  gCb = cb;
  Bluefruit.begin();
  Bluefruit.setName(deviceName);
  Bluefruit.Periph.setConnectCallback(onConnect);
  Bluefruit.Periph.setDisconnectCallback(onDisconnect);

  // Require an encrypted, bonded link (LESC Just Works).
  Bluefruit.Security.setIOCaps(false, false, false);  // no display/keyboard/yesno

  gDis.setManufacturer("TweetGeek");
  gDis.setModel("MiniMute");
  gDis.begin();
  gBas.begin();
  gBas.write(100);

  gSvc.begin();

  gButton.setProperties(CHR_PROPS_NOTIFY);
  gButton.setPermission(SECMODE_ENC_NO_MITM, SECMODE_NO_ACCESS);
  gButton.setFixedLen(2);
  gButton.begin();

  gIndicator.setProperties(CHR_PROPS_WRITE_WO_RESP | CHR_PROPS_WRITE);
  gIndicator.setPermission(SECMODE_NO_ACCESS, SECMODE_ENC_NO_MITM);
  gIndicator.setFixedLen(1);
  gIndicator.setWriteCallback(onIndicatorWrite);
  gIndicator.begin();

  gStatus.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
  gStatus.setPermission(SECMODE_ENC_NO_MITM, SECMODE_NO_ACCESS);
  gStatus.setFixedLen(1);
  gStatus.begin();
  uint8_t z = 0; gStatus.write(&z, 1);

  gControl.setProperties(CHR_PROPS_WRITE);
  gControl.setPermission(SECMODE_NO_ACCESS, SECMODE_ENC_NO_MITM);
  gControl.setFixedLen(1);
  gControl.setWriteCallback(onControlWrite);
  gControl.begin();
}

void BleLink::startAdvertising() {
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(gSvc);
  Bluefruit.Advertising.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  Bluefruit.Advertising.setInterval(32, 244);   // fast, slow (units of 0.625ms)
  Bluefruit.Advertising.setFastTimeout(30);
  Bluefruit.Advertising.start(0);                // 0 = advertise forever
}

bool BleLink::connected() const { return Bluefruit.connected() > 0; }

void BleLink::notifyButton(uint8_t code, uint8_t seq) {
  uint8_t payload[2] = {code, seq};
  gButton.notify(payload, sizeof(payload));
}

void BleLink::updateBattery(uint8_t percent) { gBas.write(percent); }

void BleLink::updateStatus(uint8_t statusFlags) {
  gStatus.write(&statusFlags, 1);
  gStatus.notify(&statusFlags, 1);
}

void BleLink::clearBondAndRepair() {
  Bluefruit.Periph.clearBonds();        // verify API name in BSP
  if (Bluefruit.connected()) Bluefruit.disconnect(0);
  startAdvertising();
}

void BleLink::requestFastParams() { Bluefruit.Periph.setConnInterval(12, 24); }   // 15-30ms
void BleLink::requestSlowParams() { Bluefruit.Periph.setConnInterval(320, 400); } // 400-500ms

}  // namespace mm
```

- [ ] **Step 3: Compile for the device**

Run: `pio run -e seeed-xiao-afruitnrf52-nrf52840`
Expected: build succeeds. Resolve any BSP API name mismatches (callback signatures, `clearBonds`, `setConnInterval`) per compiler errors.

- [ ] **Step 4: Commit**

```bash
git add lib/hal/BleLink.*
git commit -m "feat(firmware): BLE link (custom GATT, bonding, advertising)

<trailer>"
```

---

## Task 10: main.cpp composition + scheduler

Wires everything: poll button → gesture → notify + activity + LED confirm; periodic battery read → BAS/status; resolve + render LED; power update → enter System OFF on DeepSleep; configure button wake.

**Files:**
- Modify: `src/main.cpp` (replace boilerplate)

- [ ] **Step 1: Replace `src/main.cpp`**

```cpp
#include <Arduino.h>
#include "GestureRecognizer.h"
#include "LedStateResolver.h"
#include "PowerManager.h"
#include "BatteryGauge.h"
#include "AppLinkProtocol.h"
#include "NrfButton.h"
#include "NrfRgbLed.h"
#include "NrfBatterySensor.h"
#include "BleLink.h"

using namespace mm;

// --- pin config (verify against BSP / prototype wiring) ---
static constexpr uint32_t kButtonPin = D1;        // button to GND, INPUT_PULLUP

static NrfButton        button(kButtonPin);
static NrfRgbLed        led(LED_RED, LED_GREEN, LED_BLUE, /*activeLow*/ true);
static NrfBatterySensor battery;
static BleLink          ble;

static GestureRecognizer gesture;
static PowerManager      power;
static BatteryGauge      gauge;

static LedInputs ledIn;
static uint8_t   buttonSeq = 0;
static uint32_t  identifyUntil = 0;
static uint32_t  lastBatteryRead = 0;
static uint32_t  lastStatusByte = 0xFF;

// --- BLE callbacks ---
static void onIndicator(IndicatorState s) {
  ledIn.muted = s.muted;
  ledIn.inCall = s.inCall;
  power.setInCall(s.inCall);
  power.noteActivity(millis());
}
static void onIdentify()  { identifyUntil = millis() + 3000; }
static void onForget()    { ble.clearBondAndRepair(); }
static void onConnected(bool up) {
  ledIn.connected = up;
  power.setConnected(up, millis());
  if (up) ble.requestFastParams();
}

static void enterDeepSleep() {
  led.show({LedColor::Off, LedMode::Solid}, millis());
  // Wake on button press (active-low): configure sense before System OFF.
  pinMode(kButtonPin, INPUT_PULLUP_SENSE);
  sd_power_system_off();                 // does not return; wake = reset
}

void setup() {
  button.begin();
  led.begin();
  battery.begin();

  BleCallbacks cb;
  cb.onIndicator = onIndicator;
  cb.onIdentify = onIdentify;
  cb.onForgetBond = onForget;
  cb.onConnected = onConnected;
  ble.begin("MiniMute", cb);
  ble.startAdvertising();
}

void loop() {
  uint32_t now = millis();

  // 1) Button -> gesture
  ButtonEvent ev = gesture.update(button.isPressed(), now);
  if (ev != ButtonEvent::None) {
    power.noteActivity(now);
    if (ev == ButtonEvent::VeryLong) {
      ble.clearBondAndRepair();          // local re-pair
    } else {
      uint8_t code;
      if (encodeButtonEvent(ev, code) && ble.connected()) {
        ble.notifyButton(code, buttonSeq++);
        ble.requestFastParams();
      }
    }
  }

  // 2) Battery (every 30s)
  if (now - lastBatteryRead >= 30000 || lastBatteryRead == 0) {
    lastBatteryRead = now;
    gauge.addSample(battery.readMilliVolts());
    ledIn.charging = battery.charging();
    ledIn.charged = battery.vbusPresent() && !battery.charging();
    ledIn.lowBattery = gauge.lowBattery();
    if (gauge.hasSample()) ble.updateBattery(gauge.percent());
  }

  // 3) Power state
  PowerState ps = power.update(now);
  if (ps == PowerState::DeepSleep) {
    enterDeepSleep();                    // no return
  }
  if (ps == PowerState::ConnectedIdle) ble.requestSlowParams();
  ledIn.deepSleep = false;

  // 4) Status notify on change
  uint8_t pstate = (ps == PowerState::ConnectedIdle) ? 1 : 0;
  uint8_t statusByte = encodeStatus(ledIn.charging, ledIn.lowBattery, pstate);
  if (statusByte != lastStatusByte && ble.connected()) {
    ble.updateStatus(statusByte);
    lastStatusByte = statusByte;
  }

  // 5) LED overlays + render
  ledIn.identify = (now < identifyUntil);
  ledIn.lowBatteryFlash = ledIn.lowBattery && (now % 5000) < 150;  // brief warning window
  led.show(resolveLed(ledIn), now);

  delay(10);                             // ~100 Hz poll; light sleep between BLE events
}
```

> **Verify against BSP:** `INPUT_PULLUP_SENSE`, `sd_power_system_off()` (or the Bluefruit-recommended sleep entry), and `D1`/`LED_*` macros. Adjust if the compiler reports them missing.

- [ ] **Step 2: Compile for the device**

Run: `pio run -e seeed-xiao-afruitnrf52-nrf52840`
Expected: build succeeds.

- [ ] **Step 3: Flash and run the on-device verification checklist (manual)**

Flash: `pio run -e seeed-xiao-afruitnrf52-nrf52840 -t upload`

Verify (mark each):
- [ ] Boots, LED shows blue slow blink (searching).
- [ ] Pairs and bonds from a BLE scanner (nRF Connect) on macOS, then again on Windows; LED stops blue blinking when connected.
- [ ] Short press → Button Event notify `0x01` with incrementing seq; Long press → `0x02`.
- [ ] Writing Indicator State with `in_call`+`muted` → LED solid red; clearing `muted` → solid green; clearing `in_call` → LED off.
- [ ] Battery Level characteristic shows a plausible percentage; unplugging/charging flips `charging` in Device Status.
- [ ] Control `0x01` (IDENTIFY) → ~3s white fast blink; Control `0x02` (FORGET_BOND) → drops link, returns to blue blink and is re-pairable.
- [ ] Very-long press (≥3s) → forgets bond and re-advertises (no Long sent to app).
- [ ] Leave disconnected ≥ disconnected-timeout → device enters System OFF (current drops); button press wakes and reconnects.
- [ ] With `in_call` set, device does not deep-sleep.
- [ ] Measure current in active / connected-idle / deep-sleep with a power profiler; record numbers in the spec's open-questions section.

- [ ] **Step 4: Commit**

```bash
git add src/main.cpp
git commit -m "feat(firmware): compose modules in main loop with sleep + wake

<trailer>"
```

---

## Self-Review Notes (resolved during planning)

- **Spec coverage:** transport/GATT → Tasks 6, 9; dumb-controller gestures → Tasks 2, 6, 10; app-driven LED + priorities → Tasks 3, 8, 10; two-stage sleep + wake + in_call inhibit → Tasks 4, 10; battery/charging + BAS/status → Tasks 5, 7, 10; bonding/FORGET_BOND/re-pair → Tasks 9, 10; testing strategy → Tasks 1–6 (native) + Task 10 checklist (on-device). Definition-of-Done items 1–6 all map to Task 10's checklist.
- **Refinement vs spec:** LONG is emitted on release (not on threshold crossing) to cleanly disambiguate from VERY_LONG and guarantee a very-long hold never also triggers LONG. Noted in Task 2.
- **Low-battery overlay:** implemented as a caller-gated `lowBatteryFlash` window (Task 10, step) keeping `resolveLed` a pure function (Task 3).
- **Type consistency:** `ButtonEvent`, `LedPattern`/`LedColor`/`LedMode`, `LedInputs`, `IndicatorState`, `PowerState`, and the `MM_*` constants are defined once and referenced consistently across tasks and `main.cpp`.
- **Hardware unknowns** (pin macros, Bluefruit API names, System OFF entry) are flagged inline as "verify against BSP" rather than left as silent assumptions; they surface at compile time in Tasks 7–10.
```

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

// Enter nRF52 System OFF (deep sleep).
// BSP systemOff(pin, 0) configures pull-up + SENSE_LOW on the button pin, then
// calls sd_power_system_off() when the SoftDevice is enabled, or writes
// NRF_POWER->SYSTEMOFF directly otherwise.  Does not return; wake = reset.
static void enterDeepSleep() {
  led.show(LedPattern(LedColor::Off, LedMode::Solid), millis());
  systemOff(kButtonPin, 0);   // wake_logic=0 -> wake on LOW (button press to GND)
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

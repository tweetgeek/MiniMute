#include "NrfBatterySensor.h"

namespace mm {

// XIAO nRF52840 battery circuit (BQ25100 charger):
//   VBAT_ENABLE  = D14 (P0.14) – drive LOW to connect resistor divider; macro from variant.h
//   PIN_VBAT     = D32 (P0.31) – analog input after divider; macro from variant.h
//   ~CHG         = D23 (P0.17) – active-LOW charge status from BQ25100; no BSP macro,
//                  raw Arduino pin number used (confirmed in variant.cpp: D23 → P0.17)
//   HICHG/PIN_CHARGING_CURRENT = D22 (P0.13) is the high-charge-current selector,
//                  NOT the charge-status line – we do not touch it here.
//
// Divider: 1 MΩ + 510 kΩ, bottom = 510 kΩ  →  ratio = (1000+510)/510 ≈ 2.961
// ADC: 12-bit (ADC_RESOLUTION=12 in variant.h), Adafruit nRF52 BSP default Vref = 3.6 V
namespace {
constexpr uint32_t kVbatEnablePin = VBAT_ENABLE;  // 14 – confirmed macro, variant.h line 56
constexpr uint32_t kVbatAdcPin    = PIN_VBAT;     // 32 – confirmed macro, variant.h line 69
constexpr uint32_t kChargePin     = 23;           // P0.17 (~CHG) – D23, variant.cpp line 40
constexpr float    kDivider       = 2.961f;
constexpr float    kAdcRefVolts   = 3.6f;         // Adafruit nRF52 BSP default SAADC reference
constexpr float    kAdcMax        = 4095.0f;      // 12-bit resolution (ADC_RESOLUTION=12)
}  // namespace

void NrfBatterySensor::begin() {
  // VBAT_ENABLE is already configured OUTPUT/HIGH by initVariant() in the BSP,
  // but we set it explicitly here for clarity and to be safe if called before Arduino init.
  pinMode(kVbatEnablePin, OUTPUT);
  digitalWrite(kVbatEnablePin, HIGH);  // disabled until a read
  pinMode(kChargePin, INPUT_PULLUP);
}

uint16_t NrfBatterySensor::readMilliVolts() {
  digitalWrite(kVbatEnablePin, LOW);   // enable divider
  delay(5);
  int raw = analogRead(kVbatAdcPin);
  digitalWrite(kVbatEnablePin, HIGH);  // disable to save power
  float pinV = (raw / kAdcMax) * kAdcRefVolts;
  return static_cast<uint16_t>(pinV * kDivider * 1000.0f);
}

bool NrfBatterySensor::charging() {
  // BQ25100 ~CHG pin: LOW = actively charging, HIGH (or open-drain floating) = not charging.
  return digitalRead(kChargePin) == LOW;
}

bool NrfBatterySensor::vbusPresent() {
  // POWER_USBREGSTATUS_VBUSDETECT_Msk is defined in the nRF5 SDK MDK headers,
  // included transitively via the Adafruit nRF52 Arduino core.
  return (NRF_POWER->USBREGSTATUS & POWER_USBREGSTATUS_VBUSDETECT_Msk) != 0;
}

}  // namespace mm

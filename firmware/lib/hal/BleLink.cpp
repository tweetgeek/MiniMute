#include "BleLink.h"
#include "minimute_protocol.h"

namespace mm {

namespace {

// BLEService / BLECharacteristic accept BLEUuid, which has a const char* constructor,
// so the string-UUID macros from minimute_protocol.h convert implicitly.
BLEService        gSvc(MM_SERVICE_UUID);
BLECharacteristic gButton(MM_CHAR_BUTTON_UUID);
BLECharacteristic gIndicator(MM_CHAR_INDICATOR_UUID);
BLECharacteristic gStatus(MM_CHAR_STATUS_UUID);
BLECharacteristic gControl(MM_CHAR_CONTROL_UUID);

BLEDis  gDis;
BLEBas  gBas;

BleCallbacks gCb;

// Write callback signature: (conn_hdl, chr*, data, len)
void onIndicatorWrite(uint16_t /*conn_hdl*/, BLECharacteristic* /*chr*/,
                      uint8_t* data, uint16_t len) {
  if (len >= 1 && gCb.onIndicator) {
    gCb.onIndicator(decodeIndicator(data[0]));
  }
}

void onControlWrite(uint16_t /*conn_hdl*/, BLECharacteristic* /*chr*/,
                    uint8_t* data, uint16_t len) {
  if (len < 1) return;
  if (data[0] == MM_CTL_IDENTIFY  && gCb.onIdentify)   gCb.onIdentify();
  if (data[0] == MM_CTL_FORGET_BOND && gCb.onForgetBond) gCb.onForgetBond();
}

// Connect / disconnect callbacks: (conn_hdl) and (conn_hdl, reason)
void onConnect   (uint16_t /*conn_hdl*/)              { if (gCb.onConnected) gCb.onConnected(true);  }
void onDisconnect(uint16_t /*conn_hdl*/, uint8_t /*reason*/) { if (gCb.onConnected) gCb.onConnected(false); }

}  // namespace

void BleLink::begin(const char* deviceName, const BleCallbacks& cb) {
  gCb = cb;

  Bluefruit.begin();
  Bluefruit.setName(deviceName);

  // Register GAP connect / disconnect callbacks (BLEPeriph API).
  Bluefruit.Periph.setConnectCallback(onConnect);
  Bluefruit.Periph.setDisconnectCallback(onDisconnect);

  // Request Just-Works encrypted bonding: no display, no yes/no button, no keyboard.
  // BLESecurity::setIOCaps(bool display, bool yes_no, bool keyboard)
  Bluefruit.Security.setIOCaps(false, false, false);
  // Disable MITM so Just Works (unauthenticated) pairing is accepted.
  Bluefruit.Security.setMITM(false);

  // Device Information Service
  gDis.setManufacturer("TweetGeek");
  gDis.setModel("MiniMute");
  gDis.begin();

  // Battery Service — initial value 100%
  gBas.begin();
  gBas.write(100);

  // Custom MiniMute service
  gSvc.begin();

  // ---- Button Event characteristic (Notify, device->app, encrypted) ----
  gButton.setProperties(CHR_PROPS_NOTIFY);
  gButton.setPermission(SECMODE_ENC_NO_MITM, SECMODE_NO_ACCESS);
  gButton.setFixedLen(2);
  gButton.begin();

  // ---- Indicator State characteristic (Write, app->device, encrypted) ----
  gIndicator.setProperties(CHR_PROPS_WRITE_WO_RESP | CHR_PROPS_WRITE);
  gIndicator.setPermission(SECMODE_NO_ACCESS, SECMODE_ENC_NO_MITM);
  gIndicator.setFixedLen(1);
  gIndicator.setWriteCallback(onIndicatorWrite);
  gIndicator.begin();

  // ---- Device Status characteristic (Read + Notify, device->app, encrypted) ----
  gStatus.setProperties(CHR_PROPS_READ | CHR_PROPS_NOTIFY);
  gStatus.setPermission(SECMODE_ENC_NO_MITM, SECMODE_NO_ACCESS);
  gStatus.setFixedLen(1);
  gStatus.begin();
  uint8_t z = 0;
  gStatus.write(&z, 1);

  // ---- Device Control characteristic (Write, app->device, encrypted) ----
  gControl.setProperties(CHR_PROPS_WRITE);
  gControl.setPermission(SECMODE_NO_ACCESS, SECMODE_ENC_NO_MITM);
  gControl.setFixedLen(1);
  gControl.setWriteCallback(onControlWrite);
  gControl.begin();
}

void BleLink::startAdvertising() {
  Bluefruit.Advertising.clearData();
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(gSvc);
  Bluefruit.Advertising.addName();
  Bluefruit.Advertising.restartOnDisconnect(true);
  // setInterval takes fast and slow in 0.625 ms units (32 = 20 ms, 244 = 152.5 ms)
  Bluefruit.Advertising.setInterval(32, 244);
  Bluefruit.Advertising.setFastTimeout(30);   // fast phase: 30 s
  Bluefruit.Advertising.start(0);             // 0 = advertise forever
}

bool BleLink::connected() const {
  return Bluefruit.connected() > 0;
}

bool BleLink::notifyButton(uint8_t code, uint8_t seq) {
  uint8_t payload[2] = {code, seq};
  return gButton.notify(payload, sizeof(payload));
}

void BleLink::updateBattery(uint8_t percent) {
  gBas.write(percent);
}

void BleLink::updateStatus(uint8_t statusFlags) {
  gStatus.write(&statusFlags, 1);
  gStatus.notify(&statusFlags, 1);
}

void BleLink::clearBondAndRepair() {
  // bond_clear_prph() (from bonding.h) removes all stored peripheral bonds from flash.
  // Bluefruit.Periph.clearBonds() is the public API wrapper confirmed in BLEPeriph.h.
  Bluefruit.Periph.clearBonds();
  // Disconnect any active connection so the peer forgets the bond too.
  if (Bluefruit.connected()) {
    Bluefruit.disconnect(Bluefruit.connHandle());
  }
  startAdvertising();
}

// Request tighter connection parameters (15–30 ms) for low-latency bursts.
void BleLink::requestFastParams() {
  Bluefruit.Periph.setConnInterval(12, 24);   // 12*1.25=15 ms … 24*1.25=30 ms
}

// Request relaxed connection parameters (400–500 ms) for idle power saving.
void BleLink::requestSlowParams() {
  Bluefruit.Periph.setConnInterval(320, 400); // 320*1.25=400 ms … 400*1.25=500 ms
}

}  // namespace mm

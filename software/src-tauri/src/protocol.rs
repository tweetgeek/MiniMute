//! BLE/GATT protocol — mirror of firmware/include/minimute_protocol.h. KEEP IN SYNC.
use serde::Serialize;

pub const SERVICE_UUID: &str       = "6E6D0001-B5A3-F393-E0A9-E50E24DCCA9E";
pub const CHAR_BUTTON_UUID: &str    = "6E6D0002-B5A3-F393-E0A9-E50E24DCCA9E";
pub const CHAR_INDICATOR_UUID: &str = "6E6D0003-B5A3-F393-E0A9-E50E24DCCA9E";
pub const CHAR_STATUS_UUID: &str    = "6E6D0004-B5A3-F393-E0A9-E50E24DCCA9E";
pub const CHAR_CONTROL_UUID: &str   = "6E6D0005-B5A3-F393-E0A9-E50E24DCCA9E";
pub const BATTERY_LEVEL_UUID: &str  = "00002A19-0000-1000-8000-00805F9B34FB";

const BTN_SHORT: u8 = 0x01;
const BTN_LONG: u8 = 0x02;
const BTN_DOUBLE: u8 = 0x03;
const IND_MUTED: u8 = 1 << 0;
const IND_CAMERA_OFF: u8 = 1 << 1;
const IND_IN_CALL: u8 = 1 << 2;
const ST_CHARGING: u8 = 1 << 0;
const ST_LOW_BATTERY: u8 = 1 << 1;
const ST_POWER_MASK: u8 = 0x0C;
const ST_POWER_SHIFT: u8 = 2;
const CTL_IDENTIFY: u8 = 0x01;
const CTL_FORGET_BOND: u8 = 0x02;

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
#[serde(rename_all = "camelCase")]
pub enum ButtonCode { Short, Long, Double }

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct ButtonEvent { pub code: ButtonCode, pub seq: u8 }

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Default)]
#[serde(rename_all = "camelCase")]
pub enum PowerState { #[default] Active, ConnectedIdle }

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Default)]
#[serde(rename_all = "camelCase")]
pub struct DeviceStatus { pub charging: bool, pub low_battery: bool, pub power_state: PowerState }

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Default)]
#[serde(rename_all = "camelCase")]
pub struct IndicatorState { pub muted: bool, pub camera_off: bool, pub in_call: bool }

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ControlCmd { Identify, ForgetBond }

pub fn decode_button(bytes: &[u8]) -> Option<ButtonEvent> {
    if bytes.len() < 2 { return None; }
    let code = match bytes[0] {
        BTN_SHORT => ButtonCode::Short,
        BTN_LONG => ButtonCode::Long,
        BTN_DOUBLE => ButtonCode::Double,
        _ => return None,
    };
    Some(ButtonEvent { code, seq: bytes[1] })
}

pub fn decode_status(byte: u8) -> DeviceStatus {
    let power = if (byte & ST_POWER_MASK) >> ST_POWER_SHIFT == 1 {
        PowerState::ConnectedIdle
    } else {
        PowerState::Active
    };
    DeviceStatus { charging: byte & ST_CHARGING != 0, low_battery: byte & ST_LOW_BATTERY != 0, power_state: power }
}

pub fn decode_battery(bytes: &[u8]) -> Option<u8> { bytes.first().copied() }

pub fn encode_indicator(s: IndicatorState) -> u8 {
    let mut b = 0u8;
    if s.muted { b |= IND_MUTED; }
    if s.camera_off { b |= IND_CAMERA_OFF; }
    if s.in_call { b |= IND_IN_CALL; }
    b
}

pub fn encode_control(cmd: ControlCmd) -> u8 {
    match cmd { ControlCmd::Identify => CTL_IDENTIFY, ControlCmd::ForgetBond => CTL_FORGET_BOND }
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test] fn button_codes() {
        assert_eq!(decode_button(&[0x01, 5]), Some(ButtonEvent { code: ButtonCode::Short, seq: 5 }));
        assert_eq!(decode_button(&[0x02, 9]).unwrap().code, ButtonCode::Long);
        assert_eq!(decode_button(&[0x03, 0]).unwrap().code, ButtonCode::Double);
        assert_eq!(decode_button(&[0xFF, 0]), None);
        assert_eq!(decode_button(&[0x01]), None);
    }
    #[test] fn status_flags() {
        let s = decode_status(ST_CHARGING | (1 << ST_POWER_SHIFT));
        assert!(s.charging && !s.low_battery);
        assert_eq!(s.power_state, PowerState::ConnectedIdle);
        let s2 = decode_status(ST_LOW_BATTERY);
        assert!(s2.low_battery && !s2.charging);
        assert_eq!(s2.power_state, PowerState::Active);
    }
    #[test] fn indicator_encoding() {
        assert_eq!(encode_indicator(IndicatorState { muted: true, camera_off: false, in_call: true }), 0b101);
        assert_eq!(encode_indicator(IndicatorState::default()), 0);
    }
    #[test] fn control_encoding() {
        assert_eq!(encode_control(ControlCmd::Identify), 0x01);
        assert_eq!(encode_control(ControlCmd::ForgetBond), 0x02);
    }
    #[test] fn battery_decoding() {
        assert_eq!(decode_battery(&[87]), Some(87));
        assert_eq!(decode_battery(&[]), None);
    }
}

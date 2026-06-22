use std::collections::HashMap;
use serde::{Deserialize, Serialize};
use crate::protocol::{ButtonEvent, DeviceStatus, IndicatorState};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Default)]
#[serde(rename_all = "camelCase")]
pub enum ConnectionKind { #[default] Discovered, Connecting, Connected, Disconnected, Error }

#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct Device {
    pub id: String,
    pub name: String,
    pub known: bool,
    pub connection: ConnectionKind,
    pub error: Option<String>,
    pub battery_percent: Option<u8>,
    pub status: DeviceStatus,
    pub last_button: Option<ButtonEvent>,
    pub indicator_sent: IndicatorState,
}

impl Device {
    fn new(id: String, name: String) -> Self {
        Device {
            id, name, known: false, connection: ConnectionKind::Discovered, error: None,
            battery_percent: None, status: DeviceStatus::default(),
            last_button: None, indicator_sent: IndicatorState::default(),
        }
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct KnownDevice { pub id: String, pub name: String }

#[derive(Default)]
pub struct DeviceRegistry { devices: HashMap<String, Device> }

impl DeviceRegistry {
    pub fn new() -> Self { Self::default() }

    pub fn upsert_discovered(&mut self, id: &str, name: &str) {
        self.devices.entry(id.to_string())
            .and_modify(|d| d.name = name.to_string())
            .or_insert_with(|| Device::new(id.to_string(), name.to_string()));
    }
    pub fn set_connection(&mut self, id: &str, kind: ConnectionKind, error: Option<String>) {
        if let Some(d) = self.devices.get_mut(id) { d.connection = kind; d.error = error; }
    }
    pub fn mark_known(&mut self, id: &str, known: bool) {
        if let Some(d) = self.devices.get_mut(id) { d.known = known; }
    }
    /// Returns true if a sequence gap (missed notification) was detected.
    pub fn apply_button(&mut self, id: &str, ev: ButtonEvent) -> bool {
        if let Some(d) = self.devices.get_mut(id) {
            let gap = matches!(d.last_button, Some(prev) if ev.seq != prev.seq.wrapping_add(1));
            d.last_button = Some(ev);
            gap
        } else { false }
    }
    pub fn apply_status(&mut self, id: &str, status: DeviceStatus) {
        if let Some(d) = self.devices.get_mut(id) { d.status = status; }
    }
    pub fn apply_battery(&mut self, id: &str, pct: u8) {
        if let Some(d) = self.devices.get_mut(id) { d.battery_percent = Some(pct); }
    }
    pub fn set_indicator_sent(&mut self, id: &str, ind: IndicatorState) {
        if let Some(d) = self.devices.get_mut(id) { d.indicator_sent = ind; }
    }
    pub fn remove(&mut self, id: &str) { self.devices.remove(id); }
    pub fn get(&self, id: &str) -> Option<&Device> { self.devices.get(id) }
    pub fn list(&self) -> Vec<Device> { self.devices.values().cloned().collect() }

    pub fn known_devices(&self) -> Vec<KnownDevice> {
        self.devices.values().filter(|d| d.known)
            .map(|d| KnownDevice { id: d.id.clone(), name: d.name.clone() }).collect()
    }
    pub fn load_known(&mut self, known: Vec<KnownDevice>) {
        for k in known {
            let mut d = Device::new(k.id.clone(), k.name);
            d.known = true;
            d.connection = ConnectionKind::Disconnected;
            self.devices.insert(k.id, d);
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::protocol::{ButtonCode, ButtonEvent, DeviceStatus};

    #[test] fn discover_and_update() {
        let mut r = DeviceRegistry::new();
        r.upsert_discovered("A", "MiniMute");
        assert_eq!(r.get("A").unwrap().connection, ConnectionKind::Discovered);
        r.apply_battery("A", 80);
        assert_eq!(r.get("A").unwrap().battery_percent, Some(80));
        r.apply_status("A", DeviceStatus { charging: true, ..Default::default() });
        assert!(r.get("A").unwrap().status.charging);
    }
    #[test] fn button_gap_detection() {
        let mut r = DeviceRegistry::new();
        r.upsert_discovered("A", "MiniMute");
        assert!(!r.apply_button("A", ButtonEvent { code: ButtonCode::Short, seq: 1 }));
        assert!(!r.apply_button("A", ButtonEvent { code: ButtonCode::Short, seq: 2 }));
        assert!(r.apply_button("A", ButtonEvent { code: ButtonCode::Short, seq: 5 }));
    }
    #[test] fn known_roundtrip() {
        let mut r = DeviceRegistry::new();
        r.upsert_discovered("A", "MiniMute");
        r.mark_known("A", true);
        let known = r.known_devices();
        assert_eq!(known.len(), 1);
        let mut r2 = DeviceRegistry::new();
        r2.load_known(known);
        assert!(r2.get("A").unwrap().known);
        assert_eq!(r2.get("A").unwrap().connection, ConnectionKind::Disconnected);
    }
}

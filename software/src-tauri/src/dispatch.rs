use crate::devices::{ConnectionKind, DeviceRegistry};
use crate::events::{AppEvent, LogEntry};
use crate::protocol::{self, decode_battery, decode_button, decode_status};

/// Turns raw BLE happenings into registry updates + AppEvents. Pure & synchronous.
pub struct Dispatcher {
    pub registry: DeviceRegistry,
    clock: fn() -> u64,
}

impl Dispatcher {
    pub fn new() -> Self { Self { registry: DeviceRegistry::new(), clock: now_ms } }
    #[cfg(test)]
    pub fn with_clock(clock: fn() -> u64) -> Self { Self { registry: DeviceRegistry::new(), clock } }

    fn log(&self, device_id: Option<&str>, direction: &str, message: String) -> AppEvent {
        AppEvent::Log { entry: LogEntry {
            ts: (self.clock)(), device_id: device_id.map(str::to_string),
            direction: direction.to_string(), message,
        }}
    }
    fn snapshot(&self, id: &str) -> Vec<AppEvent> {
        self.registry.get(id).cloned().map(|device| AppEvent::DeviceUpserted { device }).into_iter().collect()
    }

    pub fn on_discovered(&mut self, id: &str, name: &str) -> Vec<AppEvent> {
        self.registry.upsert_discovered(id, name);
        self.snapshot(id)
    }
    pub fn on_connected(&mut self, id: &str) -> Vec<AppEvent> {
        // FIX 7: ensure device exists even if it wasn't previously discovered
        if self.registry.get(id).is_none() { self.registry.upsert_discovered(id, "MiniMute"); }
        self.registry.set_connection(id, ConnectionKind::Connected, None);
        self.registry.mark_known(id, true);
        let mut ev = vec![self.log(Some(id), "in", "connected".into())];
        ev.extend(self.snapshot(id));
        ev
    }
    pub fn on_disconnected(&mut self, id: &str) -> Vec<AppEvent> {
        self.registry.set_connection(id, ConnectionKind::Disconnected, None);
        let mut ev = vec![self.log(Some(id), "in", "disconnected".into())];
        ev.extend(self.snapshot(id));
        ev
    }
    pub fn on_notification(&mut self, id: &str, char_uuid: &str, bytes: &[u8]) -> Vec<AppEvent> {
        let cu = char_uuid.to_uppercase();
        let mut ev = Vec::new();
        if cu == protocol::CHAR_BUTTON_UUID {
            if let Some(b) = decode_button(bytes) {
                let gap = self.registry.apply_button(id, b);
                ev.push(self.log(Some(id), "in",
                    format!("button {:?} seq={}{}", b.code, b.seq, if gap { " (gap!)" } else { "" })));
            }
        } else if cu == protocol::CHAR_STATUS_UUID {
            if let Some(byte) = bytes.first() {
                let st = decode_status(*byte);
                self.registry.apply_status(id, st);
                ev.push(self.log(Some(id), "in", format!("status {:?}", st)));
            }
        } else if cu == protocol::BATTERY_LEVEL_UUID {
            if let Some(pct) = decode_battery(bytes) {
                self.registry.apply_battery(id, pct);
                ev.push(self.log(Some(id), "in", format!("battery {}%", pct)));
            }
        }
        // MINOR: only emit device snapshot when something was recognized
        if !ev.is_empty() { ev.extend(self.snapshot(id)); }
        ev
    }
}

fn now_ms() -> u64 {
    use std::time::{SystemTime, UNIX_EPOCH};
    SystemTime::now().duration_since(UNIX_EPOCH).map(|d| d.as_millis() as u64).unwrap_or(0)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::events::AppEvent;
    fn fixed_clock() -> u64 { 1000 }

    fn upserted_id(evs: &[AppEvent]) -> Option<String> {
        evs.iter().find_map(|e| match e { AppEvent::DeviceUpserted { device } => Some(device.id.clone()), _ => None })
    }

    #[test] fn discovered_emits_snapshot() {
        let mut d = Dispatcher::with_clock(fixed_clock);
        let evs = d.on_discovered("A", "MiniMute");
        assert_eq!(upserted_id(&evs).as_deref(), Some("A"));
    }
    #[test] fn connected_marks_known_and_logs() {
        let mut d = Dispatcher::with_clock(fixed_clock);
        d.on_discovered("A", "MiniMute");
        let evs = d.on_connected("A");
        assert!(d.registry.get("A").unwrap().known);
        assert!(evs.iter().any(|e| matches!(e, AppEvent::Log { entry } if entry.message == "connected")));
    }
    #[test] fn button_notification_updates_and_logs() {
        let mut d = Dispatcher::with_clock(fixed_clock);
        d.on_discovered("A", "MiniMute");
        let evs = d.on_notification("A", &protocol::CHAR_BUTTON_UUID.to_lowercase(), &[0x01, 7]);
        assert!(d.registry.get("A").unwrap().last_button.is_some());
        assert!(evs.iter().any(|e| matches!(e, AppEvent::Log { entry } if entry.message.contains("button"))));
    }
    #[test] fn battery_notification_updates() {
        let mut d = Dispatcher::with_clock(fixed_clock);
        d.on_discovered("A", "MiniMute");
        d.on_notification("A", &protocol::BATTERY_LEVEL_UUID.to_lowercase(), &[55]);
        assert_eq!(d.registry.get("A").unwrap().battery_percent, Some(55));
    }
}

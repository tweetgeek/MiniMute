# MiniMute Companion App Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the macOS menubar companion app for MiniMute (Tauri v2 + Rust/btleplug + React/TS): discover & connect to MiniMute BLE devices, receive button/status/battery notifications and send Indicator/Control, all from a tray-launched diagnostic console window.

**Architecture:** Tauri v2. Rust backend owns BLE (btleplug behind a `BleTransport` trait), a `Dispatcher` that decodes notifications into domain events + a `DeviceRegistry`, and Tauri commands. The React/TS webview is the window UI, driven by a Zustand store fed by Tauri events. Hardware-independent logic (protocol codec, registry, dispatcher, store reducer, components) is unit-tested; the btleplug impl, Tauri command/tray glue, and packaging are verified by build + a manual on-device checklist.

**Tech Stack:** Tauri v2, Rust (tokio, btleplug, serde, async-trait), React 18 + TypeScript + Vite, Zustand, Vitest + React Testing Library.

**Spec:** `software/docs/superpowers/specs/2026-06-22-minimute-app-design.md`
**BLE contract source of truth:** `firmware/include/minimute_protocol.h`

**Working directory:** all paths relative to `software/` (the Tauri project root). Rust crate root is `software/src-tauri/`. Run `cargo` from `software/src-tauri/`, `npm`/`vitest` from `software/`.

**Commit convention:** every commit message ends with:
```
Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>
```
(Shown fully in Task 1; written as `<trailer>` later — always include it.)

---

## File Structure

```
software/
├── package.json, vite.config.ts, tsconfig.json, index.html, vitest.config.ts
├── src/                              # React frontend
│   ├── main.tsx                      # React entry
│   ├── App.tsx                       # layout: Header + ScanBar + DeviceList + EventLog
│   ├── types.ts                      # TS mirror of Rust serde types
│   ├── store.ts                      # Zustand store + pure applyEvent reducer
│   ├── bridge.ts                     # Tauri invoke/listen wrappers (mockable)
│   ├── store.test.ts                 # reducer unit tests (Vitest)
│   └── components/
│       ├── DeviceCard.tsx + DeviceCard.test.tsx
│       ├── EventLog.tsx  + EventLog.test.tsx
│       ├── ScanBar.tsx
│       └── Header.tsx
├── src-tauri/
│   ├── Cargo.toml
│   ├── tauri.conf.json
│   ├── Info.plist (or bundle config)  # NSBluetoothAlwaysUsageDescription, LSUIElement
│   ├── build.rs
│   └── src/
│       ├── main.rs                    # entry -> lib::run()
│       ├── lib.rs                     # Tauri builder: state, commands, tray, window
│       ├── protocol.rs                # codec (mirror minimute_protocol.h)  [TDD]
│       ├── devices.rs                 # Device + DeviceRegistry             [TDD]
│       ├── events.rs                  # AppEvent / LogEntry
│       ├── dispatch.rs                # Dispatcher: notif -> registry -> events [TDD]
│       ├── ble.rs                     # BleTransport trait + BtleplugTransport
│       └── commands.rs                # Tauri command handlers
```

**Responsibilities:** `protocol`/`devices`/`dispatch` are pure (no btleplug/tauri at runtime) and unit-tested. `ble` isolates btleplug behind a trait. `commands`/`lib` are thin Tauri glue. Frontend `store`/`types` are pure and tested; `components` render from state; `bridge` isolates Tauri APIs.

---

## Task 1: Scaffold Tauri + React/TS project and verify toolchain

**Files:** creates the whole `software/` Tauri scaffold (preserving existing `software/docs/`).

- [ ] **Step 1: Verify toolchain present**

Run: `rustc --version && cargo --version && node --version && npm --version`
Expected: all print versions. If Rust is missing, stop and report BLOCKED (needs `rustup`). macOS WKWebView is built-in.

- [ ] **Step 2: Scaffold into a temp dir, then merge into `software/`**

`software/` already contains `docs/`. Scaffold elsewhere and move files in:

```bash
cd /Users/sdawidziak/_TweetGeek/_Projects/MiniMute
npm create tauri-app@latest minimute-scaffold -- --template react-ts --manager npm --identifier pl.tweetgeek.minimute --yes
# move everything (including dotfiles) into software/, keeping software/docs
shopt -s dotglob
mv minimute-scaffold/* software/
rmdir minimute-scaffold
shopt -u dotglob
```
If the CLI flags differ in the installed version, run it interactively/adjust, but the result must be a React-TS Tauri v2 project rooted at `software/` with `src/` and `src-tauri/`. Verify `software/src-tauri/Cargo.toml` and `software/package.json` exist.

- [ ] **Step 3: Install JS deps and add Zustand**

```bash
cd software
npm install
npm install zustand
npm install -D vitest @testing-library/react @testing-library/jest-dom jsdom
```

- [ ] **Step 4: Add Vitest config**

Create `software/vitest.config.ts`:
```ts
import { defineConfig } from "vitest/config";
export default defineConfig({
  test: { environment: "jsdom", globals: true, setupFiles: ["./src/test-setup.ts"] },
});
```
Create `software/src/test-setup.ts`:
```ts
import "@testing-library/jest-dom";
```
Add to `software/package.json` `"scripts"`: `"test": "vitest run"`.

- [ ] **Step 5: Add Rust deps**

In `software/src-tauri/Cargo.toml` `[dependencies]`, add:
```toml
btleplug = "0.11"
tokio = { version = "1", features = ["full"] }
async-trait = "0.1"
futures = "0.3"
uuid = "1"
```
(serde + tauri are already present from the scaffold. Adjust versions if cargo reports incompatibilities.)

- [ ] **Step 6: Toolchain sanity tests**

Add a trivial Rust test in `software/src-tauri/src/lib.rs` (append):
```rust
#[cfg(test)]
mod sanity_tests {
  #[test]
  fn toolchain_runs() { assert_eq!(2 + 2, 4); }
}
```
Create `software/src/sanity.test.ts`:
```ts
import { test, expect } from "vitest";
test("toolchain runs", () => { expect(2 + 2).toBe(4); });
```

- [ ] **Step 7: Run both test runners**

Run: `cd software/src-tauri && cargo test` → expect `toolchain_runs ... ok`.
Run: `cd software && npm test` → expect `sanity.test.ts` passes.
(First `cargo test` compiles btleplug + tauri; this is slow once. If btleplug fails to build on macOS, report the exact error.)

- [ ] **Step 8: Commit**

```bash
cd /Users/sdawidziak/_TweetGeek/_Projects/MiniMute
git add software
git commit -m "$(cat <<'EOF'
chore(software): scaffold Tauri v2 + React/TS app with test runners

Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>
EOF
)"
```
Confirm the existing `firmware/` is untouched and `software/docs/` survived the merge.

---

## Task 2: protocol.rs (BLE codec) — TDD

Mirrors `firmware/include/minimute_protocol.h`. Pure module; tests pin the exact byte layout to guard the contract.

**Files:** Create `software/src-tauri/src/protocol.rs`; register `mod protocol;` in `lib.rs`.

- [ ] **Step 1: Register the module**

In `software/src-tauri/src/lib.rs`, add near the top: `pub mod protocol;`

- [ ] **Step 2: Write protocol.rs with inline failing tests**

Create `software/src-tauri/src/protocol.rs`:
```rust
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
```

- [ ] **Step 3: Run tests**

Run: `cd software/src-tauri && cargo test protocol::`
Expected: 5 tests pass.

- [ ] **Step 4: Commit**

```bash
git add software/src-tauri/src/protocol.rs software/src-tauri/src/lib.rs
git commit -m "feat(software): BLE protocol codec mirroring firmware contract

<trailer>"
```

---

## Task 3: devices.rs (DeviceRegistry) — TDD

**Files:** Create `software/src-tauri/src/devices.rs`; add `pub mod devices;` to `lib.rs`.

- [ ] **Step 1: Register module** — add `pub mod devices;` in `lib.rs`.

- [ ] **Step 2: Write devices.rs with inline tests**

Create `software/src-tauri/src/devices.rs`:
```rust
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
        assert!(!r.apply_button("A", ButtonEvent { code: ButtonCode::Short, seq: 1 })); // first: no gap
        assert!(!r.apply_button("A", ButtonEvent { code: ButtonCode::Short, seq: 2 })); // contiguous
        assert!(r.apply_button("A", ButtonEvent { code: ButtonCode::Short, seq: 5 }));  // gap
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
```

- [ ] **Step 3: Run** — `cd software/src-tauri && cargo test devices::` → 3 tests pass.

- [ ] **Step 4: Commit**
```bash
git add software/src-tauri/src/devices.rs software/src-tauri/src/lib.rs
git commit -m "feat(software): device registry with state + known-device persistence

<trailer>"
```

---

## Task 4: events.rs + dispatch.rs + ble.rs trait — TDD (dispatcher)

**Files:** Create `events.rs`, `dispatch.rs`, `ble.rs`; register modules in `lib.rs`.

- [ ] **Step 1: Register modules** — add to `lib.rs`: `pub mod events; pub mod dispatch; pub mod ble;`

- [ ] **Step 2: events.rs**

Create `software/src-tauri/src/events.rs`:
```rust
use serde::Serialize;
use crate::devices::Device;

#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct LogEntry {
    pub ts: u64,
    pub device_id: Option<String>,
    pub direction: String, // "in" | "out"
    pub message: String,
}

#[derive(Debug, Clone, Serialize)]
#[serde(tag = "type", rename_all = "camelCase")]
pub enum AppEvent {
    DeviceUpserted { device: Device },
    DeviceRemoved { id: String },
    Log { entry: LogEntry },
    BleState { available: bool, reason: Option<String> },
}
```

- [ ] **Step 3: ble.rs (trait only for now)**

Create `software/src-tauri/src/ble.rs`:
```rust
use async_trait::async_trait;

/// Outgoing BLE operations. Implemented by btleplug (BtleplugTransport) and by a
/// fake in tests. Incoming notifications are routed into Dispatcher::on_* separately.
#[async_trait]
pub trait BleTransport: Send + Sync {
    async fn start_scan(&self) -> Result<(), String>;
    async fn stop_scan(&self) -> Result<(), String>;
    async fn connect(&self, id: &str) -> Result<(), String>;
    async fn disconnect(&self, id: &str) -> Result<(), String>;
    async fn write_char(&self, id: &str, char_uuid: &str, data: &[u8], with_response: bool) -> Result<(), String>;
}
```

- [ ] **Step 4: dispatch.rs with inline tests**

Create `software/src-tauri/src/dispatch.rs`:
```rust
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
        ev.extend(self.snapshot(id));
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
```

- [ ] **Step 5: Run** — `cd software/src-tauri && cargo test dispatch::` → 4 tests pass. Then full `cargo test` → all (protocol+devices+dispatch+sanity) pass.

- [ ] **Step 6: Commit**
```bash
git add software/src-tauri/src/events.rs software/src-tauri/src/ble.rs software/src-tauri/src/dispatch.rs software/src-tauri/src/lib.rs
git commit -m "feat(software): event types, BLE transport trait, notification dispatcher

<trailer>"
```

---

## Task 5: BtleplugTransport (real BLE) + async wiring

Implements `BleTransport` with btleplug and drives a background task that feeds discovery/connection/notification happenings into the `Dispatcher` and emits the resulting `AppEvent`s. No unit tests (hardware/OS) — verified by `cargo build`; runtime verified on a device in Task 10.

> **Verify against btleplug 0.11 API** (`Manager`, `Adapter`, `Peripheral`, `Central` traits, `CharPropFlags`, `peripheral.notifications()`, `ScanFilter`). Method names/signatures may differ slightly — adjust to compile. btleplug peripheral id on macOS is `peripheral.id()` (a `PeripheralId`, stringify with `to_string()`).

**Files:** Modify `software/src-tauri/src/ble.rs` (add `BtleplugTransport` + the runtime). It needs a handle to emit events to the frontend (a `tauri::AppHandle`) and shared `Dispatcher`.

- [ ] **Step 1: Add the emitter + shared-state types**

In `software/src-tauri/src/ble.rs`, add (above/below the trait):
```rust
use std::sync::Arc;
use tokio::sync::Mutex;
use crate::dispatch::Dispatcher;
use crate::events::AppEvent;

/// Anything that can deliver an AppEvent to the UI. Abstracts tauri::AppHandle so
/// the runtime is testable later if needed.
pub trait EventSink: Send + Sync + 'static {
    fn emit(&self, event: &AppEvent);
}

pub type SharedDispatcher = Arc<Mutex<Dispatcher>>;
```

- [ ] **Step 2: Implement BtleplugTransport**

Append to `ble.rs`:
```rust
use btleplug::api::{Central, CentralEvent, Manager as _, Peripheral as _, ScanFilter, WriteType};
use btleplug::platform::{Adapter, Manager, Peripheral};
use futures::StreamExt;
use uuid::Uuid;

pub struct BtleplugTransport {
    adapter: Adapter,
    dispatcher: SharedDispatcher,
    sink: Arc<dyn EventSink>,
}

impl BtleplugTransport {
    pub async fn new(dispatcher: SharedDispatcher, sink: Arc<dyn EventSink>) -> Result<Arc<Self>, String> {
        let manager = Manager::new().await.map_err(|e| e.to_string())?;
        let adapters = manager.adapters().await.map_err(|e| e.to_string())?;
        let adapter = adapters.into_iter().next().ok_or("no BLE adapter")?;
        let me = Arc::new(Self { adapter, dispatcher, sink });
        me.clone().spawn_event_loop().await?;
        Ok(me)
    }

    fn parse_uuid(s: &str) -> Uuid { Uuid::parse_str(s).expect("static UUID") }

    async fn find(&self, id: &str) -> Option<Peripheral> {
        for p in self.adapter.peripherals().await.ok()? {
            if p.id().to_string() == id { return Some(p); }
        }
        None
    }

    async fn spawn_event_loop(self: Arc<Self>) -> Result<(), String> {
        let mut events = self.adapter.events().await.map_err(|e| e.to_string())?;
        let this = self.clone();
        tokio::spawn(async move {
            while let Some(ev) = events.next().await {
                match ev {
                    CentralEvent::DeviceDiscovered(pid) | CentralEvent::DeviceUpdated(pid) => {
                        if let Some(p) = this.adapter.peripheral(&pid).await.ok() {
                            let advertises = p.properties().await.ok().flatten()
                                .map(|pr| pr.services.iter().any(|u| *u == Self::parse_uuid(crate::protocol::SERVICE_UUID)))
                                .unwrap_or(false);
                            let name = p.properties().await.ok().flatten()
                                .and_then(|pr| pr.local_name).unwrap_or_else(|| "MiniMute".into());
                            if advertises {
                                let evs = { this.dispatcher.lock().await.on_discovered(&pid.to_string(), &name) };
                                for e in evs { this.sink.emit(&e); }
                            }
                        }
                    }
                    CentralEvent::DeviceConnected(pid) => {
                        let evs = { this.dispatcher.lock().await.on_connected(&pid.to_string()) };
                        for e in &evs { this.sink.emit(e); }
                        // subscribe after connect
                        let _ = this.clone().subscribe_all(&pid.to_string()).await;
                    }
                    CentralEvent::DeviceDisconnected(pid) => {
                        let evs = { this.dispatcher.lock().await.on_disconnected(&pid.to_string()) };
                        for e in &evs { this.sink.emit(e); }
                    }
                    _ => {}
                }
            }
        });
        Ok(())
    }

    async fn subscribe_all(self: Arc<Self>, id: &str) -> Result<(), String> {
        let p = self.find(id).await.ok_or("peripheral not found")?;
        p.discover_services().await.map_err(|e| e.to_string())?;
        let want = [crate::protocol::CHAR_BUTTON_UUID, crate::protocol::CHAR_STATUS_UUID, crate::protocol::BATTERY_LEVEL_UUID];
        for ch in p.characteristics() {
            if want.iter().any(|w| ch.uuid == Self::parse_uuid(w)) {
                let _ = p.subscribe(&ch).await;
            }
        }
        // stream notifications -> dispatcher
        let id_owned = id.to_string();
        let this = self.clone();
        let mut notifs = p.notifications().await.map_err(|e| e.to_string())?;
        tokio::spawn(async move {
            while let Some(n) = notifs.next().await {
                let evs = { this.dispatcher.lock().await.on_notification(&id_owned, &n.uuid.to_string(), &n.value) };
                for e in &evs { this.sink.emit(e); }
            }
        });
        Ok(())
    }
}

#[async_trait]
impl BleTransport for BtleplugTransport {
    async fn start_scan(&self) -> Result<(), String> {
        self.adapter.start_scan(ScanFilter { services: vec![Self::parse_uuid(crate::protocol::SERVICE_UUID)] })
            .await.map_err(|e| e.to_string())
    }
    async fn stop_scan(&self) -> Result<(), String> {
        self.adapter.stop_scan().await.map_err(|e| e.to_string())
    }
    async fn connect(&self, id: &str) -> Result<(), String> {
        let p = self.find(id).await.ok_or("peripheral not found")?;
        p.connect().await.map_err(|e| e.to_string())
    }
    async fn disconnect(&self, id: &str) -> Result<(), String> {
        let p = self.find(id).await.ok_or("peripheral not found")?;
        p.disconnect().await.map_err(|e| e.to_string())
    }
    async fn write_char(&self, id: &str, char_uuid: &str, data: &[u8], with_response: bool) -> Result<(), String> {
        let p = self.find(id).await.ok_or("peripheral not found")?;
        let ch = p.characteristics().into_iter().find(|c| c.uuid == Self::parse_uuid(char_uuid))
            .ok_or("characteristic not found")?;
        let kind = if with_response { WriteType::WithResponse } else { WriteType::WithoutResponse };
        p.write(&ch, data, kind).await.map_err(|e| e.to_string())
    }
}
```

- [ ] **Step 2b: Build**

Run: `cd software/src-tauri && cargo build`
Expected: compiles. Fix btleplug API mismatches per compiler errors (e.g., `peripheral()` taking `&PeripheralId`, `properties()` return shape). Do not change the dispatcher contract.

- [ ] **Step 3: Commit**
```bash
git add software/src-tauri/src/ble.rs
git commit -m "feat(software): btleplug transport + async discovery/notify wiring

<trailer>"
```

---

## Task 6: Tauri state + commands + event emission

Wires a shared `Dispatcher` + `BtleplugTransport` into Tauri managed state, implements the `EventSink` over `AppHandle`, and exposes commands. Verified by `cargo build`; runtime in Task 10.

> **Verify against Tauri v2 API:** `tauri::State`, `#[tauri::command]`, `app.emit(...)` (Tauri v2 uses `Emitter` trait / `app_handle.emit("event", payload)`), `tauri::Manager`. Adjust event-emit call to the exact v2 signature.

**Files:** Create `software/src-tauri/src/commands.rs`; modify `lib.rs`.

- [ ] **Step 1: commands.rs**

Create `software/src-tauri/src/commands.rs`:
```rust
use std::sync::Arc;
use tauri::State;
use crate::ble::{BleTransport, SharedDispatcher};
use crate::devices::Device;
use crate::protocol::{ControlCmd, IndicatorState, encode_control, encode_indicator, CHAR_CONTROL_UUID, CHAR_INDICATOR_UUID};

pub struct AppState {
    pub transport: Arc<dyn BleTransport>,
    pub dispatcher: SharedDispatcher,
}

#[tauri::command]
pub async fn list_devices(state: State<'_, AppState>) -> Result<Vec<Device>, String> {
    Ok(state.dispatcher.lock().await.registry.list())
}
#[tauri::command]
pub async fn scan_start(state: State<'_, AppState>) -> Result<(), String> { state.transport.start_scan().await }
#[tauri::command]
pub async fn scan_stop(state: State<'_, AppState>) -> Result<(), String> { state.transport.stop_scan().await }
#[tauri::command]
pub async fn connect_device(state: State<'_, AppState>, id: String) -> Result<(), String> { state.transport.connect(&id).await }
#[tauri::command]
pub async fn disconnect_device(state: State<'_, AppState>, id: String) -> Result<(), String> { state.transport.disconnect(&id).await }
#[tauri::command]
pub async fn forget_device(state: State<'_, AppState>, id: String) -> Result<(), String> {
    state.transport.disconnect(&id).await.ok();
    let mut d = state.dispatcher.lock().await;
    d.registry.mark_known(&id, false);
    d.registry.remove(&id);
    Ok(())
}
#[tauri::command]
pub async fn send_indicator(state: State<'_, AppState>, id: String, muted: bool, camera_off: bool, in_call: bool) -> Result<(), String> {
    let ind = IndicatorState { muted, camera_off, in_call };
    state.transport.write_char(&id, CHAR_INDICATOR_UUID, &[encode_indicator(ind)], false).await?;
    state.dispatcher.lock().await.registry.set_indicator_sent(&id, ind);
    Ok(())
}
#[tauri::command]
pub async fn send_control(state: State<'_, AppState>, id: String, command: String) -> Result<(), String> {
    let cmd = match command.as_str() {
        "identify" => ControlCmd::Identify,
        "forgetBond" => ControlCmd::ForgetBond,
        other => return Err(format!("unknown control command: {other}")),
    };
    state.transport.write_char(&id, CHAR_CONTROL_UUID, &[encode_control(cmd)], true).await
}
```

- [ ] **Step 2: Implement EventSink over AppHandle and assemble state in lib.rs**

In `software/src-tauri/src/lib.rs`, replace the generated `run()` with wiring that (a) creates the shared dispatcher, (b) builds the app, (c) in `setup`, constructs the btleplug transport with an `EventSink` that emits over the `AppHandle`, (d) registers commands. Concretely add:
```rust
pub mod commands;

use std::sync::Arc;
use tokio::sync::Mutex;
use tauri::{Emitter, Manager};
use crate::ble::{BtleplugTransport, EventSink, SharedDispatcher};
use crate::dispatch::Dispatcher;
use crate::events::AppEvent;
use crate::commands::AppState;

struct TauriSink { app: tauri::AppHandle }
impl EventSink for TauriSink {
    fn emit(&self, event: &AppEvent) { let _ = self.app.emit("app-event", event); }
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    let dispatcher: SharedDispatcher = Arc::new(Mutex::new(Dispatcher::new()));
    let dispatcher_for_setup = dispatcher.clone();

    tauri::Builder::default()
        .plugin(tauri_plugin_autostart::init(
            tauri_plugin_autostart::MacosLauncher::LaunchAgent, None))
        .setup(move |app| {
            let handle = app.handle().clone();
            let disp = dispatcher_for_setup.clone();
            tauri::async_runtime::spawn(async move {
                let sink: Arc<dyn EventSink> = Arc::new(TauriSink { app: handle.clone() });
                match BtleplugTransport::new(disp.clone(), sink).await {
                    Ok(transport) => {
                        handle.manage(AppState { transport, dispatcher: disp });
                        let _ = transport_post_init(&handle).await;
                    }
                    Err(e) => { let _ = handle.emit("app-event",
                        &AppEvent::BleState { available: false, reason: Some(e) }); }
                }
            });
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            commands::list_devices, commands::scan_start, commands::scan_stop,
            commands::connect_device, commands::disconnect_device, commands::forget_device,
            commands::send_indicator, commands::send_control,
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}

async fn transport_post_init(handle: &tauri::AppHandle) -> Result<(), String> {
    if let Some(state) = handle.try_state::<AppState>() {
        state.transport.start_scan().await.ok();
    }
    Ok(())
}
```
Note: `manage()` after async init means commands referencing `State<AppState>` must tolerate early calls before state is set — Tauri returns an error if state is missing, which surfaces as a command error in the UI (acceptable). If the executor finds a cleaner ordering (e.g., build transport before `Builder` via `tauri::async_runtime::block_on`), that is fine.

- [ ] **Step 3: Add autostart plugin dep**

In `Cargo.toml`: `tauri-plugin-autostart = "2"`. In `package.json`: `npm install @tauri-apps/plugin-autostart`. Register the plugin's permission in `src-tauri/capabilities/default.json` (add `"autostart:default"`).

- [ ] **Step 4: Build** — `cd software/src-tauri && cargo build`. Fix Tauri v2 API mismatches (emit signature, `manage`, `try_state`) per compiler output.

- [ ] **Step 5: Commit**
```bash
git add software/src-tauri/src/commands.rs software/src-tauri/src/lib.rs software/src-tauri/Cargo.toml software/src-tauri/capabilities software/package.json software/package-lock.json
git commit -m "feat(software): Tauri state, BLE commands, event emission

<trailer>"
```

---

## Task 7: Tray, window behavior, accessory app, autostart, Info.plist

No unit tests — verified by `cargo build` and the Task 10 manual checklist.

> **Verify against Tauri v2 tray/window API:** `tauri::tray::TrayIconBuilder`, `TrayIconEvent::Click`, `WindowEvent::Focused`, `app.get_webview_window("main")`, `set_activation_policy(ActivationPolicy::Accessory)`.

**Files:** modify `lib.rs` (tray + window + activation policy), `tauri.conf.json` (window hidden at start, macOS bundle), add Info.plist entries.

- [ ] **Step 1: Hide window at startup + macOS bundle config**

In `software/src-tauri/tauri.conf.json`: set the main window `"visible": false`, `"title": "MiniMute"`, reasonable `width`/`height` (e.g. 420×560). Under `bundle > macOS`, ensure an identifier. Add an `Info.plist` (or `bundle.macOS.entitlements`/`infoPlist`) with:
```xml
<key>LSUIElement</key><true/>
<key>NSBluetoothAlwaysUsageDescription</key>
<string>MiniMute uses Bluetooth to connect to your MiniMute button.</string>
```
(In Tauri v2, plist entries can be supplied via `bundle.macOS.infoPlist` path or a merged `Info.plist`. Use whatever the installed Tauri version supports; verify the produced `.app` contains these keys in Task 10.)

- [ ] **Step 2: Accessory policy + tray + window toggle in lib.rs**

In the `.setup()` closure (before the async spawn), add:
```rust
#[cfg(target_os = "macos")]
app.set_activation_policy(tauri::ActivationPolicy::Accessory);

use tauri::tray::{TrayIconBuilder, TrayIconEvent};
use tauri::Manager;
let _tray = TrayIconBuilder::new()
    .icon(app.default_window_icon().unwrap().clone())
    .on_tray_icon_event(|tray, event| {
        if let TrayIconEvent::Click { .. } = event {
            let app = tray.app_handle();
            if let Some(win) = app.get_webview_window("main") {
                if win.is_visible().unwrap_or(false) { let _ = win.hide(); }
                else { let _ = win.show(); let _ = win.set_focus(); }
            }
        }
    })
    .build(app)?;
```
Add window hide-on-blur: in the Tauri `Builder`, add `.on_window_event(|win, event| { if let tauri::WindowEvent::Focused(false) = event { let _ = win.hide(); } })`. Also intercept the close button to hide instead of exit: handle `WindowEvent::CloseRequested { api, .. }` → `api.prevent_close(); win.hide();`.
The in-window "Quit" button (Task 9) calls a `quit_app` command:
```rust
#[tauri::command]
pub fn quit_app(app: tauri::AppHandle) { app.exit(0); }
```
Add `quit_app` to `commands.rs` and to `generate_handler!`.

- [ ] **Step 3: Build** — `cd software/src-tauri && cargo build`. Resolve tray/window API differences per compiler output.

- [ ] **Step 4: Commit**
```bash
git add software/src-tauri software/src-tauri/tauri.conf.json
git commit -m "feat(software): menubar tray, accessory app, window toggle + hide-on-blur

<trailer>"
```

---

## Task 8: Frontend types + store reducer + Tauri bridge — TDD (reducer)

**Files:** Create `src/types.ts`, `src/store.ts`, `src/bridge.ts`, `src/store.test.ts`.

- [ ] **Step 1: types.ts**

Create `software/src/types.ts`:
```ts
export type ButtonCode = "short" | "long" | "double";
export interface ButtonEvent { code: ButtonCode; seq: number; }
export type PowerState = "active" | "connectedIdle";
export interface DeviceStatus { charging: boolean; lowBattery: boolean; powerState: PowerState; }
export interface IndicatorState { muted: boolean; cameraOff: boolean; inCall: boolean; }
export type ConnectionKind = "discovered" | "connecting" | "connected" | "disconnected" | "error";
export interface Device {
  id: string; name: string; known: boolean;
  connection: ConnectionKind; error: string | null;
  batteryPercent: number | null; status: DeviceStatus;
  lastButton: ButtonEvent | null; indicatorSent: IndicatorState;
}
export interface LogEntry { ts: number; deviceId: string | null; direction: "in" | "out"; message: string; }
export type AppEvent =
  | { type: "deviceUpserted"; device: Device }
  | { type: "deviceRemoved"; id: string }
  | { type: "log"; entry: LogEntry }
  | { type: "bleState"; available: boolean; reason: string | null };
```

- [ ] **Step 2: store.test.ts (failing)**

Create `software/src/store.test.ts`:
```ts
import { describe, it, expect } from "vitest";
import { applyEvent, initialState, LOG_CAP } from "./store";
import type { Device } from "./types";

const dev = (id: string): Device => ({
  id, name: "MiniMute", known: true, connection: "connected", error: null,
  batteryPercent: 80, status: { charging: false, lowBattery: false, powerState: "active" },
  lastButton: null, indicatorSent: { muted: false, cameraOff: false, inCall: false },
});

describe("applyEvent", () => {
  it("upserts and removes devices", () => {
    let s = applyEvent(initialState, { type: "deviceUpserted", device: dev("A") });
    expect(s.devices["A"].batteryPercent).toBe(80);
    s = applyEvent(s, { type: "deviceRemoved", id: "A" });
    expect(s.devices["A"]).toBeUndefined();
  });
  it("prepends logs and caps length", () => {
    let s = initialState;
    for (let i = 0; i < LOG_CAP + 10; i++) {
      s = applyEvent(s, { type: "log", entry: { ts: i, deviceId: null, direction: "in", message: `m${i}` } });
    }
    expect(s.log.length).toBe(LOG_CAP);
    expect(s.log[0].message).toBe(`m${LOG_CAP + 9}`); // newest first
  });
  it("tracks ble state", () => {
    const s = applyEvent(initialState, { type: "bleState", available: false, reason: "off" });
    expect(s.ble).toEqual({ available: false, reason: "off" });
  });
});
```

- [ ] **Step 3: Run to fail** — `cd software && npm test` → fails (no store.ts).

- [ ] **Step 4: store.ts**

Create `software/src/store.ts`:
```ts
import { create } from "zustand";
import type { AppEvent, Device, LogEntry } from "./types";

export const LOG_CAP = 500;

export interface AppState {
  devices: Record<string, Device>;
  log: LogEntry[];
  ble: { available: boolean; reason: string | null };
}
export const initialState: AppState = { devices: {}, log: [], ble: { available: true, reason: null } };

export function applyEvent(state: AppState, ev: AppEvent): AppState {
  switch (ev.type) {
    case "deviceUpserted":
      return { ...state, devices: { ...state.devices, [ev.device.id]: ev.device } };
    case "deviceRemoved": {
      const devices = { ...state.devices };
      delete devices[ev.id];
      return { ...state, devices };
    }
    case "log":
      return { ...state, log: [ev.entry, ...state.log].slice(0, LOG_CAP) };
    case "bleState":
      return { ...state, ble: { available: ev.available, reason: ev.reason } };
  }
}

interface Store extends AppState { apply: (ev: AppEvent) => void; }
export const useStore = create<Store>((set) => ({
  ...initialState,
  apply: (ev) => set((s) => applyEvent(s, ev)),
}));
```

- [ ] **Step 5: bridge.ts**

Create `software/src/bridge.ts`:
```ts
import { invoke } from "@tauri-apps/api/core";
import { listen } from "@tauri-apps/api/event";
import type { AppEvent, Device } from "./types";
import { useStore } from "./store";

export const api = {
  listDevices: () => invoke<Device[]>("list_devices"),
  scanStart: () => invoke<void>("scan_start"),
  scanStop: () => invoke<void>("scan_stop"),
  connect: (id: string) => invoke<void>("connect_device", { id }),
  disconnect: (id: string) => invoke<void>("disconnect_device", { id }),
  forget: (id: string) => invoke<void>("forget_device", { id }),
  sendIndicator: (id: string, muted: boolean, cameraOff: boolean, inCall: boolean) =>
    invoke<void>("send_indicator", { id, muted, cameraOff, inCall }),
  sendControl: (id: string, command: "identify" | "forgetBond") =>
    invoke<void>("send_control", { id, command }),
  quit: () => invoke<void>("quit_app"),
};

/** Subscribe the store to backend events. Returns an unlisten fn. */
export async function startEventBridge() {
  const un = await listen<AppEvent>("app-event", (e) => useStore.getState().apply(e.payload));
  const devices = await api.listDevices();
  for (const d of devices) useStore.getState().apply({ type: "deviceUpserted", device: d });
  return un;
}
```

- [ ] **Step 6: Run** — `cd software && npm test` → all pass.

- [ ] **Step 7: Commit**
```bash
git add software/src/types.ts software/src/store.ts software/src/bridge.ts software/src/store.test.ts
git commit -m "feat(software): frontend types, store reducer, Tauri bridge

<trailer>"
```

---

## Task 9: Frontend components — DeviceCard, EventLog, ScanBar, Header, App

**Files:** Create `src/components/{DeviceCard,EventLog,ScanBar,Header}.tsx`, `DeviceCard.test.tsx`, `EventLog.test.tsx`; rewrite `src/App.tsx`.

- [ ] **Step 1: DeviceCard.test.tsx (failing)**

Create `software/src/components/DeviceCard.test.tsx`:
```tsx
import { describe, it, expect, vi } from "vitest";
import { render, screen, fireEvent } from "@testing-library/react";
import { DeviceCard } from "./DeviceCard";
import type { Device } from "../types";

const dev: Device = {
  id: "A", name: "MiniMute", known: true, connection: "connected", error: null,
  batteryPercent: 73, status: { charging: true, lowBattery: false, powerState: "active" },
  lastButton: { code: "short", seq: 4 },
  indicatorSent: { muted: true, cameraOff: false, inCall: true },
};

describe("DeviceCard", () => {
  it("renders battery, status and last event", () => {
    render(<DeviceCard device={dev} on={{} as any} />);
    expect(screen.getByText(/73%/)).toBeInTheDocument();
    expect(screen.getByText(/short/i)).toBeInTheDocument();
  });
  it("calls onControl with identify", () => {
    const onControl = vi.fn();
    render(<DeviceCard device={dev} on={{ indicator: vi.fn(), control: onControl, disconnect: vi.fn(), forget: vi.fn() }} />);
    fireEvent.click(screen.getByRole("button", { name: /identify/i }));
    expect(onControl).toHaveBeenCalledWith("A", "identify");
  });
});
```

- [ ] **Step 2: Run to fail** — `cd software && npm test` → fails.

- [ ] **Step 3: DeviceCard.tsx**

Create `software/src/components/DeviceCard.tsx`:
```tsx
import type { Device } from "../types";

export interface DeviceHandlers {
  indicator: (id: string, muted: boolean, cameraOff: boolean, inCall: boolean) => void;
  control: (id: string, command: "identify" | "forgetBond") => void;
  disconnect: (id: string) => void;
  forget: (id: string) => void;
}

const dot: Record<Device["connection"], string> = {
  connected: "#2ecc71", disconnected: "#888", connecting: "#f1c40f", discovered: "#3498db", error: "#e74c3c",
};

export function DeviceCard({ device: d, on }: { device: Device; on: DeviceHandlers }) {
  const ind = d.indicatorSent;
  return (
    <div className="device-card">
      <div className="row">
        <span className="dot" style={{ background: dot[d.connection] }} />
        <strong>{d.name}</strong>
        <span className="batt">{d.batteryPercent != null ? `${d.batteryPercent}%` : "—"}{d.status.charging ? " ⚡" : ""}</span>
      </div>
      <div className="row small">
        {d.status.lowBattery ? "low battery · " : ""}power: {d.status.powerState}
        {d.lastButton ? ` · last: ${d.lastButton.code} (#${d.lastButton.seq})` : ""}
      </div>
      <div className="row toggles">
        <label><input type="checkbox" checked={ind.muted} onChange={(e) => on.indicator(d.id, e.target.checked, ind.cameraOff, ind.inCall)} /> Muted</label>
        <label><input type="checkbox" checked={ind.cameraOff} onChange={(e) => on.indicator(d.id, ind.muted, e.target.checked, ind.inCall)} /> Camera off</label>
        <label><input type="checkbox" checked={ind.inCall} onChange={(e) => on.indicator(d.id, ind.muted, ind.cameraOff, e.target.checked)} /> In call</label>
      </div>
      <div className="row actions">
        <button onClick={() => on.control(d.id, "identify")}>Identify</button>
        <button onClick={() => on.control(d.id, "forgetBond")}>Forget bond</button>
        <button onClick={() => on.disconnect(d.id)}>Disconnect</button>
        <button onClick={() => on.forget(d.id)}>Forget device</button>
      </div>
    </div>
  );
}
```

- [ ] **Step 4: EventLog.test.tsx + EventLog.tsx**

Create `software/src/components/EventLog.test.tsx`:
```tsx
import { describe, it, expect } from "vitest";
import { render, screen } from "@testing-library/react";
import { EventLog } from "./EventLog";

describe("EventLog", () => {
  it("renders entries newest-first as given", () => {
    render(<EventLog log={[
      { ts: 2, deviceId: "A", direction: "in", message: "button short" },
      { ts: 1, deviceId: "A", direction: "out", message: "identify" },
    ]} />);
    expect(screen.getByText(/button short/)).toBeInTheDocument();
    expect(screen.getByText(/identify/)).toBeInTheDocument();
  });
});
```
Create `software/src/components/EventLog.tsx`:
```tsx
import type { LogEntry } from "../types";
export function EventLog({ log }: { log: LogEntry[] }) {
  return (
    <div className="event-log">
      {log.map((e, i) => (
        <div key={i} className={`log-row ${e.direction}`}>
          <span className="ts">{new Date(e.ts).toLocaleTimeString()}</span>
          <span className="dir">{e.direction === "in" ? "←" : "→"}</span>
          <span className="msg">{e.message}</span>
        </div>
      ))}
    </div>
  );
}
```

- [ ] **Step 5: ScanBar.tsx + Header.tsx**

Create `software/src/components/ScanBar.tsx`:
```tsx
export function ScanBar({ scanning, onScan }: { scanning: boolean; onScan: () => void }) {
  return (
    <div className="scan-bar">
      <button onClick={onScan}>{scanning ? "Scanning…" : "Scan"}</button>
    </div>
  );
}
```
Create `software/src/components/Header.tsx`:
```tsx
export function Header({ bleAvailable, bleReason, autostart, onToggleAutostart, onQuit }:
  { bleAvailable: boolean; bleReason: string | null; autostart: boolean; onToggleAutostart: (v: boolean) => void; onQuit: () => void }) {
  return (
    <div className="header">
      <strong>MiniMute</strong>
      <span className={`ble ${bleAvailable ? "ok" : "bad"}`}>
        {bleAvailable ? "Bluetooth ready" : `Bluetooth: ${bleReason ?? "unavailable"}`}
      </span>
      <label><input type="checkbox" checked={autostart} onChange={(e) => onToggleAutostart(e.target.checked)} /> Start at login</label>
      <button onClick={onQuit}>Quit</button>
    </div>
  );
}
```

- [ ] **Step 6: App.tsx (wire store + bridge)**

Rewrite `software/src/App.tsx`:
```tsx
import { useEffect, useState } from "react";
import { useStore } from "./store";
import { api, startEventBridge } from "./bridge";
import { DeviceCard, type DeviceHandlers } from "./components/DeviceCard";
import { EventLog } from "./components/EventLog";
import { ScanBar } from "./components/ScanBar";
import { Header } from "./components/Header";
import { enable, isEnabled, disable } from "@tauri-apps/plugin-autostart";

export default function App() {
  const devices = useStore((s) => Object.values(s.devices));
  const log = useStore((s) => s.log);
  const ble = useStore((s) => s.ble);
  const [scanning, setScanning] = useState(false);
  const [autostart, setAutostart] = useState(false);

  useEffect(() => {
    const un = startEventBridge();
    isEnabled().then(setAutostart).catch(() => {});
    return () => { un.then((f) => f()).catch(() => {}); };
  }, []);

  const onScan = async () => { setScanning(true); await api.scanStart(); setTimeout(() => { api.scanStop(); setScanning(false); }, 8000); };
  const onToggleAutostart = async (v: boolean) => { v ? await enable() : await disable(); setAutostart(v); };

  const handlers: DeviceHandlers = {
    indicator: (id, m, c, i) => { api.sendIndicator(id, m, c, i); },
    control: (id, cmd) => { api.sendControl(id, cmd); },
    disconnect: (id) => { api.disconnect(id); },
    forget: (id) => { api.forget(id); },
  };

  return (
    <div className="app">
      <Header bleAvailable={ble.available} bleReason={ble.reason} autostart={autostart}
        onToggleAutostart={onToggleAutostart} onQuit={() => api.quit()} />
      <ScanBar scanning={scanning} onScan={onScan} />
      <div className="device-list">
        {devices.length === 0 ? <p className="empty">No devices. Click Scan.</p>
          : devices.map((d) => <DeviceCard key={d.id} device={d} on={handlers} />)}
      </div>
      <EventLog log={log} />
    </div>
  );
}
```
Install the autostart JS API if not already: `npm install @tauri-apps/plugin-autostart` (done in Task 6 step 3).

- [ ] **Step 7: Run** — `cd software && npm test` → DeviceCard + EventLog + store tests pass.

- [ ] **Step 8: Commit**
```bash
git add software/src
git commit -m "feat(software): console UI — device cards, event log, scan bar, header

<trailer>"
```

---

## Task 10: Full build + manual on-device verification

- [ ] **Step 1: Type-check + full builds**

Run: `cd software && npm test` (all frontend tests) and `cd software/src-tauri && cargo test` (all Rust tests) → all green.
Run: `cd software && npm run tauri build` (or `npm run tauri dev` for a quick run). Expected: builds a `.app`. Resolve any final wiring errors.

- [ ] **Step 2: Verify the bundle has the right plist keys**

Inspect the built `.app`'s `Info.plist` contains `LSUIElement = true` and `NSBluetoothAlwaysUsageDescription`. (The app should have no Dock icon when launched.)

- [ ] **Step 3: Manual on-device checklist (requires a MiniMute device)**

Run the app and verify (mark each):
- [ ] No Dock icon; a menubar icon appears (accessory app).
- [ ] Left-click the menubar icon shows the window; click again (or click away / lose focus) hides it; no right-click context menu.
- [ ] macOS prompts for Bluetooth permission on first scan; granting it lets scanning proceed.
- [ ] "Scan" discovers a MiniMute (advertising the service UUID); "Connect" connects (macOS handles pairing) and the card turns connected.
- [ ] Pressing the device button shows a Button event (short/long) in the card + event log with incrementing seq.
- [ ] Device Status and Battery updates appear (charging flips when plugging the device).
- [ ] Toggling Muted/Camera/In-call sends Indicator (device LED reflects it); "Identify" blinks the device; "Forget bond" triggers the device re-pair.
- [ ] Multiple MiniMute devices show as separate cards simultaneously.
- [ ] "Start at login" toggles the autostart LaunchAgent; relaunch confirms known devices auto-reconnect.
- [ ] "Quit" exits the app.

(No hardware in the dev environment → this checklist is performed by the user with a board. It doubles as firmware hardware validation.)

- [ ] **Step 4: Commit any final fixes**
```bash
git add -A software
git commit -m "chore(software): finalize build wiring and bundle config

<trailer>"
```

---

## Self-Review Notes (resolved during planning)

- **Spec coverage:** stack/Tauri → Task 1; BLE contract/codec → Task 2; device model + persistence → Task 3; notification dispatch + events + transport trait → Task 4; real btleplug → Task 5; commands + emission → Task 6; tray/window/accessory/autostart/Info.plist → Task 7; store + bridge → Task 8; console UI → Task 9; build + manual e2e (DoD 1-7) → Task 10. Auto-reconnect of known devices: registry `load_known` (Task 3) + transport scan/connect on discovery (Tasks 5-6); the load-known-on-startup persistence wiring is exercised in Task 10's relaunch check — if the executor finds startup load/save of the known-devices JSON is not yet wired, add it in Task 6/lib.rs (read on setup, write on `mark_known`/`forget`).
- **Contract sync:** `protocol.rs` mirrors `minimute_protocol.h`; tests pin byte values matching the firmware's `AppLinkProtocol` tests.
- **Type consistency:** Rust serde `rename_all = "camelCase"` matches `types.ts` fields (batteryPercent, lastButton, indicatorSent, powerState, lowBattery, cameraOff, inCall). `AppEvent` tag values (deviceUpserted/deviceRemoved/log/bleState) match the TS union. Command names match `bridge.ts` (`list_devices`, `scan_start`, …, `send_indicator` args muted/cameraOff/inCall, `send_control` arg `command`).
- **Glue tasks flagged:** btleplug (Task 5) and Tauri v2 (Tasks 6-7) APIs are version-sensitive — each carries an explicit "verify against API, adjust to compile" note and a `cargo build` gate; runtime behavior is covered by Task 10's manual checklist.
- **Known persistence:** the JSON load/save of known devices is the one piece intentionally left to wire during Task 6/10 (small, depends on Tauri path API) — called out above rather than left silent.
```

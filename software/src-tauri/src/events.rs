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

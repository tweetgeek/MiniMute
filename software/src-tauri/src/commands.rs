use std::sync::{Arc, OnceLock};
use tauri::{Emitter, State};
use crate::ble::{BleTransport, SharedDispatcher};
use crate::devices::Device;
use crate::protocol::{ControlCmd, IndicatorState, encode_control, encode_indicator, CHAR_CONTROL_UUID, CHAR_INDICATOR_UUID};

pub struct AppState {
    pub dispatcher: SharedDispatcher,
    pub transport: OnceLock<Arc<dyn BleTransport>>,
}

impl AppState {
    fn transport(&self) -> Result<Arc<dyn BleTransport>, String> {
        self.transport.get().cloned().ok_or_else(|| "BLE not ready yet".to_string())
    }
}

#[tauri::command]
pub async fn list_devices(state: State<'_, AppState>) -> Result<Vec<Device>, String> {
    Ok(state.dispatcher.lock().await.registry.list())
}
#[tauri::command]
pub async fn scan_start(state: State<'_, AppState>) -> Result<(), String> { state.transport()?.start_scan().await }
#[tauri::command]
pub async fn scan_stop(state: State<'_, AppState>) -> Result<(), String> { state.transport()?.stop_scan().await }
#[tauri::command]
pub async fn connect_device(state: State<'_, AppState>, id: String) -> Result<(), String> { state.transport()?.connect(&id).await }
#[tauri::command]
pub async fn disconnect_device(state: State<'_, AppState>, id: String) -> Result<(), String> { state.transport()?.disconnect(&id).await }

#[tauri::command]
pub async fn forget_device(app: tauri::AppHandle, state: State<'_, AppState>, id: String) -> Result<(), String> {
    if let Ok(t) = state.transport() { t.disconnect(&id).await.ok(); }
    {
        let mut d = state.dispatcher.lock().await;
        d.registry.mark_known(&id, false);
        d.registry.remove(&id);
    }
    // re-save known list (FIX 3)
    if let Some(p) = crate::known_path(&app) {
        let known = { state.dispatcher.lock().await.registry.known_devices() };
        crate::persist::save(&p, &known);
    }
    // FIX 2: emit DeviceRemoved so UI drops the card
    let _ = app.emit("app-event", &crate::events::AppEvent::DeviceRemoved { id });
    Ok(())
}

#[tauri::command]
pub async fn send_indicator(state: State<'_, AppState>, id: String, muted: bool, camera_off: bool, in_call: bool) -> Result<(), String> {
    let ind = IndicatorState { muted, camera_off, in_call };
    state.transport()?.write_char(&id, CHAR_INDICATOR_UUID, &[encode_indicator(ind)], false).await?;
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
    state.transport()?.write_char(&id, CHAR_CONTROL_UUID, &[encode_control(cmd)], true).await
}

#[tauri::command]
pub fn quit_app(app: tauri::AppHandle) { app.exit(0); }

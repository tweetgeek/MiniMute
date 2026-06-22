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

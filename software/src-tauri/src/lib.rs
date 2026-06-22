pub mod protocol;
pub mod devices;
pub mod events;
pub mod dispatch;
pub mod ble;
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
        .plugin(tauri_plugin_opener::init())
        .plugin(tauri_plugin_autostart::init(
            tauri_plugin_autostart::MacosLauncher::LaunchAgent, None))
        .setup(move |app| {
            let handle = app.handle().clone();
            let disp = dispatcher_for_setup.clone();
            tauri::async_runtime::spawn(async move {
                let sink: Arc<dyn EventSink> = Arc::new(TauriSink { app: handle.clone() });
                match BtleplugTransport::new(disp.clone(), sink).await {
                    Ok(transport) => {
                        let t: Arc<dyn crate::ble::BleTransport> = transport;
                        handle.manage(AppState { transport: t, dispatcher: disp });
                        if let Some(state) = handle.try_state::<AppState>() {
                            let _ = state.transport.start_scan().await;
                        }
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

#[cfg(test)]
mod sanity_tests {
  #[test]
  fn toolchain_runs() { assert_eq!(2 + 2, 4); }
}

pub mod protocol;
pub mod devices;
pub mod events;
pub mod dispatch;
pub mod ble;
pub mod commands;
pub mod persist;

use std::sync::{Arc, OnceLock};
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

pub(crate) fn known_path(app: &tauri::AppHandle) -> Option<std::path::PathBuf> {
    app.path().app_data_dir().ok().map(|d| d.join("known_devices.json"))
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    let dispatcher: SharedDispatcher = Arc::new(Mutex::new(Dispatcher::new()));
    let dispatcher_for_setup = dispatcher.clone();

    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .plugin(tauri_plugin_autostart::init(
            tauri_plugin_autostart::MacosLauncher::LaunchAgent, None))
        .on_window_event(|window, event| {
            match event {
                tauri::WindowEvent::CloseRequested { api, .. } => {
                    api.prevent_close();
                    let _ = window.hide();
                }
                tauri::WindowEvent::Focused(false) => { let _ = window.hide(); }
                _ => {}
            }
        })
        .setup(move |app| {
            #[cfg(target_os = "macos")]
            app.set_activation_policy(tauri::ActivationPolicy::Accessory);

            {
                use tauri::tray::{TrayIconBuilder, TrayIconEvent};
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
            }

            // FIX 1: manage AppState synchronously before the spawn so commands
            // work even when BLE transport isn't ready yet.
            let app_state = AppState {
                dispatcher: dispatcher_for_setup.clone(),
                transport: OnceLock::new(),
            };
            app.manage(app_state);

            let handle = app.handle().clone();
            let disp = dispatcher_for_setup.clone();
            tauri::async_runtime::spawn(async move {
                let sink: Arc<dyn EventSink> = Arc::new(TauriSink { app: handle.clone() });

                // FIX 3: load known devices from disk before constructing transport
                let kp = known_path(&handle);
                if let Some(ref p) = kp {
                    let known = crate::persist::load(p);
                    disp.lock().await.registry.load_known(known);
                }

                match BtleplugTransport::new(disp.clone(), sink, kp).await {
                    Ok(transport) => {
                        let t: Arc<dyn crate::ble::BleTransport> = transport;
                        let _ = handle.state::<AppState>().transport.set(t.clone());
                        let _ = t.start_scan().await;
                    }
                    Err(e) => {
                        let _ = handle.emit("app-event",
                            &AppEvent::BleState { available: false, reason: Some(e) });
                    }
                }
            });
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            commands::list_devices, commands::scan_start, commands::scan_stop,
            commands::connect_device, commands::disconnect_device, commands::forget_device,
            commands::send_indicator, commands::send_control,
            commands::quit_app,
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}

#[cfg(test)]
mod sanity_tests {
  #[test]
  fn toolchain_runs() { assert_eq!(2 + 2, 4); }
}

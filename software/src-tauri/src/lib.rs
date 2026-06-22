// Learn more about Tauri commands at https://tauri.app/develop/calling-rust/
pub mod protocol;
pub mod devices;
pub mod events;
pub mod dispatch;
pub mod ble;

#[tauri::command]
fn greet(name: &str) -> String {
    format!("Hello, {}! You've been greeted from Rust!", name)
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .invoke_handler(tauri::generate_handler![greet])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}

#[cfg(test)]
mod sanity_tests {
  #[test]
  fn toolchain_runs() { assert_eq!(2 + 2, 4); }
}

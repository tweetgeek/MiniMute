use std::path::PathBuf;
use crate::devices::KnownDevice;

pub fn load(path: &PathBuf) -> Vec<KnownDevice> {
    std::fs::read_to_string(path).ok()
        .and_then(|s| serde_json::from_str(&s).ok())
        .unwrap_or_default()
}

pub fn save(path: &PathBuf, known: &[KnownDevice]) {
    if let Some(dir) = path.parent() { let _ = std::fs::create_dir_all(dir); }
    if let Ok(s) = serde_json::to_string_pretty(known) { let _ = std::fs::write(path, s); }
}

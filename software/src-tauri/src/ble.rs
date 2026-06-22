use async_trait::async_trait;

/// Outgoing BLE operations. Implemented by btleplug (later) and by a fake in tests.
/// Incoming notifications are routed into Dispatcher::on_* separately.
#[async_trait]
pub trait BleTransport: Send + Sync {
    async fn start_scan(&self) -> Result<(), String>;
    async fn stop_scan(&self) -> Result<(), String>;
    async fn connect(&self, id: &str) -> Result<(), String>;
    async fn disconnect(&self, id: &str) -> Result<(), String>;
    async fn write_char(&self, id: &str, char_uuid: &str, data: &[u8], with_response: bool) -> Result<(), String>;
}

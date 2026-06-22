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

use std::sync::Arc;
use tokio::sync::Mutex;
use crate::dispatch::Dispatcher;
use crate::events::AppEvent;

/// Anything that can deliver an AppEvent to the UI (abstracts tauri::AppHandle).
pub trait EventSink: Send + Sync + 'static {
    fn emit(&self, event: &AppEvent);
}

pub type SharedDispatcher = Arc<Mutex<Dispatcher>>;

use btleplug::api::{Central, CentralEvent, Manager as _, Peripheral as _, ScanFilter, WriteType};
use btleplug::platform::{Adapter, Manager, Peripheral};
use futures::StreamExt;
use uuid::Uuid;

pub struct BtleplugTransport {
    adapter: Adapter,
    dispatcher: SharedDispatcher,
    sink: Arc<dyn EventSink>,
    known_path: Option<std::path::PathBuf>,
}

impl BtleplugTransport {
    pub async fn new(
        dispatcher: SharedDispatcher,
        sink: Arc<dyn EventSink>,
        known_path: Option<std::path::PathBuf>,
    ) -> Result<Arc<Self>, String> {
        let manager = Manager::new().await.map_err(|e| e.to_string())?;
        let adapters = manager.adapters().await.map_err(|e| e.to_string())?;
        let adapter = adapters.into_iter().next().ok_or("no BLE adapter")?;
        let me = Arc::new(Self { adapter, dispatcher, sink, known_path });
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

    /// FIX 3: persist known devices to disk after every connect/forget.
    async fn save_known(&self) {
        if let Some(p) = &self.known_path {
            let known = { self.dispatcher.lock().await.registry.known_devices() };
            crate::persist::save(p, &known);
        }
    }

    async fn spawn_event_loop(self: Arc<Self>) -> Result<(), String> {
        let mut events = self.adapter.events().await.map_err(|e| e.to_string())?;
        let this = self.clone();
        tokio::spawn(async move {
            while let Some(ev) = events.next().await {
                match ev {
                    CentralEvent::DeviceDiscovered(pid) | CentralEvent::DeviceUpdated(pid) => {
                        if let Ok(p) = this.adapter.peripheral(&pid).await {
                            let props = p.properties().await.ok().flatten();
                            let advertises = props.as_ref()
                                .map(|pr| pr.services.iter().any(|u| *u == Self::parse_uuid(crate::protocol::SERVICE_UUID)))
                                .unwrap_or(false);
                            let name = props.and_then(|pr| pr.local_name).unwrap_or_else(|| "MiniMute".into());
                            if advertises {
                                let evs = { this.dispatcher.lock().await.on_discovered(&pid.to_string(), &name) };
                                for e in &evs { this.sink.emit(e); }
                            }
                        }
                    }
                    CentralEvent::DeviceConnected(pid) => {
                        let evs = { this.dispatcher.lock().await.on_connected(&pid.to_string()) };
                        for e in &evs { this.sink.emit(e); }
                        // FIX 3: save known list after marking device known on connect
                        this.save_known().await;
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

        // FIX 5: initial read of Status + Battery after subscribe
        for ch in p.characteristics() {
            if ch.uuid == Self::parse_uuid(crate::protocol::CHAR_STATUS_UUID)
                || ch.uuid == Self::parse_uuid(crate::protocol::BATTERY_LEVEL_UUID)
            {
                if let Ok(val) = p.read(&ch).await {
                    let evs = { self.dispatcher.lock().await.on_notification(id, &ch.uuid.to_string(), &val) };
                    for e in &evs { self.sink.emit(e); }
                }
            }
        }

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

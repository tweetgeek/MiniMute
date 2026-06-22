import { useEffect, useState } from "react";
import { useStore } from "./store";
import { api, startEventBridge } from "./bridge";
import { DeviceCard, type DeviceHandlers } from "./components/DeviceCard";
import { EventLog } from "./components/EventLog";
import { ScanBar } from "./components/ScanBar";
import { Header } from "./components/Header";
import { enable, isEnabled, disable } from "@tauri-apps/plugin-autostart";
import "./App.css";

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

  const onScan = async () => { if (scanning) return; setScanning(true); await api.scanStart(); setTimeout(() => { api.scanStop(); setScanning(false); }, 8000); };
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

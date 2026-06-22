import type { Device } from "../types";

export interface DeviceHandlers {
  indicator: (id: string, muted: boolean, cameraOff: boolean, inCall: boolean) => void;
  control: (id: string, command: "identify" | "forgetBond") => void;
  disconnect: (id: string) => void;
  forget: (id: string) => void;
}

const dot: Record<Device["connection"], string> = {
  connected: "#2ecc71", disconnected: "#888", connecting: "#f1c40f", discovered: "#3498db", error: "#e74c3c",
};

export function DeviceCard({ device: d, on }: { device: Device; on: DeviceHandlers }) {
  const ind = d.indicatorSent;
  return (
    <div className="device-card">
      <div className="row">
        <span className="dot" style={{ background: dot[d.connection] }} />
        <strong>{d.name}</strong>
        <span className="batt">{d.batteryPercent != null ? `${d.batteryPercent}%` : "—"}{d.status.charging ? " ⚡" : ""}</span>
      </div>
      <div className="row small">
        {d.status.lowBattery ? "low battery · " : ""}power: {d.status.powerState}
        {d.lastButton ? ` · last: ${d.lastButton.code} (#${d.lastButton.seq})` : ""}
      </div>
      <div className="row toggles">
        <label><input type="checkbox" checked={ind.muted} onChange={(e) => on.indicator(d.id, e.target.checked, ind.cameraOff, ind.inCall)} /> Muted</label>
        <label><input type="checkbox" checked={ind.cameraOff} onChange={(e) => on.indicator(d.id, ind.muted, e.target.checked, ind.inCall)} /> Camera off</label>
        <label><input type="checkbox" checked={ind.inCall} onChange={(e) => on.indicator(d.id, ind.muted, ind.cameraOff, e.target.checked)} /> In call</label>
      </div>
      <div className="row actions">
        <button onClick={() => on.control(d.id, "identify")}>Identify</button>
        <button onClick={() => on.control(d.id, "forgetBond")}>Forget bond</button>
        <button onClick={() => on.disconnect(d.id)}>Disconnect</button>
        <button onClick={() => on.forget(d.id)}>Forget device</button>
      </div>
    </div>
  );
}

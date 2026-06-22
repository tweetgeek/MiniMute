export function Header({ bleAvailable, bleReason, autostart, onToggleAutostart, onQuit }:
  { bleAvailable: boolean; bleReason: string | null; autostart: boolean; onToggleAutostart: (v: boolean) => void; onQuit: () => void }) {
  return (
    <div className="header">
      <strong>MiniMute</strong>
      <span className={`ble ${bleAvailable ? "ok" : "bad"}`}>
        {bleAvailable ? "Bluetooth ready" : `Bluetooth: ${bleReason ?? "unavailable"}`}
      </span>
      <label><input type="checkbox" checked={autostart} onChange={(e) => onToggleAutostart(e.target.checked)} /> Start at login</label>
      <button onClick={onQuit}>Quit</button>
    </div>
  );
}

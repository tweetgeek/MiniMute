export function ScanBar({ scanning, onScan }: { scanning: boolean; onScan: () => void }) {
  return (
    <div className="scan-bar">
      <button onClick={onScan} disabled={scanning}>{scanning ? "Scanning…" : "Scan"}</button>
    </div>
  );
}

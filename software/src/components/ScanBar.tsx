export function ScanBar({ scanning, onScan }: { scanning: boolean; onScan: () => void }) {
  return (
    <div className="scan-bar">
      <button onClick={onScan}>{scanning ? "Scanning…" : "Scan"}</button>
    </div>
  );
}

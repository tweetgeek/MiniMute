import type { LogEntry } from "../types";
export function EventLog({ log }: { log: LogEntry[] }) {
  return (
    <div className="event-log">
      {log.map((e, i) => (
        <div key={i} className={`log-row ${e.direction}`}>
          <span className="ts">{new Date(e.ts).toLocaleTimeString()}</span>
          <span className="dir">{e.direction === "in" ? "←" : "→"}</span>
          <span className="msg">{e.message}</span>
        </div>
      ))}
    </div>
  );
}

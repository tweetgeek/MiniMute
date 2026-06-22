export type ButtonCode = "short" | "long" | "double";
export interface ButtonEvent { code: ButtonCode; seq: number; }
export type PowerState = "active" | "connectedIdle";
export interface DeviceStatus { charging: boolean; lowBattery: boolean; powerState: PowerState; }
export interface IndicatorState { muted: boolean; cameraOff: boolean; inCall: boolean; }
export type ConnectionKind = "discovered" | "connecting" | "connected" | "disconnected" | "error";
export interface Device {
  id: string; name: string; known: boolean;
  connection: ConnectionKind; error: string | null;
  batteryPercent: number | null; status: DeviceStatus;
  lastButton: ButtonEvent | null; indicatorSent: IndicatorState;
}
export interface LogEntry { ts: number; deviceId: string | null; direction: "in" | "out"; message: string; }
export type AppEvent =
  | { type: "deviceUpserted"; device: Device }
  | { type: "deviceRemoved"; id: string }
  | { type: "log"; entry: LogEntry }
  | { type: "bleState"; available: boolean; reason: string | null };

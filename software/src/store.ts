import { create } from "zustand";
import type { AppEvent, Device, LogEntry } from "./types";

export const LOG_CAP = 500;

export interface AppState {
  devices: Record<string, Device>;
  log: LogEntry[];
  ble: { available: boolean; reason: string | null };
}
export const initialState: AppState = { devices: {}, log: [], ble: { available: true, reason: null } };

export function applyEvent(state: AppState, ev: AppEvent): AppState {
  switch (ev.type) {
    case "deviceUpserted":
      return { ...state, devices: { ...state.devices, [ev.device.id]: ev.device } };
    case "deviceRemoved": {
      const devices = { ...state.devices };
      delete devices[ev.id];
      return { ...state, devices };
    }
    case "log":
      return { ...state, log: [ev.entry, ...state.log].slice(0, LOG_CAP) };
    case "bleState":
      return { ...state, ble: { available: ev.available, reason: ev.reason } };
  }
}

interface Store extends AppState { apply: (ev: AppEvent) => void; }
export const useStore = create<Store>((set) => ({
  ...initialState,
  apply: (ev) => set((s) => applyEvent(s, ev)),
}));

import { invoke } from "@tauri-apps/api/core";
import { listen } from "@tauri-apps/api/event";
import type { AppEvent, Device } from "./types";
import { useStore } from "./store";

export const api = {
  listDevices: () => invoke<Device[]>("list_devices"),
  scanStart: () => invoke<void>("scan_start"),
  scanStop: () => invoke<void>("scan_stop"),
  connect: (id: string) => invoke<void>("connect_device", { id }),
  disconnect: (id: string) => invoke<void>("disconnect_device", { id }),
  forget: (id: string) => invoke<void>("forget_device", { id }),
  sendIndicator: (id: string, muted: boolean, cameraOff: boolean, inCall: boolean) =>
    invoke<void>("send_indicator", { id, muted, cameraOff, inCall }),
  sendControl: (id: string, command: "identify" | "forgetBond") =>
    invoke<void>("send_control", { id, command }),
  quit: () => invoke<void>("quit_app"),
};

/** Subscribe the store to backend events. Returns an unlisten fn. */
export async function startEventBridge() {
  const un = await listen<AppEvent>("app-event", (e) => useStore.getState().apply(e.payload));
  const devices = await api.listDevices();
  for (const d of devices) useStore.getState().apply({ type: "deviceUpserted", device: d });
  return un;
}

import { describe, it, expect } from "vitest";
import { applyEvent, initialState, LOG_CAP } from "./store";
import type { Device } from "./types";

const dev = (id: string): Device => ({
  id, name: "MiniMute", known: true, connection: "connected", error: null,
  batteryPercent: 80, status: { charging: false, lowBattery: false, powerState: "active" },
  lastButton: null, indicatorSent: { muted: false, cameraOff: false, inCall: false },
});

describe("applyEvent", () => {
  it("upserts and removes devices", () => {
    let s = applyEvent(initialState, { type: "deviceUpserted", device: dev("A") });
    expect(s.devices["A"].batteryPercent).toBe(80);
    s = applyEvent(s, { type: "deviceRemoved", id: "A" });
    expect(s.devices["A"]).toBeUndefined();
  });
  it("prepends logs and caps length", () => {
    let s = initialState;
    for (let i = 0; i < LOG_CAP + 10; i++) {
      s = applyEvent(s, { type: "log", entry: { ts: i, deviceId: null, direction: "in", message: `m${i}` } });
    }
    expect(s.log.length).toBe(LOG_CAP);
    expect(s.log[0].message).toBe(`m${LOG_CAP + 9}`);
  });
  it("tracks ble state", () => {
    const s = applyEvent(initialState, { type: "bleState", available: false, reason: "off" });
    expect(s.ble).toEqual({ available: false, reason: "off" });
  });
});

import { describe, it, expect, vi } from "vitest";
import { render, screen, fireEvent } from "@testing-library/react";
import { DeviceCard } from "./DeviceCard";
import type { Device } from "../types";

const dev: Device = {
  id: "A", name: "MiniMute", known: true, connection: "connected", error: null,
  batteryPercent: 73, status: { charging: true, lowBattery: false, powerState: "active" },
  lastButton: { code: "short", seq: 4 },
  indicatorSent: { muted: true, cameraOff: false, inCall: true },
};

describe("DeviceCard", () => {
  it("renders battery, status and last event", () => {
    render(<DeviceCard device={dev} on={{ indicator: vi.fn(), control: vi.fn(), disconnect: vi.fn(), forget: vi.fn() }} />);
    expect(screen.getByText(/73%/)).toBeInTheDocument();
    expect(screen.getByText(/short/i)).toBeInTheDocument();
  });
  it("calls onControl with identify", () => {
    const onControl = vi.fn();
    render(<DeviceCard device={dev} on={{ indicator: vi.fn(), control: onControl, disconnect: vi.fn(), forget: vi.fn() }} />);
    fireEvent.click(screen.getByRole("button", { name: /identify/i }));
    expect(onControl).toHaveBeenCalledWith("A", "identify");
  });
});

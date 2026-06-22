import { describe, it, expect } from "vitest";
import { render, screen } from "@testing-library/react";
import { EventLog } from "./EventLog";

describe("EventLog", () => {
  it("renders entries as given", () => {
    render(<EventLog log={[
      { ts: 2, deviceId: "A", direction: "in", message: "button short" },
      { ts: 1, deviceId: "A", direction: "out", message: "identify" },
    ]} />);
    expect(screen.getByText(/button short/)).toBeInTheDocument();
    expect(screen.getByText(/identify/)).toBeInTheDocument();
  });
});

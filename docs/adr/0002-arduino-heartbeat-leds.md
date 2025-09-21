# ADR 0002: Arduino Heartbeat LED Policy

## Status

Accepted

## Context

Phase 8 adds a dual-LED heartbeat (D5 green, D6 red) to signal link health between the Python daemon and the Arduino UI. The daemon emits telemetry frames at a configurable `interval` (seconds) and includes a `META interval=<seconds>` header so the Arduino can adjust its watchdog. We needed:

1. Deterministic LED patterns for healthy, stale, lost, and command-ack states without blocking delays.
2. A loss timeout that handles jitter, serial latency, and occasional pauses on the host without flapping into the "waiting" screen too early.
3. Behavior that stays in sync with the existing LCD watchdog messaging so users get consistent signals from both display and LEDs.

## Decision

- Parse the `META interval` and store it as `heartbeatIntervalMs`. Recalculate the loss timeout (`frameTimeoutMs`) as `clamp(10 × interval, 5 s, 60 s)`. The stale threshold is `max(2 × interval, 500 ms)` but never exceeds the loss timeout.
- Drive the LEDs via a non-blocking `millis()` state machine:
  - Healthy: every telemetry (or metadata keepalive) frame triggers a ~120 ms pulse on the green LED (D5); red stays LOW.
  - Stale: when time since last frame exceeds `2 × interval` but is still below the loss timeout, flash the red LED (D6) for ~50 ms once per second so the device shows it is alive but waiting.
  - Lost: when time since last frame exceeds the loss timeout, fall back to the existing "Waiting for data…" LCD mode and reuse its spinner cadence (250 ms steps) to toggle the red LED. Green remains off.
  - Command acknowledgement: whenever a `SELECT <id>` is sent, pulse the red LED for ~150 ms regardless of state.
- Avoid changes to LCD rendering and keep all LED updates opaque to the rest of the UI logic.

### Rationale for 10× interval loss timeout

- **Stability**: Stretched threshold prevents false negatives caused by host jitter, serial buffering, or short pauses (e.g., thermal throttle, manual diagnostics).
- **Distinct staging**: Using 2× for stale and 10× for lost creates clear, easy-to-explain bands (healthy < 2×, stale 2×–10×, lost > 10×).
- **Bounded behavior**: Clamping to 5–60 s ensures sensible output if intervals are extremely small or large.

## Consequences

- Healthy behavior feels responsive even at short intervals (green pulses per frame). During outages, both LCD and LEDs transition together, reducing confusion.
- Manual testing remains required: observers must verify LED transitions while starting/stopping the daemon and sending commands.
- If future requirements demand tighter detection or user-configurable thresholds, the multiplier and clamping rules can move into a configuration struct (e.g., via `config.h` or protocol extension).


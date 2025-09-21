### Phase 0: Setup & Environment

- [x] Scaffold monorepo with `/arduino`, `/server`, `/docs`.
- [x] Configure PlatformIO for Arduino Nano and LiquidCrystal.
- [x] Configure Python project with uv, pytest, Ruff, mypy (strict).
- [x] Add Makefile targets: `fmt`, `fmt-check`, `lint`, `type`, `test`, `ci`, `e2e`, `setup`.
- [x] Add GitHub Actions workflows (Python, Arduino). Currently manual via `workflow_dispatch`; enable push/PR triggers in a later phase.

### Phase 1: Arduino Proof of Concept

- [x] Implement minimal Arduino sketch that prints “Hello World” on LCD.
- [x] Document wiring in `/docs/wiring.md` (Nano pins to LCD pins).
- [x] Commit schematic and pin mapping (pin table documented in wiring.md).

### Phase 2: Arduino Input Handling

- [x] Add rotary encoder support (rotation + button pin setup).
- [x] Implement memory buffer in Arduino code for scrollable LCD lines.
- [x] Unit test buffer logic with PlatformIO tests.

### Phase 3: Server Mockup

- [x] Write Python script sending mock serial data to Arduino every 5s (`server/src/mock_sender.py`).
- [x] Use simple line-framed protocol with blank line terminator (`server/src/protocol.py`).
- [x] Add Makefile e2e target to run mock against hardware (`make e2e PORT=/dev/ttyACM0`).
- [x] Verified on hardware: Arduino receives frames and echoes debug.
-

### Phase 4: Arduino Serial Integration

- [x] Add Arduino serial handler to parse server mockup input (newline-separated lines, blank line as frame end).
- [x] Display first 4 lines on LCD, scrollable via rotary encoder.
- [x] Watchdog reset to "Waiting for data..." after timeout if stream stops.
- [x] Verified on hardware with mock sender.


### Phase 5: Server Sensor Reader

- [x] Define YAML config schema and loader (`server/src/config.py`).
- [x] Implement daemon with dry-run/once and serial output (`server/src/main.py`).
- [x] Update example config (`server/config.example.yaml`).
- [x] Add unit tests for config parsing and dry-run assembly (`server/tests`).
- [x] Add GPU fallback via `nvidia-smi` when NVML unavailable.
- [x] Add minimal logging (default ERROR, `--verbose` for INFO). Metrics hooks TBD.
- [x] Prepend `META interval=<seconds>` to each telemetry frame so Arduino can scale its watchdog dynamically.
-

### Phase 6: Commands UI & Protocol (logging only)

- [x] Server: respond to `REQ COMMANDS` with a `COMMANDS v1` frame assembled from config.
- [x] Server: on `SELECT <id>`, log `selected id=<id> label=<label>` (no execution).
- [x] Config: define `commands: [{id, label}]` and update `server/config.example.yaml`.
- [x] Tests: unit tests for commands frame assembly and select logging handler.
- [x] Docs: add ADR/update for commands protocol specifics.
- [x] Arduino (next subphase): add Commands mode UI (cursor, scroll) and `REQ COMMANDS` on enter.

### Phase 6.4: Service Packaging (System mode)

- [x] Provide a systemd system service unit (root-managed) with hardening defaults.
- [x] Add Makefile targets with install instructions for system mode.
- [x] Document deployment steps (service user, dialout group, venv in /opt, /etc/default env file).
- [x] Add `service-system-update` target, pip cache prep, and sudo documentation; default exec driver uses shell with `sudo -n` guidance.

### Phase 7: Hardening & Docs

- [x] Arduino: recover from serial outages in any mode and show animated “Waiting for data …” indicator.
- [x] Arduino: replace LCD sizing magic numbers, split `commitFrameIfAny` into helpers, and harden millis() comparisons (`arduino/src/main.cpp`).
- [ ] Python: adopt structured logging (e.g., JSON adapters) for daemon events.
- [x] Python: add retry backoff for serial init failures so the service can start before hardware is connected.
- [ ] Docs: add troubleshooting section covering serial loss recovery and sudo configuration examples.
- [ ] Instrument daemon with metrics (OpenTelemetry optional) and document monitoring hooks.
- [ ] Hardware verification pending: confirm LED pulses, command navigation, and frame handling after the refactor on connected hardware.

### Phase 8: Heartbeat Controller

- [x] Drive D5 green/D6 red LEDs with non-blocking heartbeat (healthy pulse, stale flash, lost spinner, command ACK pulse) synced with telemetry META intervals.

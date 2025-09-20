### Phase 0: Setup & Environment

- Scaffold monorepo with `/arduino` and `/server` directories.
- Configure PlatformIO project for Arduino Nano.
- Configure Python project with uv, pytest, and Ruff.
- Add Makefile with `fmt`, `lint`, `type`, `test`, `ci` targets.
- Set up GitHub Actions CI pipeline for both Arduino and Python.

### Phase 1: Arduino Proof of Concept

- Implement minimal Arduino sketch that prints “Hello World” on LCD.
- Document wiring in `/docs/wiring.md` (Nano pins to LCD pins).
- Commit schematic and pin mapping.

### Phase 2: Arduino Input Handling

- Add rotary encoder support (rotation + button press).
- Implement memory buffer in Arduino code for scrollable LCD lines.
- Unit test buffer logic with PlatformIO tests.

### Phase 3: Server Mockup

- Write Python script sending mock serial data to Arduino every 5s.
- Data structure: multiple lines (≥3×20 chars), include counter for visible change.
- Verify display of incoming serial data on Arduino.

### Phase 4: Arduino Serial Integration

- Add Arduino serial handler to parse server mockup input.
- Display first 4 lines on LCD, scrollable using encoder.
- Test with Python mockup.

### Phase 5: Server Sensor Reader

- Design config file format (YAML or TOML). Fields: sensors to monitor, thresholds, polling interval.
- Implement Python daemon to read system metrics (CPU temp, GPU load, memory usage, etc).
- Ensure daemon sends structured serial output matching Arduino buffer logic.
- Add unit tests for config parsing and metric collection.
- here are the details:
  Phase 5 plan details (no code changes yet)

    5.1 Config schema (YAML)

    Define sensors to enable, update interval, and output layout.
    Suggested fields:
    top-level: interval, serial: {port, baud}, max_lines: 12
    sensors: list of sensor blocks with:
    name: display label (e.g., CPU, GPU)
    provider: which backend to use (e.g., cpu, mem, temp, nvml, nvidia_smi)
    enabled: true/false
    format: string template truncated to 20 chars (e.g., "CPU {util}% {temp}C {mem}%")
    params: provider-specific options (e.g., gpu index)
    Use graceful defaults; unknown sensors ignored with a warning.
    5.2 Config loader and validation

    Use stdlib dataclasses + type hints to avoid heavy deps.
    Add PyYAML for parsing.
    Validate fields; set defaults if missing; keep it robust under mypy strict.
    5.3 Sensor providers (pluggable, optional)

    CPU/memory/temps via psutil (works everywhere).
    GPU via:
    Preferred: pynvml if available.
    Fallback: nvidia-smi subprocess parse if configured and present.
    If neither available or disabled: return None and skip in output.
    Providers return short strings, never exceeding 20 chars after formatting/truncation.
    5.4 Assembler + framing

    Assemble sensor lines in config order.
    Clamp to 12 lines, 20 chars each.
    Reuse Outbound.encode() and send to serial.
    5.5 “Dry-run” display mode

    Add a --dry-run flag that prints the lines to stdout instead of serial. This lets you validate formatting on dev machines w/o Arduino.
    5.6 Tests

    Unit tests for config parsing (enabled/disabled sensors, missing YAML fields).
    Unit tests for providers (mock psutil, mock pynvml, simulate absence).
    Tests for line assembly and clamping.
    5.7 Docs

    Update server/config.example.yaml with realistic examples (with and without GPU).
    Add notes on optional GPU support and how to enable/disable per machine.

  Resolved decisions

    - PyYAML is added for config parsing; maintain strict mypy coverage.
    - Default layout emits a metadata line plus up to 11 telemetry lines; the metadata (`META interval=<seconds>`) lets Arduino scale its watchdog while the remaining lines feed the scroll buffer.
    - GPU support prefers `pynvml` with an `nvidia-smi` fallback when NVML is unavailable.

### Phase 6: Commands UI & Execution (multi-step)

6.1 Arduino UI (modes, cursor, navigation)

- Two modes: Telemetry and Commands. Telemetry shows only telemetry; Commands shows only commands.
- Enter Commands: long press in Telemetry. Exit Commands: select the built-in `Exit` item (always appended) or long press (safety).
- Commands list: labels from server; render with a cursor `>` at column 0 for the selected line.
- Rotary in Commands: moves selection; window auto-scrolls to keep cursor visible. Selection index range is [0..N], where N is the `Exit` entry.
- Double press in Commands: send the selected command (see protocol). Selecting `Exit` returns to Telemetry (no message) and resets Telemetry scroll to top.
- Data structures: keep telemetry buffer unchanged; add a separate commands array (id + 19-char label), plus `cursor_index` and `window_start`.
- Connection monitoring: Arduino maintains a watchdog for telemetry frames in both modes; on timeout it switches to an animated "Waiting for data…" screen and returns to Telemetry once frames resume.

6.2 Server protocol (logging only; no execution)

- Framing: newline-separated frames with blank-line terminator.
- Commands frame (server → Arduino): first line `COMMANDS v1`, followed by one command per line as `<id> <label>` (server truncates to fit 20 chars).
- Arduino requests: `REQ COMMANDS` when entering Commands mode; Arduino switches to waiting view until the frame arrives.
- Selection (Arduino → server): `SELECT <id>` on double press. Selecting `Exit` sends nothing.
- Server behavior: on `REQ COMMANDS`, send current Commands frame; on `SELECT <id>`, log `selected id=<id> label=<label>`. Ignore all non-command lines.
- Refresh strategy: do not spam commands; send on request. Device reboot is covered because entering Commands mode triggers a request.

6.3 Command execution (deferred)

- Config: whitelist mapping `{id -> label, exec}`; no free-form args by default; allow fixed args only.
- Execution: prefer systemd units or sudoers rules tailored to these commands to avoid running as root broadly. Alternatively, a small privileged helper with a strict allowlist.
- Security: never execute unknown IDs; validate config on load; log structured results; redact sensitive output.
- Tests: integration with dummy commands; simulate failures and permission denials.

6.4 Service packaging

- Provide a systemd unit for the server daemon (config path, env, restart policy).
- Serial permissions via group membership or udev rule; document in `/docs`.
- Logging to journald; optional rotation and metrics later.

### Phase 7: Hardening & Docs

- ✅ Hardened systemd deployment: default exec driver uses shell + `sudo -n`, service update target added, pip cache prepared, and documentation refreshed. Systemd unit keeps `NoNewPrivileges=no` to allow scoped sudo while other hardening flags stay in place.
- ✅ Arduino resiliency: command view now tracks serial link loss, auto-returns to telemetry on recovery, and shows an animated waiting indicator during outages.
- ✅ Python serial resiliency: daemon retries opening the serial port with exponential backoff (5s → 30s capped) and state-aware logging to avoid systemd churn.
- ☐ Add structured logging + rate-limited error handling in Python daemon (beyond the initial serial backoff).
- ☐ Expand `/docs` with troubleshooting for hardware disconnects and sudo configuration examples.
- ☐ Implement monitoring and metrics for Python daemon (OpenTelemetry optional).
- ☐ Finalize ADRs for architecture choices.

### Phase 8 (Optional): Heartbeat Controller (LED on D5)

- Goal: Hardware link-health indicator with zero LCD changes.
- Hardware: drive an external LED on `D5` via 220Ω to GND (preferred). Optionally switch to the built-in `D13` LED if wiring is not desired.
- States & patterns (non-blocking):
  - OK (fresh frames ≤ 2× interval): single short blink at 1 Hz (≈80 ms ON per second).
  - Stale (> 2× to 10× interval): double-blink every 2 s (two ≈80 ms pulses, 150 ms apart).
  - Lost (> 10× interval): LED OFF (optionally a very slow 0.2 Hz blink if desired later).
- Implementation: small millis()-based scheduler/state machine; update `lastFrameMs` on both data and heartbeat frames; no `delay()`; no LCD changes.
- Config: fixed thresholds initially; future optional tuning via server config if needed.
- Verification: manual check of LED patterns while stopping/starting the server; keep Arduino resource use minimal.

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
    Questions to confirm before I start

    OK to add PyYAML as a dependency for config parsing?
    Do you want a single default layout (e.g., 4 key lines) or a longer 10–12 line page that you scroll?
    For GPU, prefer pynvml first with fallback to nvidia-smi, or nvidia-smi only?

### Phase 6: Command Execution Loop

- Extend Arduino to display incoming command list with IDs.
- On encoder button press, Arduino sends selected command ID back to server.
- Extend Python daemon to map IDs to predefined system commands (shutdown, restart, start process).
- Secure execution by whitelisting commands in config.
- Add integration tests with dummy commands.

### Phase 7: Hardening & Docs

- Add error handling, logging, and recovery in Arduino and Python code.
- Expand `/docs` with usage instructions, wiring diagrams, config examples.
- Implement monitoring and metrics for Python daemon (OpenTelemetry optional).
- Finalize ADRs for architecture choices.

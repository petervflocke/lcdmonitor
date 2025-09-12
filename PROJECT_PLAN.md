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

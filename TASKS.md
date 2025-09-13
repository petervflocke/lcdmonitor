### Phase 0: Setup & Environment

- [x] Scaffold monorepo with `/arduino`, `/server`, `/docs`.
- [x] Configure PlatformIO for Arduino Nano and LiquidCrystal.
- [x] Configure Python project with uv, pytest, Ruff, mypy (strict).
- [x] Add Makefile targets: `fmt`, `fmt-check`, `lint`, `type`, `test`, `ci`, `e2e`, `setup`.
- [x] Define manual CI process using Makefile targets (no GitHub Actions). Run `make ci` locally before PRs.

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
-

### Phase 5: Server Sensor Reader

- [x] Define YAML config schema and loader (`server/src/config.py`).
- [x] Implement daemon with dry-run/once and serial output (`server/src/main.py`).
- [x] Update example config (`server/config.example.yaml`).
- [x] Add unit tests for config parsing and dry-run assembly (`server/tests`).
- [ ] Add GPU fallback via `nvidia-smi` when NVML unavailable.
- [ ] Add logging/metrics and docs updates.
-

### Phase 6: Command Execution Loop

-

### Phase 7: Hardening & Docs

- 

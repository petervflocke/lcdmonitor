## Project goals

- Develop a two-part system:
  - **Element 1**: Arduino Nano with 20x4 LCD (HD44780 compatible) and rotary encoder for display and selection.
  - **Element 2**: Linux server daemon in Python that reads system and GPU metrics, sends data to Element 1, and executes commands received from it.
- Ship features incrementally in defined phases, keeping tests, CI, and docs green. Prioritize reliability, maintainability, and security.

## Development phases (high-level)

- **Element 1 (Arduino)**
  1. Verify wiring by showing “Hello World” on LCD.
  2. Add encoder support and memory buffer for scrollable LCD lines.
  3. Receive and display serial data from server mockup.
  4. Enable scroll through buffered data.
  5. Add option selection and command send-back to server.
- **Element 2 (Server Python)**
  1. Write Python mockup sending strings via serial every 5s.
  2. Implement config-driven sensor reader (CPU, GPU, temp, memory etc).
  3. Define config file format (to be recommended by Codex).
  4. Add command execution module triggered by Arduino input.

  ## Languages and tooling

- **Element 1**: Arduino C++ with PlatformIO in VS Code.
- **Element 2**: Python 3.12 in VS Code, uv/pip for deps.
- Shared: Docker compose for integration testing.
- Formatters and linters: Black, Ruff, mypy strict (Python). Arduino code: clang-format with project config.
- Tests: pytest for server, Arduino unit tests with PlatformIO framework.

## How to run locally

- Arduino: `pio run`, `pio upload`, `pio device monitor`
- Python: `uv sync`, `pytest -q`
- Integration: run server mockup, connect Arduino via USB, verify LCD output
- Docker compose: `make up`, `make down`

## Commands

- `make fmt`, `make lint`, `make type`, `make test`, `make e2e`
- `make ci` runs all of the above
- Arduino tasks scripted in Makefile for reproducibility
- Systemd helpers: `sudo make service-system-install …` for initial deployment and `sudo make service-system-update …` for redeployments (invokes pip via `sudo -H -u <service user>` to keep caches writable).

## Git and PR policy

- Branch naming: feature/, fix/, chore/
- Commit style: feat|fix|docs|chore: short summary
- Every PR must update tests and docs if behavior changes
- Target: main is protected. Use PRs, no direct pushes

## CI

- GitHub Actions. Run on push and PR. Cache deps. Collect coverage. Block merge on red.
- Required checks: lint, type, test, build, Arduino compile

## Security and secrets

- Never commit secrets. Use .env.local and CI secrets. Rotate if leaked.
- Dependencies must pass vulnerability scan. For Python use pip-audit. For Arduino, pin library versions in PlatformIO.
- Network access rules for Codex cloud. Off by default, enable only for package registries and pinned hosts.

## Architecture guidance

- Monorepo layout:
  - `/arduino` for Element 1 code
  - `/server` for Element 2 Python daemon
  - `/infra`, `/docs`, `/scripts`
- Service boundaries documented in `/docs/adr`. Every new service needs an ADR and Makefile targets.
- Logging, metrics, tracing for server required. Prefer OpenTelemetry.

## What Codex should do

- Plan, then implement tasks from PROJECT\_PLAN.md.
- Create or update Makefile targets, CI, tests, and docs.
- Generate skeletons for config files and sample data flows.
- Open focused PRs with a clear checklist, rationale, and risk notes.
- Run linters, type checkers, and tests locally or in cloud before opening PRs.

## What Codex must not do

- No pushes to main.
- No dependency upgrades without a PR that explains risk and changelog.
- No schema changes without migration plan and rollback steps.
- No external calls except those allowed under “Security and secrets”.

## Repository context files

- PROJECT\_PLAN.md defines phases and milestones.
- TASKS.md holds the current checklist, Codex must keep it updated.
- /docs/adr contains architecture decisions.
- /docs/wiring.md documents Nano-to-LCD and encoder wiring.
- /Makefile defines reproducible targets for Arduino and Python.
- /arduino/src/main.cpp entrypoint sketch with Hello World.
- /server/src/main.py entrypoint Python daemon.
- /server/config.example.yaml sample configuration file.
- /infra/systemd/lcdmonitor.system.service system-mode unit (shell exec driver default, `NoNewPrivileges=no` to allow scoped sudo commands).

## Review checklist for PRs

- Tests added or updated, coverage not down.
- CI green. Lint and type checks clean.
- Backward compatibility considered, migrations included.
- Logs and errors helpful and actionable.
- Docs updated where needed.
- Arduino code builds and runs on target hardware.

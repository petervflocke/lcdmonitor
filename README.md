# LCD Monitor

Two-part monitoring stack that shows Linux host metrics on an Arduino Nano with a 20x4 HD44780 LCD. The Arduino handles the UI (telemetry vs. command list) while a Python daemon polls system sensors, formats the frames, and exchanges commands over serial.

## Repository layout
- `arduino/` – PlatformIO project for the Nano sketch (LCD driver, rotary encoder, serial protocol).
- `server/` – Python 3.12 daemon with config loader, sensor collectors, command protocol, and systemd entrypoint.
- `infra/systemd/` – Example unit files for user and system service deployments.
- `docs/` – Wiring, ADRs, and deployment guides (`docs/deployment-systemd.md` expands on the service setup).
- `Makefile` – Shared tasks for formatting, linting, testing, builds, and service helper commands.

## Prerequisites
- Python 3.12 with [uv](https://github.com/astral-sh/uv) (or `python3 -m venv` fallback).
- PlatformIO CLI (`pio`) and serial access to the Arduino Nano.
- Optional GPU telemetry: `pynvml` or `nvidia-smi` available on the host.

## Setup
```bash
make setup        # sync Python deps with uv
make fmt          # format Python code (ruff + black)
make lint         # ruff linting
make type         # mypy (strict)
make test         # pytest
make arduino-build
```
Hardware upload/monitor commands are available via `make arduino-upload` and `make arduino-monitor`.

## Running the Python daemon
- Dry run on a development box (no Arduino required):
  ```bash
  make server-dry-run
  ```
  This prints the assembled LCD lines to stdout once and exits.
- Full daemon using the sample config and the local serial device:
  ```bash
  make server-run
  ```
  The config is driven by `server/config.example.yaml`; copy and edit it for your host. Enable command execution explicitly with `--allow-exec` and pick an execution driver (`systemd-user`, `systemd-system`, or `shell`).

## Systemd deployment
Systemd units live in `infra/systemd/` and are documented in detail in `docs/deployment-systemd.md`. In short:

- **User service (development)** – `infra/systemd/lcdmonitor.service` runs under the logged-in user. Copy it to `~/.config/systemd/user/`, adjust `WorkingDirectory`/`ExecStart`, then run:
  ```bash
  systemctl --user daemon-reload
  systemctl --user enable --now lcdmonitor
  journalctl --user -u lcdmonitor -f
  ```
  For headless operation, enable lingering via `loginctl enable-linger $USER`.

- **System Service (production)** – `infra/systemd/lcdmonitor.system.service` assumes a dedicated service account (e.g. `lcdmon`) that belongs to the serial group (`dialout`, `uucp`, etc.). Provision `/opt/lcdmonitor` (code + virtualenv) and `/etc/lcdmonitor/config.yaml`, then create `/etc/default/lcdmonitor` with:
  ```ini
  LCDMONITOR_VENV=/opt/lcdmonitor/.venv
  LCDMONITOR_CONFIG=/etc/lcdmonitor/config.yaml
  ```
  Install and start the unit:
  ```bash
  sudo cp infra/systemd/lcdmonitor.system.service /etc/systemd/system/lcdmonitor.service
  sudo systemctl daemon-reload
  sudo systemctl enable --now lcdmonitor
  sudo journalctl -u lcdmonitor -f
  ```
  Command execution stays disabled by default; add `--allow-exec --exec-driver systemd-system` to `ExecStart` once you have a whitelist in the config.

Make helpers print the same instructions for quick reference:
```bash
make service-user-install
make service-system-install
```

## Testing and CI
- `make ci` runs formatting checks, lint, mypy, pytest, and an Arduino build.
- `make e2e PORT=/dev/ttyACM0` builds the sketch and runs the mock sender against connected hardware.
- `uvx pip-audit` (via `make audit`) surfaces Python dependency issues.

## Additional docs
- Wiring diagram and bill of materials: `docs/wiring.md`.
- Systemd deployment deep dive: `docs/deployment-systemd.md`.
- Architecture and decisions: `docs/adr/`.

Keep docs, tests, and configs in sync with behavior changes before opening a PR.

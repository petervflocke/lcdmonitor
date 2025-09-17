# Deployment: systemd (user and system modes)

This guide explains how to run the Python server as a supervised systemd service.

## Overview

- Development machines: prefer a user service (no sudo, logs in user journal).
- Headless/production: prefer a system service (root-managed, runs at boot).
- Execution of configured commands remains disabled by default. Enable explicitly with `--allow-exec` and choose an exec driver.

## Prerequisites

- A working checkout under a stable path (e.g., `/opt/lcdmonitor` for production or your home directory for dev).
- Python 3.12 + virtual environment with project dependencies installed (see `make setup`).
- Serial permissions: the service user must be in the serial group (often `dialout` on Debian/Ubuntu, `uucp` on Arch, etc.).

## User Service (development)

- Install unit:
  - Copy `infra/systemd/lcdmonitor.service` to `~/.config/systemd/user/lcdmonitor.service`.
  - Adjust `WorkingDirectory` and `ExecStart` to your environment (paths to repo and venv, config path).
- Enable/start:
  - `systemctl --user daemon-reload`
  - `systemctl --user enable --now lcdmonitor`
- Headless user sessions:
  - `loginctl enable-linger $USER` to allow user services to run without an active login.
- Logs:
  - `journalctl --user -u lcdmonitor -f`

## System Service (production, headless)

- Create a dedicated service user and directories:
  - `sudo useradd -r -s /usr/sbin/nologin -G dialout lcdmon` (adjust shell/group as needed)
  - `sudo mkdir -p /opt/lcdmonitor /etc/lcdmonitor`
  - `sudo chown -R lcdmon:dialout /opt/lcdmonitor /etc/lcdmonitor`
- Install app + venv under `/opt/lcdmonitor` (or your deployment tooling). Example:
  - Create venv, install deps, and copy the repo to `/opt/lcdmonitor`; the bundled unit assumes `/opt/lcdmonitor/server` contains the Python package (`src`).
- Configuration:
  - Create `/etc/lcdmonitor/config.yaml` (based on `server/config.example.yaml`).
  - Create `/etc/default/lcdmonitor` with:
    - `LCDMONITOR_VENV=/opt/lcdmonitor/.venv`
    - `LCDMONITOR_CONFIG=/etc/lcdmonitor/config.yaml`
- Install system unit (two options):
  - Manual: `sudo cp infra/systemd/lcdmonitor.system.service /etc/systemd/system/lcdmonitor.service`, edit `User=`, `Group=`, `EnvironmentFile=`, then `sudo systemctl daemon-reload && sudo systemctl enable --now lcdmonitor`.
  - Automated: `sudo make service-system-install SERVICE_USER=lcdmon SERVICE_GROUP=dialout INSTALL_ROOT=/opt/lcdmonitor CONFIG_PATH=/etc/lcdmonitor/config.yaml ENV_FILE=/etc/default/lcdmonitor` rsyncs the current checkout into `${INSTALL_ROOT}` (set `COPY_REPO=0` to skip; falls back to `tar` without deletion if `rsync` is missing), seeds `/etc/default/lcdmonitor`, copies the adjusted unit into place, reloads systemd, and enables the service (unless `ENABLE_SERVICE=0`). The helper expects the service user/group to exist and the virtualenv at `${INSTALL_ROOT}/.venv` to contain the dependencies.
- Logs:
  - `sudo journalctl -u lcdmonitor -f`

## Execution options

- By default the daemon does not execute commands; it only logs selections.
- Enable with `--allow-exec`. Choose backend with `--exec-driver`:
  - `systemd-user`: transient units via `systemd-run --user` (default).
  - `systemd-system`: transient units via `systemd-run` (system manager).
  - `shell`: `/bin/sh -lc "<exec>"` (for development only; less safe).
- Example ExecStart for system service (via env file):
  - `ExecStart=${LCDMONITOR_VENV}/bin/python -m src.main --config ${LCDMONITOR_CONFIG} --exec-driver systemd-system`

## Serial permissions

- Put the service user into the appropriate serial group (e.g., `dialout`).
- Check the device path (e.g., `/dev/ttyUSB0` or `/dev/ttyACM0`) and match `serial.port` in config.
- If needed, add udev rules for stable device naming.

## Troubleshooting

- Service won’t start:
  - `systemctl status lcdmonitor` and `journalctl -u lcdmonitor -e` for errors.
  - Verify Python venv path and config path in the unit.
- No serial access:
  - Ensure group membership and re-login; confirm device path.
- Commands not executing:
  - Confirm `--allow-exec` and the `--exec-driver` you expect.
  - For systemd drivers, check transient unit logs: `journalctl --user -u lcdcmd-<id>-<ts>` or system scope accordingly.

## Makefile helpers

- `make service-user-install`: prints the steps to install a user-mode service.
- `make service-system-notes`: prints the manual steps for system mode.
- `sudo make service-system-install`: installs/updates the systemd unit and helper files (override variables to match your deployment paths).

# Deployment: systemd (user and system modes)

This guide explains how to run the Python server as a supervised systemd service.

## Overview

- Development machines: prefer a user service (no sudo, logs in user journal).
- Headless/production: prefer a system service (root-managed, runs at boot).
- Execution of configured commands remains disabled by default. Enable explicitly with `--allow-exec` and choose an exec driver (the `shell` driver is the default for compatibility; see notes below).

## Prerequisites

- A working checkout under a stable path (e.g., `/opt/lcdmonitor` for production or your home directory for dev).
- Python 3.12 + virtual environment with project dependencies installed (e.g., `cd server && uv sync` during development or `pip install /opt/lcdmonitor/server` inside the production venv).
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
  - Create venv, install deps (`pip install /opt/lcdmonitor/server`), and copy the repo to `/opt/lcdmonitor`; the bundled unit assumes `/opt/lcdmonitor/server` contains the Python package (`src`).
- Configuration:
  - Create `/etc/lcdmonitor/config.yaml` (based on `server/config.example.yaml`).
  - Create `/etc/default/lcdmonitor` with:
    - `LCDMONITOR_VENV=/opt/lcdmonitor/.venv`
    - `LCDMONITOR_CONFIG=/etc/lcdmonitor/config.yaml`
- Install system unit (two options):
  - Manual: `sudo cp infra/systemd/lcdmonitor.system.service /etc/systemd/system/lcdmonitor.service`, edit `User=`, `Group=`, `EnvironmentFile=`, then `sudo systemctl daemon-reload && sudo systemctl enable --now lcdmonitor`.
  - Automated: `sudo make service-system-install SERVICE_USER=lcdmon SERVICE_GROUP=dialout INSTALL_ROOT=/opt/lcdmonitor CONFIG_PATH=/etc/lcdmonitor/config.yaml ENV_FILE=/etc/default/lcdmonitor` rsyncs the current checkout into `${INSTALL_ROOT}` (set `COPY_REPO=0` to skip; falls back to `tar` without deletion if `rsync` is missing), seeds `/etc/default/lcdmonitor`, copies the adjusted unit into place, reloads systemd, ensures the config file is owned by the service account, and enables the service (unless `ENABLE_SERVICE=0`). The helper expects the service user/group to exist and the virtualenv at `${INSTALL_ROOT}/.venv` to contain the dependencies.
  - Updating in place: after pulling new code, run `sudo make service-system-update SERVICE_USER=lcdmon SERVICE_GROUP=dialout INSTALL_ROOT=/opt/lcdmonitor CONFIG_PATH=/etc/lcdmonitor/config.yaml ENV_FILE=/etc/default/lcdmonitor`. This reruns the installer with `ENABLE_SERVICE=0`, reinstalls the Python package into the virtualenv, reloads systemd, restarts the service, and surfaces its status output.
  - The installer will pre-create `${SERVICE_USER}`'s `~/.cache/pip` (if the home directory exists) so pip upgrades can cache wheels without warnings; the update target runs pip as `sudo -H -u ${SERVICE_USER}` for the same reason.
  - The unit leaves `NoNewPrivileges=no` so shell-driven sudo commands can escalate; review the other hardening directives and adjust if your security posture requires tighter lockdowns.
- Logs:
  - `sudo journalctl -u lcdmonitor -f`

## Execution options

- By default the daemon does not execute commands; it only logs selections.
- Enable with `--allow-exec`. Choose backend with `--exec-driver`:
  - `shell` (default): runs `/bin/sh -lc "<exec>"` as the service user. Combine with restrictive sudo rules (`sudo -n …`) if root access is required.
  - `systemd-user`: creates transient units via `systemd-run --user`; requires a running user manager (e.g., `loginctl enable-linger lcdmon`).
  - `systemd-system`: creates transient units via the system manager; only works if the service user has polkit/sudo permission to call `systemd-run` at the system scope.
- Example ExecStart for system service (via env file):
  - `ExecStart=/bin/sh -c 'exec "$${LCDMONITOR_VENV}/bin/python" -m src.main --config "$${LCDMONITOR_CONFIG}" --exec-driver shell'`
  - Pair root-requiring commands with restrictive sudoers rules (e.g., `lcdmon ALL=(root) NOPASSWD:/sbin/shutdown,/sbin/reboot`) so `sudo -n` succeeds without prompting.
- Serial port handling: the daemon retries opening the port with exponential backoff (5s → 30s, capped) so you can start it before the Arduino is attached without systemd churn.

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
- `sudo make service-system-update`: refreshes the deployed code, reinstalls the Python package in the venv, and restarts the unit.

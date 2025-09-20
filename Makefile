# Minimal unified Makefile for the scaffold
PY=uv run
PIO=pio

.PHONY: setup setup-pip fmt fmt-check lint type pytest test ci e2e up down audit \
        arduino-build arduino-upload arduino-monitor arduino-clean arduino-test \
        server-run server-dry-run server-run-pip server-dry-run-pip \
        service-user-install service-system-install service-system-notes \
        service-system-update

# Install Python deps (runtime + dev) with uv
setup:
	cd server && uv sync --extra dev

# Fallback: setup using pip venv without uv
setup-pip:
	python3 -m venv server/.venv
	server/.venv/bin/pip install --upgrade pip
	server/.venv/bin/pip install pyserial psutil PyYAML
	@echo "Optional GPU support: server/.venv/bin/pip install pynvml (if available)"

# Format code (writes changes)
fmt:
	cd server && $(PY) ruff format .
	cd server && $(PY) black .

# Check formatting (no changes)
fmt-check:
	cd server && $(PY) ruff format --check .
	cd server && $(PY) black --check .

lint:
	cd server && $(PY) ruff check .

type:
	cd server && $(PY) mypy .

pytest:
	cd server && $(PY) pytest -q

test: pytest

# CI aggregate: check-only steps + Arduino build
ci: fmt-check lint type test arduino-build

# End-to-end smoke: build Arduino and run mock sender locally
# E2E: build Arduino and run mock sender. Override with: make e2e PORT=/dev/ttyACM0
PORT ?= /dev/ttyUSB0
e2e: arduino-build
	cd server && $(PY) python -m src.mock_sender --port=$(PORT)

# Docker compose placeholders (infra not yet present)
up:
	@echo "No infra/docker-compose.yml found; add in Phase 3+"

down:
	@echo "No infra/docker-compose.yml found; add in Phase 3+"

audit:
	cd server && uvx pip-audit || true

arduino-build:
	cd arduino && $(PIO) run

arduino-upload:
	cd arduino && $(PIO) run -t upload

arduino-monitor:
	cd arduino && $(PIO) device monitor -b 115200

arduino-clean:
	cd arduino && $(PIO) run -t clean

arduino-test:
	cd arduino && $(PIO) test

# Convenience targets for server daemon (Phase 5)
.PHONY: server-run server-dry-run
server-run:
	cd server && $(PY) python -m src.main --config config.example.yaml

server-dry-run:
	cd server && $(PY) python -m src.main --dry-run --once --config config.example.yaml

# Run via pip venv created by setup-pip
server-run-pip:
	cd server && .venv/bin/python -m src.main --config config.example.yaml

server-dry-run-pip:
	cd server && .venv/bin/python -m src.main --dry-run --once --config config.example.yaml

# Systemd service install helpers (print instructions; actual install requires root)
service-user-install:
	@echo "User service install steps:"
	@echo "1) mkdir -p $$HOME/.config/systemd/user"
	@echo "2) cp infra/systemd/lcdmonitor.service $$HOME/.config/systemd/user/lcdmonitor.service"
	@echo "3) systemctl --user daemon-reload && systemctl --user enable --now lcdmonitor"
	@echo "4) Optional lingering for headless: loginctl enable-linger $$USER"

service-system-notes:
	@echo "System service install steps (requires sudo):"
	@echo "1) Create service user/group (if missing): sudo useradd -r -s /usr/sbin/nologin -G dialout lcdmon"
	@echo "2) Create runtime dirs: sudo mkdir -p /opt/lcdmonitor /etc/lcdmonitor"
	@echo "   and chown them to the service account: sudo chown -R lcdmon:dialout /opt/lcdmonitor /etc/lcdmonitor"
	@echo "3) Deploy repo under /opt/lcdmonitor and create virtualenv /opt/lcdmonitor/.venv (run make setup)."
	@echo "4) Create /etc/default/lcdmonitor with:"
	@echo "   LCDMONITOR_VENV=/opt/lcdmonitor/.venv"
	@echo "   LCDMONITOR_CONFIG=/etc/lcdmonitor/config.yaml"
	@echo "5) sudo cp infra/systemd/lcdmonitor.system.service /etc/systemd/system/lcdmonitor.service"
	@echo "6) sudo systemctl daemon-reload && sudo systemctl enable --now lcdmonitor"
	@echo "7) Check logs: sudo journalctl -u lcdmonitor -f"

SERVICE_USER ?= lcdmon
SERVICE_GROUP ?= dialout
INSTALL_ROOT ?= /opt/lcdmonitor
CONFIG_PATH ?= /etc/lcdmonitor/config.yaml
ENV_FILE ?= /etc/default/lcdmonitor
SYSTEMD_UNIT ?= lcdmonitor.service
SYSTEMD_DIR ?= /etc/systemd/system
ENABLE_SERVICE ?= 1
SUDO ?= sudo

service-system-install:
	@echo "Installing system service as $(SERVICE_USER):$(SERVICE_GROUP) (user/group must exist). Override via SERVICE_USER=..." \
		" SERVICE_GROUP=..."
	@echo "Target install root: $(INSTALL_ROOT) (server code expected under $(INSTALL_ROOT)/server)" \
		" and virtualenv path: $(INSTALL_ROOT)/.venv (override via ENV_FILE)."
	@echo "Installer rsyncs the current checkout unless COPY_REPO=0. Use ENABLE_SERVICE=0 to skip enabling until ready."
	@echo "Config will be written to $(CONFIG_PATH) if missing; env vars file: $(ENV_FILE)."
	$(SUDO) SERVICE_USER=$(SERVICE_USER) SERVICE_GROUP=$(SERVICE_GROUP) \
		INSTALL_ROOT=$(INSTALL_ROOT) CONFIG_PATH=$(CONFIG_PATH) \
		ENV_FILE=$(ENV_FILE) UNIT_NAME=$(SYSTEMD_UNIT) \
		SYSTEMD_DIR=$(SYSTEMD_DIR) ENABLE_SERVICE=$(ENABLE_SERVICE) \
		scripts/install_system_service.sh

# Update an existing system deployment after pulling new code.
service-system-update:
	@echo "Updating system deployment for $(SYSTEMD_UNIT) at $(INSTALL_ROOT)"
	$(SUDO) $(MAKE) service-system-install \
		SERVICE_USER=$(SERVICE_USER) SERVICE_GROUP=$(SERVICE_GROUP) \
		INSTALL_ROOT=$(INSTALL_ROOT) CONFIG_PATH=$(CONFIG_PATH) \
		ENV_FILE=$(ENV_FILE) SYSTEMD_UNIT=$(SYSTEMD_UNIT) \
		SYSTEMD_DIR=$(SYSTEMD_DIR) ENABLE_SERVICE=0
	$(SUDO) -u $(SERVICE_USER) $(INSTALL_ROOT)/.venv/bin/pip install --upgrade $(INSTALL_ROOT)/server
	$(SUDO) systemctl daemon-reload
	$(SUDO) systemctl restart $(SYSTEMD_UNIT)
	$(SUDO) systemctl status $(SYSTEMD_UNIT) --no-pager

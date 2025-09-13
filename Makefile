# Minimal unified Makefile for the scaffold
PY=uv run
PIO=pio

.PHONY: setup setup-pip fmt fmt-check lint type pytest test ci e2e up down audit \
        arduino-build arduino-upload arduino-monitor arduino-clean arduino-test \
        server-run server-dry-run server-run-pip server-dry-run-pip

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

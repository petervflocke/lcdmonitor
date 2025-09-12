# Minimal unified Makefile for the scaffold
PY=uv run
PIO=pio

.PHONY: setup fmt fmt-check lint type pytest test ci e2e up down audit \
        arduino-build arduino-upload arduino-monitor arduino-clean

# Install Python deps (runtime + dev) with uv
setup:
	cd server && uv sync --extra dev

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
e2e: arduino-build
	cd server && $(PY) python src/mock_sender.py

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

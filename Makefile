# Minimal unified Makefile for the scaffold
PY=uv run
PIO=pio

.PHONY: fmt lint type pytest test ci arduino-build arduino-upload arduino-monitor

fmt:
	$(PY) ruff format server || true
	-$(PY) black server || true

lint:
	$(PY) ruff check server

type:
	$(PY) mypy server

pytest:
	$(PY) pytest -q

test: pytest

ci: fmt lint type test arduino-build

arduino-build:
	cd arduino && $(PIO) run

arduino-upload:
	cd arduino && $(PIO) run -t upload

arduino-monitor:
	cd arduino && $(PIO) device monitor -b 115200
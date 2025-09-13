from __future__ import annotations

import io
import sys
from pathlib import Path

from src.main import main as daemon_main


def test_dry_run_once_outputs_lines(tmp_path: Path, monkeypatch) -> None:
    # Minimal config with 2 sensors
    cfg_path = tmp_path / "cfg.yaml"
    cfg_path.write_text(
        """
interval: 0.1
serial:
  port: /dev/null
  baud: 115200
max_lines: 5
sensors:
  - name: CPU
    provider: cpu
  - name: GPU
    provider: gpu
"""
    )

    # Capture stdout
    buf = io.StringIO()
    monkeypatch.setattr(sys, "stdout", buf)

    # Run one iteration in dry-run
    rc = daemon_main(["--config", str(cfg_path), "--dry-run", "--once"])  # type: ignore[arg-type]
    assert rc == 0
    out = buf.getvalue().strip().splitlines()
    assert 1 <= len(out) <= 2  # GPU line may be absent on machines without NVML
    # Ensure each line is at most 20 chars
    assert all(len(line) <= 20 for line in out)

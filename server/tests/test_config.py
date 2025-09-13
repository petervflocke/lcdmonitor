from __future__ import annotations

from pathlib import Path

from src.config import AppConfig, load_config


def test_load_minimal_tmpfile(tmp_path: Path) -> None:
    cfg_path = tmp_path / "cfg.yaml"
    cfg_path.write_text(
        """
interval: 2.5
serial:
  port: /dev/ttyACM0
  baud: 57600
max_lines: 8
sensors:
  - name: CPU
    provider: cpu
  - name: GPU
    provider: gpu
"""
    )
    cfg = load_config(cfg_path)
    assert isinstance(cfg, AppConfig)
    assert cfg.interval == 2.5
    assert cfg.serial.port == "/dev/ttyACM0"
    assert cfg.serial.baud == 57600
    assert cfg.max_lines == 8
    assert [s.name for s in cfg.sensors] == ["CPU", "GPU"]

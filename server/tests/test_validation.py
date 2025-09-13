from __future__ import annotations

import pytest

from src.config import AppConfig, SerialConfig, SensorConfig, validate_config


def test_validate_ok_join() -> None:
    cfg = AppConfig(
        interval=1.0,
        serial=SerialConfig(port="/dev/null", baud=115200),
        max_lines=12,
        sensors=[
            SensorConfig(name="CPU", provider="cpu"),
            SensorConfig(
                name="Temps",
                provider="join",
                join=[
                    SensorConfig(name="Pkg", provider="temp", params={"chip": "coretemp"}),
                    SensorConfig(name="NVMe", provider="temp", params={"chip": "nvme"}),
                ],
            ),
        ],
    )
    validate_config(cfg)


def test_validate_rejects_invalid_provider() -> None:
    cfg = AppConfig(
        interval=1.0,
        serial=SerialConfig(port="/dev/null", baud=115200),
        max_lines=12,
        sensors=[SensorConfig(name="X", provider="bogus")],
    )
    with pytest.raises(ValueError):
        validate_config(cfg)


def test_validate_rejects_empty_join() -> None:
    cfg = AppConfig(
        interval=1.0,
        serial=SerialConfig(port="/dev/null", baud=115200),
        max_lines=12,
        sensors=[SensorConfig(name="J", provider="join", join=[])],
    )
    with pytest.raises(ValueError):
        validate_config(cfg)


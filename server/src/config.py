from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, List

import yaml


@dataclass
class SerialConfig:
    port: str = "/dev/ttyUSB0"
    baud: int = 115200


@dataclass
class SensorConfig:
    name: str
    provider: str
    enabled: bool = True
    format: str | None = None  # reserved for future use
    params: dict[str, Any] = field(default_factory=dict)


@dataclass
class AppConfig:
    interval: float = 5.0
    serial: SerialConfig = field(default_factory=SerialConfig)
    max_lines: int = 12
    sensors: List[SensorConfig] = field(default_factory=list)


def _as_int(val: Any, default: int) -> int:
    try:
        return int(val)
    except Exception:
        return default


def _as_float(val: Any, default: float) -> float:
    try:
        return float(val)
    except Exception:
        return default


def _load_yaml(path: Path) -> dict[str, Any]:
    data = yaml.safe_load(path.read_text())
    return data or {}


def load_config(path: str | Path) -> AppConfig:
    p = Path(path)
    data = _load_yaml(p)

    # serial
    serial_raw = data.get("serial", {})
    serial = SerialConfig(
        port=str(serial_raw.get("port", SerialConfig.port)),
        baud=_as_int(serial_raw.get("baud", SerialConfig.baud), SerialConfig.baud),
    )

    # basics
    interval = _as_float(data.get("interval", 5.0), 5.0)
    max_lines = _as_int(data.get("max_lines", 12), 12)

    # sensors list
    sensors: list[SensorConfig] = []
    for item in data.get("sensors", []) or []:
        if not isinstance(item, dict):
            continue
        name = str(item.get("name", ""))
        provider = str(item.get("provider", ""))
        if not name or not provider:
            continue
        enabled = bool(item.get("enabled", True))
        fmt = item.get("format")
        if fmt is not None:
            fmt = str(fmt)
        params = item.get("params") or {}
        if not isinstance(params, dict):
            params = {}
        sensors.append(SensorConfig(name=name, provider=provider, enabled=enabled, format=fmt, params=params))

    return AppConfig(interval=interval, serial=serial, max_lines=max_lines, sensors=sensors)


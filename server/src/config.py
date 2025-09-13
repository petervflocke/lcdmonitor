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
    join: list["SensorConfig"] = field(default_factory=list)


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
        # If this is a join block, provider can be omitted; set to 'join'
        provider_raw = item.get("provider")
        join_raw = item.get("join")
        if join_raw is not None and provider_raw is None:
            provider = "join"
        else:
            provider = str(provider_raw or "")
        if not name or not provider:
            continue
        enabled = bool(item.get("enabled", True))
        fmt = item.get("format")
        if fmt is not None:
            fmt = str(fmt)
        params = item.get("params") or {}
        if not isinstance(params, dict):
            params = {}
        join_list: list[SensorConfig] = []
        if isinstance(join_raw, list):
            for sub in join_raw:
                if not isinstance(sub, dict):
                    continue
                sub_name = str(sub.get("name", ""))
                sub_provider = str(sub.get("provider", ""))
                if not sub_provider:
                    continue
                sub_enabled = bool(sub.get("enabled", True))
                sub_fmt = sub.get("format")
                if sub_fmt is not None:
                    sub_fmt = str(sub_fmt)
                sub_params = sub.get("params") or {}
                if not isinstance(sub_params, dict):
                    sub_params = {}
                join_list.append(
                    SensorConfig(
                        name=sub_name,
                        provider=sub_provider,
                        enabled=sub_enabled,
                        format=sub_fmt,  # type: ignore[arg-type]
                        params=sub_params,
                    )
                )

        sensors.append(
            SensorConfig(
                name=name,
                provider=provider,
                enabled=enabled,
                format=fmt,
                params=params,
                join=join_list,
            )
        )

    return AppConfig(interval=interval, serial=serial, max_lines=max_lines, sensors=sensors)

from __future__ import annotations

import argparse
import logging
import sys
import threading
import time
from typing import List, Optional

import serial

from .config import AppConfig, SensorConfig, load_and_validate_config
from .metrics import cpu_summary, gpu_summary, temp_summary
from .protocol import Outbound


def parse_args(argv: list[str]) -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Server daemon to send metrics to Arduino LCD")
    p.add_argument("--config", default="server/config.example.yaml", help="Path to YAML config")
    p.add_argument("--dry-run", action="store_true", help="Print lines to stdout instead of serial")
    p.add_argument("--once", action="store_true", help="Run one iteration and exit (for tests)")
    p.add_argument(
        "--no-echo",
        action="store_true",
        help="Disable printing lines read from Arduino",
    )
    p.add_argument(
        "--verbose",
        action="store_true",
        help="Enable INFO-level logging (default is ERROR)",
    )
    return p.parse_args(argv)


def _reader(ser: serial.Serial, stop: threading.Event) -> None:  # pragma: no cover
    while not stop.is_set():
        try:
            line = ser.readline()
            if line:
                try:
                    print(f"[arduino] {line.decode(errors='replace').rstrip()}")
                except Exception:
                    print(f"[arduino bytes] {line!r}")
        except Exception as e:
            logging.getLogger(__name__).error("reader error: %s", e)
            break


def _sensor_text(s: SensorConfig) -> Optional[str]:
    if not s.enabled:
        return None
    if s.provider == "cpu":
        return cpu_summary()
    if s.provider == "gpu":
        return gpu_summary()
    if s.provider == "temp":
        chip = str(s.params.get("chip")) if s.params.get("chip") is not None else None
        label = str(s.params.get("label")) if s.params.get("label") is not None else None
        return temp_summary(chip=chip, label=label)
    return None


def _collect_lines(cfg: AppConfig) -> List[str]:
    lines: List[str] = []
    for s in cfg.sensors:
        if not s.enabled:
            continue
        # Join mode: combine child sensors into one line
        if s.provider == "join" or s.join:
            parts: list[str] = []
            for child in s.join:
                t = _sensor_text(child)
                if t is None:
                    continue
                part = t
                if child.name:
                    part = f"{child.name} {part}"
                parts.append(part)
            if not parts:
                continue
            text = " ".join(parts)
            if s.name:
                text = f"{s.name} {text}"
        else:
            text = _sensor_text(s)
            if text is None:
                continue
            text = f"{s.name} {text}"
        # Truncate to LCD width here already
        lines.append(text[:20])
        if len(lines) >= cfg.max_lines:
            break
    return lines


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    # Logging: minimal by default (ERROR). --verbose switches to INFO.
    lvl = logging.INFO if args.verbose else logging.ERROR
    logging.basicConfig(level=lvl, format="%(levelname)s:%(name)s:%(message)s", stream=sys.stderr)
    log = logging.getLogger(__name__)
    try:
        cfg = load_and_validate_config(args.config)
    except Exception as e:
        log.error("Failed to load config: %s", e)
        return 2

    if args.dry_run:
        lines = _collect_lines(cfg)
        for ln in lines:
            print(ln)
        return 0

    try:
        ser = serial.Serial(cfg.serial.port, cfg.serial.baud, timeout=0.2)
    except Exception as e:
        log.error("Failed to open serial port %s: %s", cfg.serial.port, e)
        return 3

    try:
        reader_stop = threading.Event()
        reader_thread: threading.Thread | None = None
        if not args.no_echo:
            reader_thread = threading.Thread(target=_reader, args=(ser, reader_stop), daemon=True)
            reader_thread.start()
        while True:
            lines = _collect_lines(cfg)
            payload = Outbound(lines=lines).encode()
            ser.write(payload)
            ser.flush()
            if args.verbose:
                log.info("sent %d line(s)", len(lines))
            if args.once:
                return 0
            time.sleep(cfg.interval)
    except KeyboardInterrupt:
        return 0
    finally:
        try:
            try:
                reader_stop.set()
                if reader_thread is not None:
                    reader_thread.join(timeout=1.0)
            except Exception:
                pass
            ser.close()
        except Exception:
            pass


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))

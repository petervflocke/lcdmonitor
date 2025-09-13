from __future__ import annotations

import argparse
import sys
import threading
import time
from typing import List, Optional

import serial

from .config import AppConfig, load_config
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
            print(f"[reader error] {e}", file=sys.stderr)
            break


def _collect_lines(cfg: AppConfig) -> List[str]:
    lines: List[str] = []
    for s in cfg.sensors:
        if not s.enabled:
            continue
        text: Optional[str] = None
        if s.provider == "cpu":
            text = cpu_summary()
        elif s.provider == "gpu":
            text = gpu_summary()
        elif s.provider == "temp":
            chip = str(s.params.get("chip")) if s.params.get("chip") is not None else None
            label = str(s.params.get("label")) if s.params.get("label") is not None else None
            text = temp_summary(chip=chip, label=label)
        else:
            # Unknown provider -> skip
            continue
        if text is None:
            continue
        # Prefix each metric with sensor name (metrics text excludes label)
        text = f"{s.name} {text}"
        # Truncate to LCD width here already
        lines.append(text[:20])
        if len(lines) >= cfg.max_lines:
            break
    return lines


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    try:
        cfg = load_config(args.config)
    except Exception as e:
        print(f"Failed to load config: {e}", file=sys.stderr)
        return 2

    if args.dry_run:
        lines = _collect_lines(cfg)
        for ln in lines:
            print(ln)
        return 0

    try:
        ser = serial.Serial(cfg.serial.port, cfg.serial.baud, timeout=0.2)
    except Exception as e:
        print(f"Failed to open serial port {cfg.serial.port}: {e}", file=sys.stderr)
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

from __future__ import annotations

import argparse
import sys
import time

import serial

from .protocol import Outbound


def parse_args(argv: list[str]) -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Mock serial sender to Arduino LCD")
    p.add_argument("--port", default="/dev/ttyUSB0", help="Serial port (default: /dev/ttyUSB0)")
    p.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    p.add_argument(
        "--interval",
        type=float,
        default=5.0,
        help="Seconds between updates (default: 5)",
    )
    p.add_argument("--lines", type=int, default=10, help="Lines per update (default: 10)")
    return p.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    try:
        ser = serial.Serial(args.port, args.baud, timeout=1)
    except Exception as e:  # pragma: no cover - hardware dependent
        print(f"Failed to open serial port {args.port}: {e}", file=sys.stderr)
        return 2

    counter = 0
    try:
        while True:
            now = time.strftime("%H:%M:%S")
            # First 4 lines as before, then add extras up to args.lines
            lines: list[str] = [
                f"Status {counter:06d}",
                f"Time {now}",
                "CPU 25% 42C",
                "GPU 12% 512MB 45C",
            ]
            # Add additional test lines (truncated on Arduino to 20 chars)
            for i in range(4, args.lines):
                lines.append(f"Item {i:02d} #{counter:06d}")

            payload = Outbound(lines=lines).encode()
            ser.write(payload)
            ser.flush()
            counter += 1
            time.sleep(args.interval)
    except KeyboardInterrupt:  # pragma: no cover - manual stop
        return 0
    finally:
        try:
            ser.close()
        except Exception:
            pass


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))

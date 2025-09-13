from __future__ import annotations

import argparse
import sys
import threading
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


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.2)
    except Exception as e:  # pragma: no cover - hardware dependent
        print(f"Failed to open serial port {args.port}: {e}", file=sys.stderr)
        return 2

    reader_stop = threading.Event()
    reader_thread: threading.Thread | None = None
    if not args.no_echo:
        reader_thread = threading.Thread(target=_reader, args=(ser, reader_stop), daemon=True)
        reader_thread.start()

    counter = 0
    try:
        while True:
            now = time.strftime("%H:%M:%S")
            lines: list[str] = [
                f"Status {counter:06d}",
                f"Time {now}",
                "CPU 25% 42C",
                "GPU 12% 512MB 45C",
            ]
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
            reader_stop.set()
            if reader_thread is not None:
                reader_thread.join(timeout=1.0)
        finally:
            try:
                ser.close()
            except Exception:
                pass


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))

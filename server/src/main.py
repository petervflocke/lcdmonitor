from __future__ import annotations

import argparse
import logging
import sys
import threading
import time
from typing import List, Optional

import serial
import subprocess

from .config import AppConfig, CommandConfig, SensorConfig, load_and_validate_config
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
    p.add_argument(
        "--allow-exec",
        action="store_true",
        help="Allow executing configured commands (DANGEROUS). Off by default.",
    )
    p.add_argument(
        "--exec-driver",
        choices=["systemd-user", "systemd-system", "shell"],
        default="shell",
        help=(
            "Execution backend when --allow-exec is set: shell (default), "
            "systemd-user, or systemd-system"
        ),
    )
    return p.parse_args(argv)


def _encode_commands_frame(cmds: list[CommandConfig]) -> bytes:
    lines = ["COMMANDS v1"]
    for c in cmds:
        # Format: "<id> <label>", truncate to LCD width
        # Ensure no newlines sneak in
        lid = str(c.id).replace("\n", " ").strip()
        lbl = str(c.label).replace("\n", " ").strip()
        lines.append(f"{lid} {lbl}"[:20])
    return Outbound(lines=lines).encode()


def _start_via_systemd(
    cmd_str: str, cmd_id: str, label: str, scope: str, log: logging.Logger
) -> None:
    """Start command using systemd-run --user as a transient unit.

    Logs outcome; relies on the user session systemd. Output is captured by journald.
    """
    # Compose a simple unit name: lcdcmd-<id>-<timestamp>
    unit = f"lcdcmd-{cmd_id}-{int(time.time())}"
    # Wrap command in /bin/sh -lc to support shell strings from config
    full = ["systemd-run"]
    if scope == "systemd-user":
        full.append("--user")
    # For system scope, omit --user to run under the system manager
    full += [
        "--unit",
        unit,
        "--collect",
        "--property=Restart=no",
        "/bin/sh",
        "-lc",
        cmd_str,
    ]
    try:
        subprocess.run(full, check=True, capture_output=True, text=True)
        log.info("started systemd unit=%s id=%s label=%s", unit, cmd_id, label)
    except Exception as e:
        log.error("failed to start systemd unit for id=%s label=%s: %s", cmd_id, label, e)


def _maybe_execute(cmd: CommandConfig, allow: bool, driver: str, log: logging.Logger) -> None:
    if not allow:
        log.info("execution blocked id=%s label=%s cmd=%s", cmd.id, cmd.label, cmd.exec)
        return
    if not cmd.exec:
        log.info("no exec configured for id=%s label=%s", cmd.id, cmd.label)
        return
    if driver in ("systemd-user", "systemd-system"):
        _start_via_systemd(cmd.exec, str(cmd.id), cmd.label, driver, log)
        return
    # Shell driver (compatible, less safe)
    try:
        subprocess.Popen(cmd.exec, shell=True)
        log.info("started exec id=%s label=%s", cmd.id, cmd.label)
    except Exception as e:
        log.error("failed to start exec id=%s label=%s: %s", cmd.id, cmd.label, e)


def _handle_incoming_line(
    line: str,
    ser: serial.Serial,
    cfg: AppConfig,
    log: logging.Logger,
    allow_exec: bool = False,
    exec_driver: str = "shell",
) -> None:
    msg = line.strip()
    if msg == "REQ COMMANDS":
        payload = _encode_commands_frame(cfg.commands)
        try:
            ser.write(payload)
            ser.flush()
        except Exception as e:  # pragma: no cover - hardware dependent
            log.error("failed to send commands: %s", e)
        return
    if msg.startswith("SELECT "):
        sel = msg[len("SELECT ") :].strip()
        cmd = next((c for c in cfg.commands if str(c.id) == sel), None)
        label = cmd.label if cmd else ""
        log.info("selected id=%s label=%s", sel, label)
        if cmd is not None:
            _maybe_execute(cmd, allow_exec, exec_driver, log)


def _reader(
    ser: serial.Serial,
    stop: threading.Event,
    cfg: AppConfig,
    log: logging.Logger,
    allow_exec: bool,
    exec_driver: str,
) -> None:  # pragma: no cover
    while not stop.is_set():
        try:
            line = ser.readline()
            if line:
                try:
                    text = line.decode(errors="replace").rstrip()
                    print(f"[arduino] {text}")
                    _handle_incoming_line(text, ser, cfg, log, allow_exec=allow_exec, exec_driver=exec_driver)
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
    limit = cfg.max_lines - 1 if cfg.max_lines > 1 else 1
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
        if len(lines) >= limit:
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
        lines.insert(0, f"META interval={cfg.interval:.3f}")
        for ln in lines:
            print(ln)
        return 0

    backoff = 5.0
    ser: serial.Serial | None = None
    warned = False
    while ser is None:
        try:
            ser = serial.Serial(cfg.serial.port, cfg.serial.baud, timeout=0.2)
            if warned:
                log.info("Opened serial port %s", cfg.serial.port)
            break
        except Exception as e:
            if not warned:
                log.warning("Serial port unavailable (%s): %s", cfg.serial.port, e)
                warned = True
            else:
                log.debug("Serial port still unavailable: %s", e)
            try:
                time.sleep(backoff)
            except KeyboardInterrupt:
                return 3
            backoff = min(backoff * 2.0, 30.0)

    if ser is None:
        return 3

    try:
        reader_stop = threading.Event()
        reader_thread: threading.Thread | None = None
        if not args.no_echo:
            reader_thread = threading.Thread(
                target=_reader,
                args=(
                    ser,
                    reader_stop,
                    cfg,
                    log,
                    bool(args.allow_exec),
                    str(args.exec_driver),
                ),
                daemon=True,
            )
            reader_thread.start()
        while True:
            lines = _collect_lines(cfg)
            lines.insert(0, f"META interval={cfg.interval:.3f}")
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

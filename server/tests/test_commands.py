from __future__ import annotations

import io
import logging
from types import SimpleNamespace

from src.config import AppConfig, CommandConfig
from src.main import _encode_commands_frame, _handle_incoming_line


def test_encode_commands_frame_truncates() -> None:
    cfg_cmds = [
        CommandConfig(id="1", label="Shutdown"),
        CommandConfig(id="2", label="This label is definitely longer than twenty chars"),
    ]
    payload = _encode_commands_frame(cfg_cmds)
    text = payload.decode()
    lines = [l for l in text.splitlines() if l]
    assert lines[0] == "COMMANDS v1"
    # Ensure truncation to <= 20
    assert all(len(l) <= 20 for l in lines[1:])


class FakeSerial:
    def __init__(self) -> None:
        self.writes: list[bytes] = []

    def write(self, b: bytes) -> int:  # pragma: no cover - trivial
        self.writes.append(b)
        return len(b)

    def flush(self) -> None:  # pragma: no cover - trivial
        return None


def test_handle_incoming_req_commands_writes_frame(caplog) -> None:
    cfg = AppConfig(commands=[CommandConfig(id="1", label="Shutdown")])
    ser = FakeSerial()
    caplog.set_level(logging.INFO)
    _handle_incoming_line("REQ COMMANDS", ser, cfg, logging.getLogger(__name__))
    assert ser.writes, "expected a commands frame to be written"
    frame = ser.writes[-1].decode()
    assert frame.startswith("COMMANDS v1\n") or "COMMANDS v1\n" in frame


def test_handle_incoming_select_logs_label(caplog) -> None:
    cfg = AppConfig(commands=[CommandConfig(id="42", label="Reboot", exec="shutdown -r now")])
    ser = FakeSerial()
    caplog.set_level(logging.INFO)
    logger = logging.getLogger("test")
    _handle_incoming_line("SELECT 42", ser, cfg, logger, allow_exec=False)
    msgs = "\n".join(r.message for r in caplog.records)
    assert "selected id=42 label=Reboot" in msgs
    assert "execution blocked" in msgs

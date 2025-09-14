from __future__ import annotations

import logging
from types import SimpleNamespace

from src.config import AppConfig, CommandConfig
from src.main import _handle_incoming_line
import src.main as main_mod


class DummySerial:
    def write(self, b: bytes) -> int:  # pragma: no cover - not used
        return len(b)

    def flush(self) -> None:  # pragma: no cover - not used
        return None


def test_select_exec_blocked(monkeypatch, caplog) -> None:
    cfg = AppConfig(commands=[CommandConfig(id="1", label="Shutdown", exec="shutdown -h now")])
    ser = DummySerial()
    caplog.set_level(logging.INFO)
    _handle_incoming_line("SELECT 1", ser, cfg, logging.getLogger(__name__), allow_exec=False)
    msgs = "\n".join(r.message for r in caplog.records)
    assert "execution blocked" in msgs


def test_select_exec_runs_when_allowed(monkeypatch, caplog) -> None:
    cfg = AppConfig(commands=[CommandConfig(id="2", label="Echo", exec="/bin/echo hi")])
    ser = DummySerial()
    called = {}

    def fake_popen(cmd, shell):  # type: ignore[no-redef]
        called["cmd"] = cmd
        called["shell"] = shell
        return SimpleNamespace()

    monkeypatch.setattr(main_mod.subprocess, "Popen", fake_popen)
    caplog.set_level(logging.INFO)
    _handle_incoming_line("SELECT 2", ser, cfg, logging.getLogger(__name__), allow_exec=True)
    assert called["cmd"] == "/bin/echo hi"
    assert called["shell"] is True
    msgs = "\n".join(r.message for r in caplog.records)
    assert "started exec id=2 label=Echo" in msgs


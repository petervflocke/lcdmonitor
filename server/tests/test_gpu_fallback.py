from __future__ import annotations

from types import SimpleNamespace

import src.metrics as metrics


def test_gpu_summary_uses_nvidia_smi_when_nvml_absent(monkeypatch) -> None:
    # Force NVML loader to return None
    monkeypatch.setattr(metrics, "_load_nvml", lambda: None)

    # Fake nvidia-smi output: gpu%, mem.used, mem.total, temp (no units)
    fake_out = "12, 512, 2048, 45\n"

    def fake_run(cmd, capture_output, text, check):  # type: ignore[no-redef]
        assert "nvidia-smi" in cmd[0]
        return SimpleNamespace(stdout=fake_out)

    monkeypatch.setattr(metrics.subprocess, "run", fake_run)

    s = metrics.gpu_summary()
    # 12% GPU, 25% mem (512/2048), 45C
    assert s is not None
    assert "%" in s and "C" in s
    assert " 12%" in s
    assert " 25%" in s
    assert " 45C" in s

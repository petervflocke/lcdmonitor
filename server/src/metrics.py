from __future__ import annotations
import warnings
import psutil
from typing import Optional


def cpu_summary() -> str:
    t = psutil.sensors_temperatures()
    temp = None
    for k, arr in t.items():
        if arr:
            temp = arr[0].current
            break
    mem = psutil.virtual_memory().percent
    cpu = psutil.cpu_percent(interval=None)
    return f"{cpu:.0f}% {mem:.0f}% {'' if temp is None else f'{temp:.0f}C'}"


def _load_nvml():
    """Return NVML module (imported as 'pynvml') or None.

    Prefer the 'nvidia-ml-py' distribution which exposes the 'pynvml' module
    without deprecation warnings. If the legacy 'pynvml' package is installed,
    suppress its FutureWarning.
    """
    try:
        with warnings.catch_warnings():
            # Some distros ship legacy 'pynvml' with a FutureWarning on import.
            # Silence only that specific message to avoid noisy logs.
            warnings.filterwarnings(
                "ignore",
                category=FutureWarning,
                message=r"^The pynvml package is deprecated.*",
            )
            import pynvml  # type: ignore

        return pynvml
    except Exception:
        return None


def gpu_summary() -> Optional[str]:
    nvml = _load_nvml()
    if nvml is None:
        return None
    try:
        nvml.nvmlInit()
        h = nvml.nvmlDeviceGetHandleByIndex(0)
        util = nvml.nvmlDeviceGetUtilizationRates(h)
        mem = nvml.nvmlDeviceGetMemoryInfo(h)
        temp = nvml.nvmlDeviceGetTemperature(h, nvml.NVML_TEMPERATURE_GPU)
        mem_pct = 0
        try:
            if getattr(mem, "total", 0):
                mem_pct = int(round((mem.used / mem.total) * 100))
        except Exception:
            mem_pct = 0
        # Show mem utilization in percent instead of MB
        return f"{util.gpu}% {mem_pct}% {temp}C"
    except Exception:
        return None
    finally:
        try:
            nvml.nvmlShutdown()
        except Exception:
            pass

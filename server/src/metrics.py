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
    return f"CPU {cpu:.0f}% {mem:.0f}% {'' if temp is None else f'{temp:.0f}C'}"


def _load_nvml():
    """Return NVML module (imported as 'pynvml') or None.

    Prefer the 'nvidia-ml-py' distribution which exposes the 'pynvml' module
    without deprecation warnings. If the legacy 'pynvml' package is installed,
    suppress its FutureWarning.
    """
    try:
        with warnings.catch_warnings():
            warnings.filterwarnings("ignore", category=FutureWarning, module=r"^pynvml$")
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
        return f"GPU {util.gpu}% {mem.used // (1024**2)}MB {temp}C"
    except Exception:
        return None
    finally:
        try:
            nvml.nvmlShutdown()
        except Exception:
            pass

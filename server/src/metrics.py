from __future__ import annotations
import psutil
from typing import Optional

try:
    import pynvml  # type: ignore
    _HAS_NVML = True
except Exception:
    _HAS_NVML = False


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


def gpu_summary() -> Optional[str]:
    if not _HAS_NVML:
        return None
    try:
        pynvml.nvmlInit()
        h = pynvml.nvmlDeviceGetHandleByIndex(0)
        util = pynvml.nvmlDeviceGetUtilizationRates(h)
        mem = pynvml.nvmlDeviceGetMemoryInfo(h)
        temp = pynvml.nvmlDeviceGetTemperature(h, pynvml.NVML_TEMPERATURE_GPU)
        return f"GPU {util.gpu}% {mem.used//(1024**2)}MB {temp}C"
    except Exception:
        return None
    finally:
        try:
            pynvml.nvmlShutdown()
        except Exception:
            pass
from __future__ import annotations
import warnings
import psutil
from typing import Optional, Iterable, Any


def _pad3(n: int) -> str:
    # Right-align in width 3
    return f"{n:>3d}"


def cpu_summary() -> str:
    t = psutil.sensors_temperatures(fahrenheit=False)
    temp_val = None
    for _k, arr in t.items():
        if arr:
            try:
                temp_val = int(round(arr[0].current))
            except Exception:
                temp_val = None
            break
    mem = int(round(psutil.virtual_memory().percent))
    cpu = int(round(psutil.cpu_percent(interval=None)))
    parts = [f"{_pad3(cpu)}%", f"{_pad3(mem)}%"]
    if temp_val is not None:
        parts.append(f"{_pad3(temp_val)}C")
    return " ".join(parts)


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
        # Show mem utilization in percent instead of MB, width 3
        return f"{_pad3(int(util.gpu))}% {_pad3(mem_pct)}% {_pad3(int(temp))}C"
    except Exception:
        return None
    finally:
        try:
            nvml.nvmlShutdown()
        except Exception:
            pass


def _pick_temp_entry(arr: Iterable[Any]) -> Optional[int]:
    for entry in arr:
        try:
            return int(round(entry.current))
        except Exception:
            continue
    return None


def temp_summary(chip: Optional[str] = None, label: Optional[str] = None) -> Optional[str]:
    """Return a temperature like " 32C" from lm-sensors via psutil.

    - chip: substring to match chip key (e.g., "coretemp", "nvme").
    - label: exact label match within that chip (e.g., "Package id 0", "Composite").
    """
    temps = psutil.sensors_temperatures(fahrenheit=False)
    items = temps.items()
    for key, arr in items:
        if chip and chip not in key:
            continue
        if label:
            labeled = [e for e in arr if getattr(e, "label", None) == label]
            if labeled:
                val = _pick_temp_entry(labeled)
                if val is not None:
                    return f"{_pad3(val)}C"
            # fall through to try any entry if no exact label found
        val = _pick_temp_entry(arr)
        if val is not None:
            return f"{_pad3(val)}C"
    return None

from __future__ import annotations
import warnings
import subprocess
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
    """Return GPU summary using NVML, with nvidia-smi fallback.

    Output format: "gpu% mem% tempC" (each right-aligned to width 3).
    Returns None if no GPU metrics are available.
    """
    nvml = _load_nvml()
    if nvml is not None:
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
            return f"{_pad3(int(util.gpu))}% {_pad3(mem_pct)}% {_pad3(int(temp))}C"
        except Exception:
            pass
        finally:
            try:
                nvml.nvmlShutdown()
            except Exception:
                pass

    # NVML not available or failed; try nvidia-smi fallback
    return _gpu_summary_nvidia_smi()


def _gpu_summary_nvidia_smi() -> Optional[str]:
    """Query nvidia-smi for utilization, memory and temperature.

    Returns formatted string or None if nvidia-smi is not available or parsing fails.
    """
    try:
        # Query without units, CSV no header: util.gpu, mem.used, mem.total, temp.gpu
        res = subprocess.run(
            [
                "nvidia-smi",
                "--query-gpu=utilization.gpu,memory.used,memory.total,temperature.gpu",
                "--format=csv,noheader,nounits",
            ],
            capture_output=True,
            text=True,
            check=True,
        )
        line = res.stdout.strip().splitlines()[0]
        parts = [p.strip() for p in line.split(",")]
        if len(parts) < 4:
            return None
        gpu_util = int(round(float(parts[0])))
        mem_used = float(parts[1])
        mem_total = float(parts[2])
        temp = int(round(float(parts[3])))
        mem_pct = int(round((mem_used / mem_total) * 100)) if mem_total else 0
        return f"{_pad3(gpu_util)}% {_pad3(mem_pct)}% {_pad3(temp)}C"
    except Exception:
        return None


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

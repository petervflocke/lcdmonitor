from __future__ import annotations

# Ensure imports like `from src...` resolve when pytest chooses a higher rootdir.
# Adds the project "server" directory to sys.path.
import sys
from pathlib import Path

PROJECT_DIR = Path(__file__).resolve().parents[1]
if str(PROJECT_DIR) not in sys.path:
    sys.path.insert(0, str(PROJECT_DIR))


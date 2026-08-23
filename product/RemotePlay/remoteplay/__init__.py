"""MTV Remote Play support for PlayStation and Xbox controllers.

Compatible use::

    from remoteplay import GCVWorker
    worker = GCVWorker()   # frame-driven: mirror + detection + release
"""
from __future__ import annotations

from .state import ControllerState, NEUTRAL_STATE

# Backward-compatible exports (the legacy threaded bridge is still testable).
from .bridge import ControllerBridge, start_bridge, stop_bridge
from .input import ControllerUnavailable, InputHandler
from .output import VirtualControllerUnavailable, VirtualXboxOutput

# New frame-driven bridge (raw ViGEm virtual pad + raw XInput controller read).
from ._vigem import ViGEmPad
from ._xinput import XInputReader

try:
    from ._mtvremoteplay_core import GCVWorker, CVWorker
except ImportError:
    try:
        from ._mtvbridge_core import CVWorker as GCVWorker
        CVWorker = GCVWorker
    except ImportError:
        GCVWorker = None
        CVWorker = None

__all__ = [
    "ControllerBridge", "ControllerState", "ControllerUnavailable",
    "CVWorker", "GCVWorker", "InputHandler", "NEUTRAL_STATE",
    "ViGEmPad", "VirtualControllerUnavailable", "VirtualXboxOutput",
    "XInputReader", "start_bridge", "stop_bridge",
]

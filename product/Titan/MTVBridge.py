"""Helios loader. The Titan implementation is compiled and protected.

This file is intentionally tiny: it only locates the compiled core
(_mtvbridge_core.cp311-win_amd64.pyd) and loads it.  Any startup problem
prints a clear, actionable message instead of a raw traceback.
"""
import os
import sys
from pathlib import Path

source_folder = Path(__file__).resolve().parent
runtime_folder = source_folder
if (source_folder / "SHA256SUMS.txt").is_file():
    # A release folder is immutable. If Helios is accidentally pointed here,
    # transparently use the installed runtime instead of returning a generic
    # SystemExit error or allowing runtime state to modify the distributable.
    candidate = (Path(os.environ.get("LOCALAPPDATA", "")) /
                 "HeliosProject" / "Helios" / "python" / "MTV" /
                 "Titan")
    if (candidate / "MTVBridge.py").is_file() and not (candidate / "SHA256SUMS.txt").exists():
        runtime_folder = candidate
        print("[MTV] Redirected extracted script to installed Titan runtime.", flush=True)
    else:
        print("[MTV] Run INSTALL_OR_REPAIR_MTV.cmd before starting this script.", flush=True)
        raise SystemExit(1)

folder = str(runtime_folder)
if folder not in sys.path:
    sys.path.insert(0, folder)


def _say(message):
    for stream in (sys.stdout, sys.stderr):
        try:
            print("[MTV] " + message, file=stream, flush=True)
        except Exception:
            pass


def _fail(message):
    _say(message)
    _say("Fix: run INSTALL_OR_REPAIR_MTV.cmd in this folder, then restart Helios.")
    raise SystemExit(1)


def _core_pyds():
    try:
        return sorted(
            n for n in os.listdir(folder)
            if n.startswith("_mtvbridge_core") and n.endswith(".pyd")
        )
    except OSError:
        return []


try:
    from _mtvbridge_core import *  # noqa: F401,F403
except ModuleNotFoundError as exc:
    if exc.name != "_mtvbridge_core":
        _fail("Missing Python package '%s'. Run INSTALL_OR_REPAIR_MTV.cmd "
              "to install all dependencies." % exc.name)
    if not _core_pyds():
        _fail("MTV core is missing: no _mtvbridge_core*.pyd was found next to "
              "MTVBridge.py. Re-extract the WHOLE product folder - do not copy "
              "MTVBridge.py on its own.")
    _fail("Python version mismatch: this build needs Python 3.11 (cp311), but "
          "Helios is running Python %d.%d. Run INSTALL_OR_REPAIR_MTV.cmd to "
          "point Helios at the MTV Python 3.11 environment."
          % (sys.version_info.major, sys.version_info.minor))
except Exception as exc:
    _fail("MTV core failed to load: %r. Re-run INSTALL_OR_REPAIR_MTV.cmd." % exc)

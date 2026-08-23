"""Helios loader for MTV Remote Play.

Runs a pre-flight check (engine deps + ViGEm virtual pad + physical
controller) and prints a clear, actionable line for each failure so the
bridge can never silently fail to start. The implementation itself lives in
``remoteplay._mtvremoteplay_core`` (frame-driven, in-process).
"""
from __future__ import annotations

import os
import sys
from pathlib import Path

here = Path(__file__).resolve().parent
product_root = here.parent if (here / "__init__.py").exists() else here

# A release folder contains its checksum manifest; an installed copy does not.
# If Helios is pointed at the extracted release folder, transparently use the
# installed runtime instead (same rule as the Titan loader). This turns the
# old hard "SystemExit: 1" into either a working session or a clear message,
# and runtime state can never mutate the distributable. When no runtime is
# installed yet, the customer must run the installer first.
if (product_root / "SHA256SUMS.txt").is_file():
    candidate = (Path(os.environ.get("LOCALAPPDATA", "")) /
                 "HeliosProject" / "Helios" / "python" / "MTV" / "RemotePlay")
    if (candidate / "MTVRemotePlay.py").is_file() and not (candidate / "SHA256SUMS.txt").exists():
        print("[MTV] Redirected extracted script to installed Remote Play runtime.", flush=True)
        here = candidate
        product_root = candidate
    else:
        print("[MTV] Run INSTALL_OR_REPAIR_MTV.cmd before starting this script.", flush=True)
        raise SystemExit("Run INSTALL_OR_REPAIR_MTV.cmd before starting this script, then restart Helios.")

folder = str(product_root)
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
    # Exit with the reason as the exception value so Helios's "Last error:"
    # line shows the actionable message instead of an opaque "SystemExit: 1".
    raise SystemExit("[MTV] " + message +
                     " Fix: run INSTALL_OR_REPAIR_MTV.cmd, then restart Helios.")


def _core_pyds():
    pkg = here / "remoteplay"
    try:
        return sorted(n for n in os.listdir(pkg) if n.endswith(".pyd"))
    except OSError:
        return []


def _preflight() -> None:
    """Print one-line status for every dependency the bridge needs."""
    ok = True
    try:
        import numpy  # noqa: F401
        print("[MTV] preflight: numpy OK", flush=True)
    except Exception as exc:
        print(f"[MTV] preflight: numpy FAILED ({exc})", flush=True)
        ok = False
    try:
        import cv2  # noqa: F401
        print("[MTV] preflight: opencv OK", flush=True)
    except Exception as exc:
        print(f"[MTV] preflight: opencv FAILED ({exc})", flush=True)
        ok = False

    # Virtual pad: ViGEmBus driver + ViGEmClient.dll (bundled next to this
    # script). This is what the game actually reads in remote play.
    try:
        from remoteplay._vigem import ViGEmPad, _find_dll
        dll = _find_dll()
        if not dll:
            print("[MTV] preflight: ViGEmClient.dll NOT FOUND -- reinstall the "
                  "product (the DLL must sit next to MTVRemotePlay.py).", flush=True)
            ok = False
        else:
            print(f"[MTV] preflight: ViGEmClient.dll OK ({Path(dll).name})", flush=True)
        pad = ViGEmPad()
        if pad.available:
            print("[MTV] preflight: ViGEmBus virtual pad OK", flush=True)
            pad.close()
        else:
            print("[MTV] preflight: ViGEmBus driver NOT running -- install "
                  "ViGEmBus and restart Helios, or no timed release can fire.",
                  flush=True)
            ok = False
    except Exception as exc:
        print(f"[MTV] preflight: ViGEm check FAILED ({exc})", flush=True)
        ok = False

    # Physical controller: Xbox pads via XInput, PlayStation pads
    # (DualSense/DualShock) via SDL -- both non-exclusive, so Helios or the
    # remote-play app can still watch the pad.
    try:
        from remoteplay._sdl_input import SDLControllerReader
        from remoteplay._xinput import XInputReader
        slots = XInputReader.scan_connected()
        sdl_ids = SDLControllerReader.scan_ids()
        if slots:
            print(f"[MTV] preflight: controller OK (XInput slot(s) "
                  f"{', '.join(str(s) for s in slots)})", flush=True)
        elif sdl_ids:
            print("[MTV] preflight: controller OK (PlayStation pad via SDL)",
                  flush=True)
        else:
            print("[MTV] preflight: NO controller detected. Plug the pad into "
                  "THIS PC via USB and press a button on it.", flush=True)
            ok = False
    except Exception as exc:
        print(f"[MTV] preflight: controller scan FAILED ({exc})", flush=True)
        ok = False

    if not ok:
        print("[MTV] preflight: one or more checks FAILED -- see lines above.",
              flush=True)


if __name__ != "__main__":
    # Only print diagnostics when loaded as a Helios CV script, not on
    # direct execution (keeps the console clean for --help style runs).
    _preflight()

try:
    from remoteplay._mtvremoteplay_core import *  # noqa: E402,F401,F403
except ModuleNotFoundError as exc:
    if not _core_pyds():
        _fail("MTV core is missing: the 'remoteplay' folder has no compiled "
              ".pyd files. Re-extract the WHOLE product folder - do not copy "
              "MTVRemotePlay.py on its own.")
    if sys.version_info[:2] != (3, 11):
        _fail("Python version mismatch: this build needs Python 3.11 (cp311), "
              "but Helios is running Python %d.%d. Run INSTALL_OR_REPAIR_MTV.cmd "
              "to point Helios at the MTV Python 3.11 environment."
              % (sys.version_info.major, sys.version_info.minor))
    _fail("Missing module '%s'. Run INSTALL_OR_REPAIR_MTV.cmd to reinstall "
          "all components." % (exc.name or "?"))
except Exception as exc:
    _fail("MTV core failed to load: %r. Re-run INSTALL_OR_REPAIR_MTV.cmd." % exc)

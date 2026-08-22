"""Compact MTV control panel for the phase-predicted Helios bridge."""
from __future__ import annotations

import json, os, sys, time, traceback
from pathlib import Path
from PyQt6.QtCore import Qt, QTimer
from PyQt6.QtGui import QIcon, QPixmap
from PyQt6.QtWidgets import (
    QApplication, QCheckBox, QComboBox, QDialog, QDoubleSpinBox, QFrame,
    QGridLayout, QHBoxLayout, QLabel, QLineEdit, QMainWindow, QPushButton,
    QSizePolicy, QSpinBox, QVBoxLayout, QWidget,
)

ROOT = Path(__file__).resolve().parent
# The bridge launches this panel with explicit paths when its runtime ROOT is
# redirected (e.g. the dev shared/ layout), so the panel edits and live-reads
# the same config the detector actually uses.
_CONFIG_ENV = os.environ.get("MTV_CONFIG_PATH")
_LIVE_ENV = os.environ.get("MTV_LIVE_PATH")
CONFIG_PATH = Path(_CONFIG_ENV) if _CONFIG_ENV else ROOT / "mtv_config.json"
LIVE_PATH = Path(_LIVE_ENV) if _LIVE_ENV else ROOT / "mtv_live.json"
LOGO_PATH = ROOT / "mtv_logo.png"
METERS_DIR = ROOT / "meters"
LOG_PATH = ROOT / "mtv_gui.log"
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
from mtv_license import core as license_core


def log_exc(context=""):
    """Append the active traceback to mtv_gui.log instead of dying silently."""
    try:
        with open(LOG_PATH, "a", encoding="utf-8") as fh:
            fh.write(f"\n[{time.strftime('%Y-%m-%d %H:%M:%S')}] {context}\n")
            traceback.print_exc(file=fh)
    except Exception:
        pass


def _to_float(value, default=0.0):
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def _to_int(value, default=0):
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def format_remaining(value, compact=False):
    if value is None:
        return "LIFETIME" if compact else "Lifetime (never expires)"
    seconds = max(0, int(_to_float(value, 0.0)))
    days, seconds = divmod(seconds, 86400)
    hours, seconds = divmod(seconds, 3600)
    minutes = seconds // 60
    if compact:
        if days:
            return f"{days}D {hours}H"
        if hours:
            return f"{hours}H {minutes}M"
        return f"{minutes}M"
    if days:
        return f"{days} day{'s' if days != 1 else ''}, {hours} hour{'s' if hours != 1 else ''}"
    if hours:
        return f"{hours} hour{'s' if hours != 1 else ''}, {minutes} minute{'s' if minutes != 1 else ''}"
    return f"{minutes} minute{'s' if minutes != 1 else ''}"

REFERENCE = {
    "meter_profile_type": "Arrow2", "meter_profile_color": "Purple",
    "meter_rgb_lower": [190, 0, 190], "meter_rgb_upper": [255, 70, 255],
    "meter_profile_occupancy_min": 0.30,
    "morph_kernel_w": 3, "morph_kernel_h": 3, "morph_iterations": 1,
    "min_component_area": 4,
    "fill_gap_bridge_px": 7, "max_late_overshoot_ratio": 0.6,
    "scan_downscale": 2, "roi_ratio": [0.0, 0.20, 1.0, 0.68],
    "color_space": "rgb",
    "target_height": 64.0, "adaptive_geometry": True, "target_margin_px": 0.0,
    "threshold_margin_px": 0.0, "threshold_margin_max_px": 0.0,
    "meter_width_normalize": False, "green_detect_enabled": True,
    "green_calibrate_lead_px": 16.0,
    "vision_localizer_enabled": True, "vision_localizer_resource": "",
    "vision_localizer_interval_ms": 300.0,
    "live_write_interval_ms": 250.0,
    "vision_localizer_input_px": 320, "vision_localizer_confidence": 0.45,
    "regular_shot_timing_ms": 11.5, "timing_adjust_ms": 18.5,
    "phase_gpc_lead_ms": 8.0,
    "phase_trigger_window_ms": 45.0, "phase_predict_early_enable": True,
    "trigger_cooldown_ms": 250, "top_lock_inner_top_pad_px": 2,
    "top_lock_inner_bottom_pad_px": 2, "top_lock_inner_margin_x_pct": 0.30,
    "fill_tip_min_pixels_per_row": 2, "lost_frames": 6,
    "track_confirm_frames": 2, "track_max_move_px": 80.0,
    "arm_min_rise_px": 1.5, "candidate_min_score": 0.45,
    "track_window_enable": True, "track_window_pad_x": 160,
    "track_window_pad_y": 220, "idle_early_reject": True,
    "idle_reject_scale": 4,
    "show_hud": True, "debug_mode": False,
    "controller_type": "PlayStation", "dunk_release_ms": 50,
    "tempo_mid_value": 40, "tempo_ms": 65, "neutral_pause_ms": 40,
    "force_regular_only": True,
    "hold_power": 100, "flick_power": 100, "up_flick_ms": 300,
    "activation_threshold": 55,
    "meter_zoom_enable": False, "meter_zoom_scale": 2.0,
    "meter_zoom_corner": "top_right", "meter_zoom_pad_px": 28,
}


def load_config():
    try:
        cfg = json.loads(CONFIG_PATH.read_text(encoding="utf-8-sig"))
    except Exception:
        cfg = {}
    if not isinstance(cfg, dict):
        cfg = {}
    for key, value in REFERENCE.items():
        cfg.setdefault(key, value)
    for key in ("auto_green_calibrate", "green_calibrate_lead_px",
                "release_style", "circle_rate", "circle_sweep_deg",
                "circle_radius", "dunk_style"):
        cfg.pop(key, None)
    cfg["vision_localizer_enabled"] = True
    cfg["meter_width_normalize"] = False
    return cfg


def load_live():
    """Read the bridge's live telemetry (mtv_live.json) for display only.

    Never merged into the settings file: the bridge writes this file, the UI
    only shows it. Keeping it separate is what stops the running bridge from
    overwriting the tuned values in mtv_config.json.
    """
    try:
        live = json.loads(LIVE_PATH.read_text(encoding="utf-8-sig"))
    except Exception:
        return {}
    return live if isinstance(live, dict) else {}


def save_config(cfg):
    tmp = CONFIG_PATH.with_name(f"{CONFIG_PATH.name}.{os.getpid()}.tmp")
    try:
        tmp.write_text(json.dumps(cfg, indent=2), encoding="utf-8")
    except Exception:
        log_exc("save_config write")
        return
    for _ in range(8):
        try:
            os.replace(tmp, CONFIG_PATH)
            return
        except OSError:
            time.sleep(.025)
    try:
        tmp.unlink()
    except Exception:
        pass


class LicenseDialog(QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("MTV Activation")
        self.setFixedWidth(430)
        layout = QVBoxLayout(self)
        title = QLabel("ACTIVATE MTV")
        title.setObjectName("DialogTitle")
        layout.addWidget(title)
        layout.addWidget(QLabel("Send this Hardware ID to the seller:"))
        hwid_row = QHBoxLayout()
        self.hwid = QLineEdit(license_core.get_hwid())
        self.hwid.setReadOnly(True)
        hwid_row.addWidget(self.hwid)
        self.copy_hwid = QPushButton("COPY HWID")
        self.copy_hwid.clicked.connect(self.copy_hardware_id)
        hwid_row.addWidget(self.copy_hwid)
        layout.addLayout(hwid_row)
        layout.addWidget(QLabel("License key"))
        self.key = QLineEdit()
        self.key.setPlaceholderText("MTV2...")
        layout.addWidget(self.key)
        self.activate = QPushButton("ACTIVATE KEY")
        self.activate.clicked.connect(self.activate_key)
        layout.addWidget(self.activate)
        self.status = QLabel("")
        self.status.setWordWrap(True)
        self.status.setStyleSheet(
            "background:#17181d; border:1px solid #34363d; border-radius:6px; "
            "padding:10px; color:#d7d8dd;"
        )
        layout.addWidget(self.status)
        self.refresh_license_status()

    def copy_hardware_id(self):
        QApplication.clipboard().setText(self.hwid.text())
        self.status.setText("Hardware ID copied. Send it to the seller for your key.")

    def activate_key(self):
        text = self.key.text().strip()
        result = license_core.activate(text)
        if result.get("ok"):
            self.key.clear()
            self.refresh_license_status()
        else:
            reason = license_core.diagnose_key(text).get("reason")
            msg = {
                "format": "That isn't a valid MTV2 key. Re-copy the FULL key "
                          "(it starts with MTV2 and has no spaces/line breaks).",
                "missing_public_key": "This build cannot verify keys (missing "
                                      "public key). Reinstall MTV.",
                "missing_cryptography": "This build is missing a required "
                                        "component. Reinstall MTV.",
                "signature": "The key doesn't match this product's signature. "
                             "Check for copy/paste errors — one character off "
                             "breaks it.",
                "claims": "The key is damaged or not a valid license.",
            }.get(reason, str(result.get("error", "Invalid license key")))
            self.status.setText(
                "<b style='color:#ff3047'>NOT ACTIVATED</b><br>" + msg
            )

    def refresh_license_status(self):
        info = license_core.check_license()
        if not info.get("ok"):
            reason = {
                "no_license": "Enter the HWID key supplied by the seller.",
                "legacy_key_upgrade_required": "The old cached license is no longer valid. Request an HWID key.",
                "hwid_mismatch": "This key belongs to a different PC.",
                "expired": "This license has expired.",
                "clock_invalid": "The PC clock is earlier than the license issue time.",
                "clock_rollback": "A backwards PC clock change was detected.",
                "discord_mismatch": "This key belongs to a different Discord account.",
                "invalid_signature": "The saved license key is invalid or damaged.",
            }.get(info.get("reason"), "No valid HWID license is active.")
            self.status.setText(
                "<b style='color:#ff3047'>STATUS: NOT ACTIVE</b><br>" + reason
            )
            return
        remaining = format_remaining(info.get("expires_in"))
        license_id = str(info.get("license_id") or "—").upper()
        self.status.setText(
            "<b style='color:#42e582'>STATUS: ACTIVE</b><br>"
            f"Plan: {info.get('label', info.get('tier', 'License'))}<br>"
            f"Time remaining: {remaining}<br>"
            f"License ID: {license_id}<br>"
            f"Bound HWID: {info.get('hwid', license_core.get_hwid())}"
        )


class Card(QFrame):
    def __init__(self, title):
        super().__init__()
        self.setObjectName("Card")
        self.body = QVBoxLayout(self)
        self.body.setContentsMargins(18, 16, 18, 16)
        self.body.setSpacing(12)
        label = QLabel(title.upper())
        label.setObjectName("CardTitle")
        self.body.addWidget(label)


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setObjectName("MainWindow")
        self.setWindowTitle("MTV — Meter Timing Vision")
        self.setFixedSize(920, 740)
        if LOGO_PATH.exists():
            self.setWindowIcon(QIcon(str(LOGO_PATH)))
        self.cfg = load_config()
        self.last_shot = _to_float(self.cfg.get("_live_last_shot"), 0.0)
        self._last_detected = None
        self._last_license_check = 0.0
        self._unlocked = False
        self._license_info = {"ok": False, "reason": "no_license"}
        self._activation_dialog_open = False
        self.build_ui()
        self.apply_style()
        self.refresh()
        self.timer = QTimer(self)
        self.timer.timeout.connect(self.refresh)
        # Status is display-only; a coarse 4 Hz poll keeps the GUI quiet while
        # Helios owns the 60 FPS capture loop.
        self.timer.setTimerType(Qt.TimerType.CoarseTimer)
        self.timer.start(250)

    def build_ui(self):
        root = QWidget()
        self.setCentralWidget(root)
        page = QVBoxLayout(root)
        page.setContentsMargins(20, 16, 20, 18)
        page.setSpacing(14)

        header = QHBoxLayout()
        if LOGO_PATH.exists():
            logo = QLabel()
            logo.setPixmap(QPixmap(str(LOGO_PATH)).scaled(56, 56, Qt.AspectRatioMode.KeepAspectRatio,
                                                        Qt.TransformationMode.SmoothTransformation))
            logo.setFixedSize(60, 60)
            header.addWidget(logo)
        names = QVBoxLayout()
        name = QLabel("MTV")
        name.setObjectName("Brand")
        subtitle = QLabel("METER TIMING VISION")
        subtitle.setObjectName("Subtitle")
        names.addWidget(name)
        names.addWidget(subtitle)
        header.addLayout(names)
        header.addStretch()
        self.license_badge = QPushButton("LICENSE")
        self.license_badge.setObjectName("QuietButton")
        self.license_badge.clicked.connect(self.open_license_dialog)
        header.addWidget(self.license_badge)
        page.addLayout(header)

        live = Card("Live detector")
        live_row = QHBoxLayout()
        self.state_dot = QLabel("●")
        self.state_dot.setObjectName("StateDot")
        self.state = QLabel("SEARCHING")
        self.state.setObjectName("State")
        live_row.addWidget(self.state_dot)
        live_row.addWidget(self.state)
        live_row.addStretch()
        self.top_value = QLabel("TOP — px")
        self.top_value.setObjectName("LiveNumber")
        live_row.addWidget(self.top_value)
        live.body.addLayout(live_row)
        self.live_detail = QLabel("Arrow2 • Purple • profile timing 50 ms")
        self.live_detail.setObjectName("Muted")
        live.body.addWidget(self.live_detail)
        # Remote Play bridge health (only present in the Remote Play product).
        # Turns silent failures (no pad on this PC, game reading the wrong
        # controller) into a visible one-line status.
        self.rp_status = QLabel("")
        self.rp_status.setWordWrap(True)
        self.rp_status.setStyleSheet("color:#9aa0ab;")
        self.rp_status.hide()
        live.body.addWidget(self.rp_status)
        page.addWidget(live)

        columns = QHBoxLayout()
        columns.setSpacing(18)
        timing = Card("Timing")
        timing_grid = QGridLayout()
        timing_grid.setHorizontalSpacing(12)
        timing_grid.setVerticalSpacing(12)
        timing_grid.addWidget(QLabel("Target"), 0, 0)
        self.target_height = QDoubleSpinBox()
        self.target_height.setRange(0.0, 600.0)
        self.target_height.setDecimals(1)
        self.target_height.setSingleStep(0.5)
        self.target_height.setSuffix(" px")
        self.target_height.setValue(float(self.cfg.get("target_height", 64.0)))
        self.target_height.setToolTip("How high the meter fills before the shot releases (px from the base toward the top).")
        self.target_height.valueChanged.connect(lambda value: self.set_value("target_height", value))
        timing_grid.addWidget(self.target_height, 0, 1)
        timing_grid.addWidget(QLabel("Dunk"), 1, 0)
        self.dunk = QSpinBox()
        self.dunk.setRange(10, 200)
        self.dunk.setSingleStep(5)
        self.dunk.setSuffix(" ms")
        self.dunk.setValue(int(self.cfg.get("dunk_release_ms", 50)))
        self.dunk.setToolTip("Dunk hold in ms (RT + right stick held through the release). 50 = the default; raise if dunks land late, lower if early.")
        self.dunk.valueChanged.connect(lambda value: self.set_value("dunk_release_ms", value))
        timing_grid.addWidget(self.dunk, 1, 1)
        timing.body.addLayout(timing_grid)
        note = QLabel("Shot: hold stick → flick. Dunk: hold RT + click L3 → timed release.")
        note.setObjectName("Muted")
        note.setWordWrap(True)
        timing.body.addWidget(note)

        # Quick ±step buttons (no typing / scroll wheel): one tap applies and
        # saves immediately. Target steps in px (coarse ±5/±1, fine ±0.5/±0.1),
        # Dunk in whole ms.
        def add_step_row(row, label, key, lo, hi, steps, step=1.0):
            row_box = QHBoxLayout()
            lbl = QLabel(label)
            lbl.setObjectName("Muted")
            row_box.addWidget(lbl)
            for delta in steps:
                text = ("%+.1f" % delta) if abs(delta) < 1 else ("%+d" % delta)
                btn = QPushButton(text)
                btn.setObjectName("StepButton")
                btn.setFixedSize(46, 26)
                btn.setToolTip("%+.2g %s" % (delta, label))
                btn.clicked.connect(
                    lambda _=False, d=delta: self.step_value(key, d, lo, hi, step))
                row_box.addWidget(btn)
            row_box.addStretch()
            timing_grid.addLayout(row_box, row, 0, 1, 3)
        add_step_row(2, "Target ±", "target_height", 0.0, 600.0, (-5, -1, 1, 5))
        add_step_row(3, "Target .5/.1", "target_height", 0.0, 600.0,
                     (-0.5, -0.1, 0.1, 0.5))
        add_step_row(4, "Dunk ±", "dunk_release_ms", 10.0, 200.0, (-5, -1, 1, 5))

        self.test = QPushButton("SEND TEST SHOT")
        self.test.clicked.connect(self.test_shot)
        timing.body.addWidget(self.test)
        columns.addWidget(timing, 1)

        meter_card = Card("Meter")
        meter_grid = QGridLayout()
        meter_grid.setHorizontalSpacing(12)
        meter_grid.setVerticalSpacing(12)
        meter_grid.addWidget(QLabel("Type"), 0, 0)
        self.meter = QComboBox()
        self.meter.addItems(["Arrow2", "Pill", "Straight", "Funnel", "Tube", "Dial", "Sword", "Arrow", "_2kOL2", "Frame2"])
        self.meter.setCurrentText(str(self.cfg.get("meter_profile_type", "Arrow2")))
        self.meter.currentTextChanged.connect(self.set_meter)
        meter_grid.addWidget(self.meter, 0, 1)
        meter_grid.addWidget(QLabel("Color"), 1, 0)
        self.color = QComboBox()
        self.color.addItems(["Purple", "Yellow", "Red", "White", "Orange", "Blue"])
        self.color.setCurrentText(str(self.cfg.get("meter_profile_color", "Purple")))
        # activated passes the item INDEX; always resolve it to text first so
        # a broken integer never lands in meter_profile_color.
        self.color.activated.connect(
            lambda idx: self.on_color(self.color.itemText(int(idx))))
        meter_grid.addWidget(self.color, 1, 1)
        meter_card.body.addLayout(meter_grid)
        self.hud = QCheckBox("Show detector overlay")
        self.hud.setChecked(bool(self.cfg.get("show_hud", True)))
        self.hud.toggled.connect(lambda value: self.set_value("show_hud", value))
        self.zoom = QCheckBox("Meter zoom (corner magnifier)")
        self.zoom.setChecked(bool(self.cfg.get("meter_zoom_enable", False)))
        self.zoom.setToolTip("Magnify the meter into the top-right corner of the "
                             "capture with the tip/target drawn on it. Backend "
                             "keys: meter_zoom_scale, meter_zoom_corner, "
                             "meter_zoom_pad_px.")
        self.zoom.toggled.connect(lambda value: self.set_value("meter_zoom_enable", value))
        meter_card.body.addWidget(self.hud)
        meter_card.body.addWidget(self.zoom)
        self.restore = QPushButton("RESTORE DEFAULTS")
        self.restore.setObjectName("QuietButton")
        self.restore.clicked.connect(self.restore_reference)
        meter_card.body.addWidget(self.restore)
        columns.addWidget(meter_card, 1)
        page.addLayout(columns)

        footer = QHBoxLayout()
        self.shots = QLabel("0 SHOTS")
        self.shots.setObjectName("Muted")
        footer.addWidget(self.shots)
        footer.addStretch()
        self.saved = QLabel("REFERENCE MODE")
        self.saved.setObjectName("FooterBadge")
        footer.addWidget(self.saved)
        page.addLayout(footer)

    def apply_style(self):
        self.setStyleSheet("""
            * { font-family: 'Segoe UI'; font-size: 12px; color: #eee8ea; }
            QMainWindow, QWidget { background: #08090b; }
            QFrame#Card { background: #101114; border: 1px solid #282a30; border-radius: 9px; }
            QLabel#Brand { color: #ff263d; font-size: 27px; font-weight: 900; letter-spacing: 2px; }
            QLabel#Subtitle { color: #888a92; font-size: 10px; font-weight: 700; letter-spacing: 2px; }
            QLabel#CardTitle { color: #ff3047; font-size: 10px; font-weight: 800; letter-spacing: 1px; }
            QLabel#State { font-size: 20px; font-weight: 900; }
            QLabel#StateDot { color: #ff3047; font-size: 20px; }
            QLabel#LiveNumber { color: #ff3047; font-size: 25px; font-weight: 900; }
            QLabel#Muted { color: #8d8f98; font-size: 11px; }
            QLabel#FixedValue { color: #ff5062; font-weight: 800; }
            QLabel#FooterBadge { color: #ff5062; font-size: 10px; font-weight: 800; }
            QPushButton { background: #ff263d; color: white; border: none; border-radius: 6px;
                          min-height: 31px; padding: 0 13px; font-weight: 800; }
            QPushButton:hover { background: #ff4357; }
            QPushButton#QuietButton { background: #17181d; border: 1px solid #34363d; color: #c6c6cb; }
            QPushButton#StepButton { background: #17181d; border: 1px solid #34363d; color: #c6c6cb;
                          font-size: 11px; font-weight: 700; border-radius: 4px; }
            QPushButton#StepButton:hover { background: #ff263d; color: white; }
            QComboBox, QSpinBox, QDoubleSpinBox, QLineEdit { background: #17181d; border: 1px solid #34363d;
                          border-radius: 5px; min-height: 29px; padding: 0 8px; selection-background-color: #ff263d; }
            QComboBox::drop-down { border: none; width: 24px; }
            QCheckBox { spacing: 7px; }
            QCheckBox::indicator { width: 16px; height: 16px; border: 1px solid #44464f; border-radius: 3px; }
            QCheckBox::indicator:checked { background: #ff263d; border-color: #ff263d; }
            QDialog { background: #0b0c0f; }
            QLabel#DialogTitle { color: #ff3047; font-size: 22px; font-weight: 900; }
        """)

    def set_value(self, key, value):
        try:
            fresh = load_config()
            fresh[key] = value
            save_config(fresh)
            self.cfg = fresh
            self.saved.setText("SAVED")
            QTimer.singleShot(800, lambda: self.saved.setText("REFERENCE MODE"))
        except Exception:
            log_exc(f"set_value({key})")

    def step_value(self, key, delta, lo, hi, step=1.0):
        """Apply a +/- step to a tuning value and save it immediately."""
        try:
            fresh = load_config()
            current = _to_float(fresh.get(key, lo), lo)
            raw = current + delta * step
            value = min(hi, max(lo, raw))
            fresh[key] = value
            save_config(fresh)
            self.cfg = fresh
            # Keep the spinbox in sync without a second save.
            if key == "target_height":
                self.target_height.blockSignals(True)
                self.target_height.setValue(value)
                self.target_height.blockSignals(False)
            elif key == "dunk_release_ms":
                self.dunk.blockSignals(True)
                self.dunk.setValue(int(round(value)))
                self.dunk.blockSignals(False)
            self.saved.setText("SAVED")
            QTimer.singleShot(800, lambda: self.saved.setText("REFERENCE MODE"))
        except Exception:
            log_exc(f"step_value({key})")

    def open_license_dialog(self):
        if self._activation_dialog_open:
            return
        self._activation_dialog_open = True
        try:
            LicenseDialog(self).exec()
        finally:
            self._activation_dialog_open = False
            self._last_license_check = 0.0

    def on_color(self, value):
        """Color selection saved immediately.

        Qt's activated signal can hand us an INDEX (int) instead of the name;
        always resolve it to the item text so a raw integer can never land in
        meter_profile_color (it would break the profile lookup).
        """
        try:
            if isinstance(value, int) or (isinstance(value, str) and value.isdigit()):
                idx = int(value)
                value = self.color.itemText(idx) if 0 <= idx < self.color.count() else "Purple"
            text = str(value)
            fresh = load_config()
            fresh["meter_profile_color"] = text
            save_config(fresh)
            self.cfg = fresh
            self.saved.setText("SAVED")
            QTimer.singleShot(800, lambda: self.saved.setText("REFERENCE MODE"))
        except Exception:
            log_exc(f"on_color({value})")

    def set_meter(self, meter_name):
        try:
            fresh = load_config()
            fresh["meter_profile_type"] = meter_name
            try:
                profile = json.loads((METERS_DIR / f"{meter_name}.json").read_text(encoding="utf-8"))
                fresh["meter_profile_color"] = profile.get("selected", {}).get("color", "Purple")
            except Exception:
                pass
            save_config(fresh)
            self.cfg = fresh
            self.color.blockSignals(True)
            self.color.setCurrentText(str(fresh.get("meter_profile_color", "Purple")))
            self.color.blockSignals(False)
            self.saved.setText("PROFILE LOADED")
        except Exception:
            log_exc(f"set_meter({meter_name})")

    def test_shot(self):
        try:
            fresh = load_config()
            fresh["_test_shot"] = 1
            save_config(fresh)
            self.test.setText("TEST SHOT QUEUED")
            QTimer.singleShot(900, lambda: self.test.setText("SEND TEST SHOT"))
        except Exception:
            log_exc("test_shot")

    def restore_reference(self):
        try:
            fresh = load_config()
            fresh.update(REFERENCE)
            save_config(fresh)
            self.cfg = fresh
            self.dunk.blockSignals(True); self.dunk.setValue(50); self.dunk.blockSignals(False)
            self.color.blockSignals(True); self.color.setCurrentText("Purple"); self.color.blockSignals(False)
            self.meter.blockSignals(True); self.meter.setCurrentText("Arrow2"); self.meter.blockSignals(False)
            self.hud.blockSignals(True); self.hud.setChecked(True); self.hud.blockSignals(False)
            self.zoom.blockSignals(True); self.zoom.setChecked(False); self.zoom.blockSignals(False)
            self.target_height.blockSignals(True); self.target_height.setValue(64.0); self.target_height.blockSignals(False)
            self.saved.setText("DEFAULTS RESTORED")
        except Exception:
            log_exc("restore_reference")

    def refresh(self):
        try:
            self._refresh()
        except Exception:
            log_exc("refresh")

    def _refresh(self):
        cfg = load_config()
        cfg.update(load_live())
        detected = bool(cfg.get("_live_detected", False))
        state = str(cfg.get("_live_state", "SEARCHING"))
        gap = _to_float(cfg.get("_live_gap_px"), 0.0)
        velocity = _to_float(cfg.get("_live_velocity_px_ms"), 0.0)
        self.state.setText(state)
        if detected != self._last_detected:
            self._last_detected = detected
            self.state_dot.setStyleSheet(f"color: {'#ff3047' if detected else '#555861'};")
        self.top_value.setText(f"GAP {gap:.0f} px" if detected else "GAP — px")
        profile = str(cfg.get("_live_profile") or cfg.get("meter_profile_type", "Arrow2"))
        color = str(cfg.get("_live_profile_color") or cfg.get("meter_profile_color", "Purple"))
        effective = _to_float(cfg.get("_live_effective_timing_ms"), 0.0)
        box = cfg.get("_live_box")
        box_text = ""
        if isinstance(box, (list, tuple)) and len(box) == 4:
            try:
                box_text = f" • {int(box[2])} × {int(box[3])}"
            except (TypeError, ValueError):
                box_text = ""
        target = _to_float(cfg.get("_live_target_height"), 0.0)
        self.live_detail.setText(f"{profile} • {color} • TGT {target:.0f}px • "
                                 f"{velocity:.2f}px/ms{box_text}")
        rp_running = cfg.get("_live_rp_running")
        if rp_running is not None:
            rp_input = str(cfg.get("_live_rp_input") or "NONE")
            rp_err = str(cfg.get("_live_rp_error") or "")
            rp_shots = _to_int(cfg.get("_live_rp_shots"), 0)
            rp_connected = bool(cfg.get("_live_rp_input_connected", False))
            if rp_connected:
                rp_armed = "ARMED" if cfg.get("_live_rp_armed") else "ready"
                rp_mode = str(cfg.get("_live_rp_mode") or "").upper()
                suffix = f" • {rp_mode}" if rp_mode else ""
                rp_text = (f"RemotePlay: {rp_armed} • {rp_input}{suffix} • "
                           f"{rp_shots} releases")
                rp_color = "#42e582"
            else:
                reason = rp_err or "no physical controller detected"
                rp_text = f"RemotePlay: NO CONTROLLER ({reason}) — plug the pad into this PC"
                rp_color = "#ff3047"
            self.rp_status.setText(rp_text)
            self.rp_status.setStyleSheet(f"color:{rp_color}; font-weight:600;")
            self.rp_status.show()
        else:
            self.rp_status.hide()
        count = _to_int(cfg.get("_live_shot_count"), 0)
        self.shots.setText(f"{count} SHOT{'S' if count != 1 else ''}")
        shot = _to_float(cfg.get("_live_last_shot"), 0.0)
        if shot and shot != self.last_shot:
            self.last_shot = shot
            self.saved.setText("SHOT RELEASED")
            QTimer.singleShot(1100, lambda: self.saved.setText("REFERENCE MODE"))
        # is_unlocked() hits the registry + license file; cache it briefly
        # instead of doing that I/O eight times a second.
        now = time.monotonic()
        if now - self._last_license_check > 1.0:
            self._last_license_check = now
            was_unlocked = self._unlocked
            try:
                self._license_info = license_core.check_license()
                self._unlocked = bool(self._license_info.get("ok"))
            except Exception:
                self._unlocked = False
                self._license_info = {"ok": False, "reason": "error"}
            if was_unlocked and not self._unlocked:
                QTimer.singleShot(0, self.open_license_dialog)
        if self._unlocked:
            remaining = format_remaining(self._license_info.get("expires_in"), compact=True)
            self.license_badge.setText(remaining if remaining == "LIFETIME" else f"{remaining} LEFT")
        else:
            self.license_badge.setText("ACTIVATE")


def main():
    app = QApplication(sys.argv)
    app.setApplicationName("MTV")

    def _excepthook(exc_type, exc_value, exc_tb):
        try:
            with open(LOG_PATH, "a", encoding="utf-8") as fh:
                fh.write(f"\n[{time.strftime('%Y-%m-%d %H:%M:%S')}] UNCAUGHT\n")
                traceback.print_exception(exc_type, exc_value, exc_tb, file=fh)
        except Exception:
            pass
        sys.__excepthook__(exc_type, exc_value, exc_tb)
    sys.excepthook = _excepthook

    window = MainWindow()
    window.show()
    if not license_core.is_unlocked():
        QTimer.singleShot(0, window.open_license_dialog)
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())

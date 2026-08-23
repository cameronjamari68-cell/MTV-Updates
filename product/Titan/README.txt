MTV — METER TIMING VISION
=========================

MTV is a Helios/Titan Two computer-vision shot releaser for NBA 2K. It watches
the shot meter on the capture card, measures how full the meter is, fires
when the fill reaches the release point, and drives the right-stick release
gesture through the Titan Two.

HOW THE VISION WORKS (corrected)
--------------------------------
The measurement model is the original MT Vision release design:

    capture -> search ROI -> color mask -> speckle removal -> close
            -> contours -> MT Vision contour filters -> candidate score
            -> track (SEARCH/ACQUIRE/TRACK/LOST/reacquire)
            -> fill height measured from the meter BASE upward
            -> fill reaches Target -> fire (deterministic threshold)

The meter is a ~165 px vertical bar: the fill rises from the base toward the
top. The release point (Target) is how high the fill reaches before MTV fires.
Green-window detection is disabled, so the green band is never measured or
used to move the target.

Target is the release point: the number you set is exactly how high the meter
fills (px above the base, toward the top) before the shot releases. There is
no green-window prediction — the detector does not measure or chase the green
band. Raise Target to release later (fill closer to the top), lower it to
release earlier. Flick is the tempo; raise it if the tempo reads rushed.

Detection uses Arrow2 geometry authored to the real captured meter (contour
width 23-30, height 33-165, min_aspect 1.5, min_solidity 0.55, bbox
19/28/19/23). This matches the actual ~26-28px-wide fill strictly, so the
relaxed fallback is only needed for genuinely smaller (downscaled) captures.

Two extra guards keep detection consistent across court locations: the tracker
only ARMS once the fill has actually risen since first sight (arm_min_rise_px),
so a static purple background object (crowd/ad/jersey) can never steal the
lock; and a candidate scoring below candidate_min_score is rejected outright
rather than being trusted.

Once the shot arms, `full_meter_lock: true` expands the detection box to the
WHOLE meter (top to base) instead of following the fill up from the bottom, so
the fill is always measured against the same stable full-bar window. The box
base is EMA-smoothed while armed, so frame-to-frame contour jitter can't make
the box (or the target line) bounce.

INSTALL / USE
-------------
1. Keep this folder together. Do not move individual files out of it.
2. In Helios, select MTVBridge.py as the CV Python script.
3. Compile and install MTV.gpc into the Titan Two memory slot.
4. Start the Helios capture and Titan bridge.
5. The compact MTV window opens automatically.
6. In NBA 2K Controller Settings, set Shooting Input to All (recommended)
   or Pro Stick Only. Shot Button Only prevents rhythm/tempo registration.

PLAYSTATION 5 CONNECTION
------------------------
The regular MTV.gpc never writes the left-stick X/Y channels; ordinary
movement uses Titan Two's default passthrough. If no controls work on PS5,
the PS5 has not accepted/authenticated the Titan output. Follow the included
PS5_TITAN_SETUP.txt before changing any MTV timing values.

TIMING / SHOOTING
-----------------
The timing panel exposes two knobs:

  - Target  — release point (px above the meter base). Raise if shots fire
              early, lower if late.
  - Dunk    — dunk hold in ms (RT + right stick held through the release).
              50 ms is MT Vision's default; raise if dunks land late, lower if early.

The tempo (full-flick hold) is fixed at 300 ms in the MT Vision back end,
so only Target and Dunk need tuning.

The GPC release gesture is MT Vision's tempo flick:

    half-flick OPPOSITE the hold (50% power, small X drift) -> 50 ms
    full flick (100% power, more X drift) -> hold `Flick` ms (300 default)
    center (40 ms)

The long full-power hold is what makes 2K read the tempo correctly — a fast
30%->75%->100% ramp read as rushed. The small X drift keeps it a rhythm flick,
not a plain stick shot.

Regular shots are held DOWN and released with an UP flick; fades are held UP
(or up-diagonal) and released with a DOWN flick. The GPC reads the held stick
direction at the trigger and flicks the opposite way, so the same tempo flick
times both shot types.

DUNK METER
----------
Hold RT (right trigger) and click the left stick (L3) to start a dunk. MTV
routes the same meter detection to the MT Vision dunk release: RT full + right stick
DOWN held for the Dunk ms, then centered. 2K's dunk green window is wide, so
the short 50 ms hold lands the fill inside it. Release timing is the Dunk knob.

RELEASE GESTURE
---------------
The GPC release gesture is configurable (config key `release_style`, or the
Release dropdown in the launcher):

  - flick (default)  — MT Vision tempo flick: half-power 50 ms, full-power hold for
    the tempo (300 ms), center. The long full-power hold is what makes 2K read
    the tempo correctly.
  - circle           — MT Vision right-stick circle sweep: the stick starts
    straight down and rotates through `circle_sweep_deg` degrees (default 720)
    at `circle_rate` deg/ms (default 1.3) with `circle_radius` percent power
    (default 100), sampled every 15 ms, then centers. The full rotation is the
    "spin" release gesture; the detector timing is identical to flick, only the
    stick motion changes. Circle knobs are clamped in both the payload and the
    GPC, so a bad value can never send a nonsense sweep.

Dunks have two styles (`dunk_style`, Dunk style dropdown):

  - hold (default)   — MT Vision: RT full + right stick DOWN held for the Dunk ms,
    then center (70 ms tail).
  - wave             — MT Vision oscillation (-50/-100/-50/0/+50) ending in
    a +100 down hold for 1000 ms, then center. Alternative timing for dunk
    meters that read better with a moving stick.

There is no down-hold re-assert and no tempo counter-hold after the flick —
those read as bad timing/tempo in 2K and were breaking both fades and standing
shots. With the default `release_mode: threshold` the release fires the
moment the fill reaches Target (minus `threshold_margin_px`), so no velocity
projection can drift early or late. Set `release_mode: predict` to re-enable
the Kalman/phase prediction for A/B testing.

Fades: the physical right stick passes straight through the GPC during the
charge (GPC2 default passthrough), so the direction you hold it locks the shot
type. No GCV stick round-trip that could zero/stale the held stick.

TARGET
------
The Timing panel exposes "Target". The value you set is exactly how high the
meter fills (px above the base, toward the top) before the release fires — no
green-window prediction. Raise it to release later, lower it to release
earlier; the live "TGT" readout shows the effective value.

By default `release_mode: threshold` releases when the fill reaches that
height. The lead is speed-normalized (margin = fill velocity x the transport
lead, clamped between `threshold_margin_px` and `threshold_margin_max_px`), so
a fast shot and a slow shot register the release at the same time before the
target — same green however quickly the meter fills. The target scale is also
EMA-smoothed, so a pass, catch, or camera flip cannot yank the release point
mid-shot. Set `release_mode: predict` to switch back to the velocity/phase
predictor for A/B testing.

A meter that is already near-full when first seen (a leftover from the previous
shot) is ignored rather than fired as a phantom.

ADAPTIVE GEOMETRY
-----------------
The profile geometry (Arrow2 fill width 23-30px) matches the real meter, so it
is detected strictly. For a heavily downscaled capture (~8px-wide fill) the
strict profile finds nothing and MTV retries with relaxed width/height/aspect
bounds, scaling the release height by the observed fill width so the bar still
fires at the same point. The relaxed pass has an 8px width floor, so a thin
line or dotted pattern (jersey/crowd) is never surfaced as a meter. Disable
with `adaptive_geometry: false` to force the exact reference geometry.

PERFORMANCE FAST PATHS (ON by default -- results are identical)
--------------------------------------------------------------
The full scan (color threshold + morphology on the whole search box) is the
per-frame cost. Two fast paths keep the numbers down without changing a single
measurement:

  - Track window (`track_window_enable`): while the tracker is locked the
    meter can't teleport between frames, so detection runs only on a padded
    window around the last box (`track_window_pad_x/y`, default 160/220 px)
    instead of the entire search box. Same full-res measurements, a fraction
    of the morphology work.
  - Idle early-reject (`idle_early_reject`): when nothing is locked (between
    possessions) a 4x-downscaled color check skips the expensive morphology on
    purple-free frames entirely. No color -> no candidate -> identical result.

Both are verified byte-identical against the full scan on real footage (same
fill heights, boxes, gaps and shot fires). The runtime also keeps OpenCV on a
single native worker, throttles the optional localizer to about 3 Hz, and
updates display-only telemetry at 4 Hz so short CPU spikes do not fight the
capture thread. Turn either fast path off in mtv_config.json if you ever need
the absolute old behavior.

SHOT TELEMETRY
--------------
Every fire appends one JSON line to `shot_log.jsonl` (fill px, gap px, target,
velocity, meter width, flick ms, score). Review it after a
session to see whether shots fired early/late and whether the tempo value you
set is actually the one that reached the GPC.

ADVANCED FEATURES (all OFF by default so the tuned behavior is unchanged)
------------------------------------------------------------------------

Adaptive color (`adaptive_color_enable`)
  Slowly EMA-tracks the fill's observed color and shifts the RGB bounds toward
  it (clamped near the authored bounds), so lighting/drift can't push the fill
  outside the fixed range. Only learns from armed (rising) meters.

Screen color cue (`screen_cue_enable`)
  A "no meter" fallback: when a color (e.g. a green flash) is confirmed in
  `screen_cue_roi` for `screen_cue_confirm_frames` frames, the release fires on
  its own. Independent of the meter tracker; set `screen_cue_color_hex` and the
  ROI to match your game's cue.

Template anchor (`template_anchor_enable`)
  Matches a template PNG (`template_anchor_path`) in `template_anchor_roi` and
  narrows the search region toward the match while SEARCHing, so background
  purple on the far side of the screen can't compete. Refreshed every 200 ms
  and cleared as soon as tracking starts. Provide your own template image.

Latency / jitter compensation (`latency_comp_enable`)
  Measures the capture frame-interval jitter and widens the release margin
  (up to `latency_comp_margin_max_px`) when frames arrive unevenly, so a
  stutter can't make the phase predictor fire late. This is local pipeline
  jitter compensation, not game-server sniffing.

FAR SHOTS (track-height normalization)
-------------------------------------
When you shoot from farther away (or at a different capture resolution) the
whole meter renders smaller, so every raw pixel value (fill, target) shrinks
with it. With `meter_width_normalize: true` (default) the release point is
scaled by the observed meter track height against the profile's 1080p
reference, so the Target you set means "how far the fill rises toward the top
of THIS meter". A far (smaller) meter gets a proportionally smaller release
point, so the bar still fills to the same spot everywhere on the court. At
your usual range the scale is ~1.00 and Target is untouched.

The scale is EMA-smoothed and clamped (0.5x-1.3x) so one bad reading can never
throw a shot far off, and it ALWAYS applies -- even a small near/far change
(e.g. the live meter's 26 vs 28 px width) keeps the release at the same
fraction of the bar instead of drifting between early and late. No reference
width is learned or remembered, so a first far or close shot can never skew
every later shot.

SEARCH REGION
-------------
The meter can sit at very different screen positions depending on camera angle
and shot type (its base has been seen from y~552 to y~838). The Arrow2 search
box now covers the full range (top 180, bottom 880) so the fill is never
clipped at the bottom edge -- a clipped fill used to read ~20-30px short and
fire some shots wildly early/late.

A shot can fire again on the next meter rise once the fill resets (empties) or
the meter is lost and re-acquired; a single-frame jump or a static fill never
re-triggers.

OVERLAY
-------
The Meter panel's "Show detector overlay" checkbox (show_hud) draws the
live detector state, the fill height, gap, velocity and FPS on the capture.
`debug_mode` (config only) additionally draws the detection box, fill box,
tip/target lines and rejected candidates.

METER ZOOM
----------
meter_zoom_enable magnifies the meter into a corner of the capture
(meter_zoom_scale, default 2.0x, meter_zoom_corner top_right,
meter_zoom_pad_px 28) with the fill tip / release target drawn on the zoom. Pure display; drawn even
when the HUD is off.

QUICK STEP BUTTONS
------------------
The Timing panel has -5 / -1 / +1 / +5 buttons for Target (px) and Dunk
(ms): one tap applies and saves immediately -- no typing or scroll wheel.
Target also has a fine -0.5 / -0.1 / +0.1 / +0.5 row.

MT Vision tuning is handled through this launcher. The GPC release path stays
fixed so launcher adjustments do not change the controller gesture.

TEST
----
Run:  python selftest.py          (product self-test)
      python test_mtv_pipeline.py (88-check regression suite)

Expected: both print a PASS line. No Helios, capture card or controller needed;
the tests run against synthetic frames that model the real captured meter.

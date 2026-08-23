# Helios Inference Python API Reference

This file is the exact callable Python surface for Helios's `helios.inference`
API inside the packaged CV Python host.

The public reference is shipped under
`versions/<version>/sdk/docs/`. Its native public contracts are shipped under
`versions/<version>/sdk/include/helios/`. The Python binding and runtime
implementations remain private Helios source and are not distributed as SDK
source files.

## Scope

- This document covers the Python API that is actually exposed by the `helios.inference` API, backed by the native `inference_core` module.
- If a native C API exists in `InferenceCore.h` but is not wrapped by the Python module, it is **not** available as a normal Python call from this binding.
- This document is optimized for agents: use the exact signatures and accepted argument shapes shown here.
- In the packaged CV Python runtime, the host owns frame sequencing and inference start. Scripts are consumers of results plus engine settings; they do not trigger inference directly.

Implementation note:

- `helios.inference` is the canonical script import. The host registers it as a
  lazy alias for the native stable-ABI `inference_core.pyd` extension; no
  Python shim file or version-specific Cython build is used.
- The callable Python surface is intended to stay the same across supported Python versions.

## Agent Rules

1. Create a session with `create_inference_engine(...)`.
2. Apply settings on the returned `InferenceSession`.
3. Call `engine.start(...)` before calling `engine.wait_for_results_raw(...)`.
4. Call `engine.destroy()` when finished. `destroy()` already calls `stop()`.
5. Do not assume every C API in `InferenceCore.h` is reachable from Python.
6. Do not try to trigger inference from Python script code. The packaged host triggers inference for the selected frame before `process()` runs; script code should only wait for results and apply settings.
7. Recommended ROI size and recommended class priority are enabled by default on a new session and after model loads until you switch to another mode.
8. If model metadata does not provide a recommended ROI, runtime fallback is `640x640`.
9. If model metadata does not provide a recommended class priority, runtime fallback is class `0`.
10. Treat OCR as a lower-priority side result. Configure explicit OCR regions, enable OCR, and read OCR results by `region_id`.

## Canonical Call Order

```python
from helios import inference

models = inference.list_models()
engine = inference.create_inference_engine(models[0]["uuid"])

engine.set_confidence_threshold(0.50)
engine.set_nms_threshold(0.45)
engine.set_roi(0.5, 0.5, 1.0, 16.0, 9.0)
engine.set_sort_method(inference.SORT_DISTANCE)

engine.start(ring_buffer_name=None, helios_pid=None)

while True:
    # In the packaged CV Python host, frame S is already selected and
    # inference for S has already been triggered before process() runs.
    # Do script work first if you want overlap.
    result = engine.wait_for_results_raw()
    if result is not None:
        num_detections = result[4]
        metrics = result[8][:num_detections]
        # consume the borrowed result views promptly

engine.destroy()
```

## Important Runtime Assumptions

- Current packaged client layout:
  - install root: `Helios.exe`
  - runtime root: `versions/<version>/`
  - `versions/<version>/lib/HeliosApp.exe`
  - `versions/<version>/lib/InferenceCoreTrt10.dll`
  - `versions/<version>/lib/InferenceCoreTrt11.dll`
  - `versions/<version>/lib/onnxruntime.dll`
  - `versions/<version>/lib/onnxruntime_providers_shared.dll`
  - `versions/<version>/lib/DirectML.dll`
  - `versions/<version>/lib/ocr/ppocrv6_small_rec/inference.onnx`
  - `versions/<version>/lib/ocr/ppocrv6_small_rec/inference.yml`
  - `versions/<version>/lib/py/cvpython_host.pyd`
  - `versions/<version>/lib/py/inference_core.pyd`
- In the packaged client, user CV Python code runs inside the selected `python.exe`, not directly inside `HeliosApp.exe`.
- `cvpython_host.pyd` owns the native frame loop inside that selected Python process.
- Object inference uses the resolved Helios Compute GPU. `auto` is the default and selects the strongest NVIDIA/TensorRT adapter first, then AMD/DirectML, then another DirectML GPU. A missing compatible Auto GPU or unavailable explicit GPU is a hard error. NVIDIA selections use TensorRT engine caches (`.ecache` / `.eecache`). DirectML selections use ONNX source weights (`.onnx` / `.ennx`).
- Every loadable model manifest entry must contain the exact task string
  `"detect"`, `"pose"`, or `"segment"`. Task is never inferred from output
  shape, normalized, or defaulted; missing and unsupported values fail model
  loading.
- The TensorRT path registers the mapped video ring memory with CUDA and uploads frames directly from that shared BGR memory. It still requires page-locked output buffers. If CUDA host-memory registration/allocation fails, inference start/model load fails instead of falling back to a staged upload path.
- The DirectML path preprocesses the selected frame or ROI into a reused RGB CHW input tensor, then runs the ONNX model through ONNX Runtime DirectML. Object models must assign fully to DirectML except for the single standard ONNX `Mod` node used by supported YOLO end-to-end output graphs; that node may execute through the CPU provider because DirectML does not support its `int64` tensor type. Any other CPU assignment fails model loading.
- OCR uses ONNX Runtime DirectML and PP-OCRv6 small recognition ONNX as one required backend path. It has separate results from object detection.
- `inference_core.pyd` is loaded from `lib/py` and resolves the matching `InferenceCoreTrt10.dll` or `InferenceCoreTrt11.dll` from the packaged `lib/` directory.
- The selected InferenceCore DLL verifies its signed release entry and the exact
  `cvpython_host.pyd` host identity functions before model functionality becomes
  available. Importing the binding in an unrelated Python process is not a
  supported hosting mode.
- `create_inference_engine(...)` and `load_model(...)` do not take a backend selector. The packaged host passes `HELIOS_COMPUTE_ADAPTER` from Helios Preferences before the native DLL initializes.
- Supported modes are independent: scripts may use no engine, object inference only, OCR only, or object inference plus OCR. Object model loading and OCR startup fail independently when the requested GPU path cannot initialize.
- One `InferenceSession` owns at most one loaded object model. Calling
  `load_model(...)` replaces it; create multiple sessions to run multiple
  object models for the same host-selected frame.
- The host registers and triggers every active session. Each session has its
  own worker, result mappings, completion event, model resources, and optional
  OCR runtime, so additional sessions add real GPU/CPU load rather than forming
  one batched inference call.
- Each session supports up to 16 configured OCR regions. One lower-priority OCR
  worker processes at most one enabled region per scheduled frame in
  round-robin order; regions do not execute in parallel within one session.
- InferenceCore manages sessions, not scripts. The active CV Python host owns
  script execution and registers the sessions that script creates.
- When a script host prepares a protected-model run, it also sets:
  - `HELIOS_COMPUTE_ADAPTER`
  - `HELIOS_SCRIPT_TYPE`
  - `HELIOS_SCRIPT_HASH`
  - `HELIOS_EXECUTION_GRANT`
- Those runtime authorization variables are consumed by the native DLL, not by the public Python API. The account session token is never added to the child environment or exposed to Python.
- In the CV Python host, `HELIOS_SCRIPT_TYPE` is always `py`.
- `start()` defaults:
  - `ring_buffer_name=None`: uses `HELIOS_VIDEO_RING_BUFFER` from the environment when available
  - `helios_pid=None`: uses `HELIOS_PROCESS_ID` from the environment, otherwise `0`
- In `cvpython_host.pyd`, the host publishes `current_frame_sequence` and triggers inference for that exact frame before user `CVWorker.process(frame)` runs.
- Scripts do not get a public Python API to trigger inference manually.
- `wait_for_results_raw()` does not start inference.
  - In the packaged CV Python host, it waits on the session's last host-triggered frame sequence.
  - This still depends on host-owned frame triggering, not on Python script code.
  - The binding cannot be hosted outside the packaged, manifest-authorized CV
    Python runtime.

### Protected model behavior

- Plain `.onnx` and `.ecache` models do not request a model decryption key.
- Protected `.ennx` and `.eecache` models require trusted host identity and a
  host-issued execution grant whose referenced Helios account session remains active.
- Public protected models are available to an ordinary authenticated user.
  Private or locked models require author, administrator, or explicit model
  authorization.
- Runtime use also requires the selected uploaded script identity and its
  scoped execution grant.
- A protected model contacts the server every time it is opened. An existing
  `.eecache` does not run completely offline.
- Existing downloaded `.ennx` files and built `.eecache` files require no
  conversion or user action.

## Module Constants

| Name | Value | Meaning |
|------|-------|---------|
| `SORT_DISTANCE` | `0` | Sort detections by polar distance |
| `SORT_CONFIDENCE` | `1` | Sort detections by confidence |
| `MODEL_DETECTION` | `0` | Detection model |
| `MODEL_POSE` | `1` | Pose model |
| `MODEL_SEGMENTATION` | `2` | Instance-segmentation model |
| `SEGMENTATION_FORMAT_BITMASK_LSB` | `1` | Packed binary masks, least-significant bit first |
| `POSE_ANCHOR_NONE` | `0` | Disable pose-keypoint anchor override |
| `POSE_ANCHOR_HEAD` | `1` | Prefer head-derived pose anchor |
| `POSE_ANCHOR_CHEST` | `2` | Prefer chest-derived pose anchor |
| `POSE_ANCHOR_ABDOMEN` | `3` | Prefer abdomen-derived pose anchor |
| `OCR_STATUS_EMPTY` | `0` | No OCR result in this slot |
| `OCR_STATUS_READY` | `1` | OCR text is available |
| `OCR_STATUS_NO_TEXT` | `2` | OCR ran but decoded no text |
| `OCR_STATUS_ERROR` | `3` | OCR failed for that region/frame |
| `OCR_STATUS_DISABLED` | `4` | Region is configured but disabled |
| `OCR_FLAG_TRUNCATED` | `1` | OCR text was truncated to the fixed ABI buffer |

## Module-Level Python API

| Call | Signature | Returns | Notes |
|------|-----------|---------|-------|
| `get_debug_log` | `get_debug_log() -> str` | `str` | Returns the internal debug log |
| `list_models` | `list_models() -> List[Dict]` | `list[dict]` | Returns parsed JSON metadata for models compatible with the detected backend |
| `get_model_info` | `get_model_info(uuid: str) -> Dict` | `dict` | Returns the exact model metadata entry for a loadable model UUID |
| `create_inference_engine` | `create_inference_engine(uuid: str = "") -> InferenceSession` | `InferenceSession` | Creates a session; omit `uuid` for OCR-only or late model loading |

Model metadata returned by `list_models()` / `get_model_info()` may include:

- `uuid`
- `type`
- `backend`
- `encrypted`
- `size`
- `visibility`
- `requires_auth`
- `source_uuid`
- `task`
- `recommended_roi_width`
- `recommended_roi_height`
- `class_priority`
- `class_recommendations`
- `name`
- `author`
- `version`

On NVIDIA selections, `list_models()` returns TensorRT cache files only. On DirectML selections, it returns ONNX/ENNX source files only. Scripts should pass the returned `uuid` directly to `create_inference_engine()` or `load_model()`.
Names, authors, and other text metadata are returned as normal Python Unicode strings, including emoji.
The binding queries the exact native JSON size before reading it, so model discovery has no fixed 64 KiB response limit.

`class_recommendations`, when present, is a JSON object keyed by class ID string. Each value may contain:

- `tier`
- `class_name`
- `confidence`
- `anchor_pct_x`
- `anchor_pct_y`
- `offset_px_x`
- `offset_px_y`

## Exceptions

### `InferenceCoreError`

Raised when the DLL reports a non-zero error.

The code and `debug_log` describe the operation that just failed. Error state is local to the calling thread and is not a persistent log. Do not repeatedly catch and print the same model setup failure from the frame loop; retain the failure state until the requested model/configuration changes or an explicit refresh is requested.

Model-load diagnostics identify the manifest name, version, and author first,
then include the exact model/engine ID, selected backend and GPU, and the native
backend cause. TensorRT engine failures also include the cache profile and,
when available, TensorRT logger output plus CUDA allocation and free-memory
details. Authentication failures do not include credentials, model keys, or
decrypted data.

Attributes:

- `code: int`
- `debug_log: str`

Known error codes:

| Code | Name |
|------|------|
| `0` | `OK` |
| `1` | `FILE_NOT_FOUND` |
| `2` | `INVALID_FORMAT` |
| `3` | `NOT_LOGGED_IN` |
| `4` | `SESSION_EXPIRED` |
| `5` | `NETWORK` |
| `6` | `ACCESS_DENIED` |
| `7` | `DECRYPT_FAILED` |
| `8` | `ONNX_LOAD` |
| `9` | `DEBUGGER` |
| `10` | `MEMORY` |
| `11` | `INVALID_SALT` |
| `12` | `NOT_ENCRYPTED` |
| `13` | `ENCRYPT_FAILED` |
| `14` | `ENGINE_BUILD` |
| `15` | `RATE_LIMITED` |
| `16` | `INVALID_ARGUMENT` |
| `17` | `INFERENCE` |

## `InferenceSession` Public State

### Writable attributes

| Attribute | Type | Meaning |
|-----------|------|---------|
| `model_loaded` | `bool` | `True` after initial load or `load_model()`, `False` after `unload_model()` |
| `bench_wait_for_results` | `bool/int` | Enables timing capture for `wait_for_results_raw()` |

### Readable benchmark fields

| Attribute | Type | Meaning |
|-----------|------|---------|
| `last_wfr_total_ms` | `int` | Total `wait_for_results_raw()` time |
| `last_wfr_wait_ms` | `int` | Event/blocking time |
| `last_wfr_select_ms` | `int` | Selection/ready-check time |

### Read-only properties

| Property | Type | Meaning |
|----------|------|---------|
| `model_type` | `int` | Current output type: detection, pose, or segmentation |
| `model_info` | `dict` | Current model metadata from `get_model_info(...)`, or `{}` if no model is loaded |
| `loaded_model_description` | `str` | Manifest name/version plus exact loaded task, dimensions, precision, and backend |
| `task` | `str` | Current model task: `"detect"`, `"pose"`, or `"segment"` |
| `is_paused` | `bool` | Pause state reported by the engine |

## `InferenceSession` Lifecycle Calls

| Call | Signature | Returns | Notes |
|------|-----------|---------|-------|
| `start` | `start(ring_buffer_name: str = None, helios_pid: int = None)` | `None` | Starts inference and attaches result shared memory |
| `wait_for_results_raw` | `wait_for_results_raw(timeout_ms: int = -1)` | `tuple | None` | Returns zero-copy shared-memory views for the completed slot for the session's last host-triggered frame sequence; `-1` waits indefinitely, while a non-negative value explicitly opts into a timeout |
| `segmentation_results_raw` | `segmentation_results_raw()` | `tuple | None` | Returns the segmentation companion slot for the last completed frame without another wait |
| `pause` | `pause()` | `None` | Pauses engine work |
| `resume` | `resume()` | `None` | Resumes engine work |
| `stop` | `stop()` | `None` | Stops inference and releases result shared memory handles |
| `destroy` | `destroy()` | `None` | Calls `stop()` and destroys the native engine |
| `load_model` | `load_model(uuid: str)` | `None` | Hot-loads a new model into the same session using the selected Compute GPU |
| `unload_model` | `unload_model()` | `None` | Unloads the current model; inference becomes a no-op |

`pause()` persists across frames. Suspended sessions reject host-frame triggers
before calling InferenceCore and discard any pending request. Since the host
triggers inference before the script's `process()` call, a `pause()` or
`resume()` made during a frame takes effect for triggering on the next
host-selected frame. Waiting during the transition frame returns immediately.
Inference already in flight when `pause()` is called may finish, but it is not
selected as the session's current result.

Successful initial and hot model loads print the manifest name and exact loaded
profile rather than the UUID. For example:

```text
[InferenceCore] Loaded model: Example Segment v2.1 (segmentation, 960x960, FP16, TensorRT)
```

The dimensions come from the loaded engine, not the recommended ROI or filename.
DirectML models report `FP32` because the DirectML object-model contract requires
float32 inputs and outputs.

There is no public Python call to trigger inference for a frame. In the packaged runtime, `cvpython_host.pyd` selects the frame and starts inference before your script's `process()` work runs.

Internally, same-process request, pause/resume, dataset-save, and OCR-worker
handoffs use `WaitOnAddress` on lock-free atomic predicates. Python result waits
still use the named completion event across the host/runtime boundary. This is
an internal synchronization change and does not alter the Python or native ABI.
Shutdown cancellation is provided by a separate internal Python-host export;
it is not part of the public host-context structure or SDK.

Context manager support:

```python
with inference.create_inference_engine(uuid) as engine:
    engine.start()
    result = engine.wait_for_results_raw()
```

## Result Views

`wait_for_results_raw()` waits indefinitely by default so each processing call
stays paired with its host-triggered frame. It returns `None` if the session has
not seen a host-triggered frame sequence, or when an explicit non-negative
`timeout_ms` expires, or when the packaged Python host is shutting down. A backend failure raises `InferenceCoreError` with code
`INFERENCE`; it is not reported as an empty detection frame. The failure remains
attached to that loaded model until the model is unloaded or replaced.

On success it returns:

```python
(
    frame_sequence,
    timestamp_ns,
    inference_time_ms,
    model_type,
    num_detections,
    frame_width,
    frame_height,
    class_ids,
    metrics,
    tracking_raw,
    keypoints,
)
```

Field layout:

- `class_ids`: `np.ndarray[(64,), uint32]`
- `metrics`: `np.ndarray[(64, 14), float32]`
  - columns are `confidence, x1, y1, x2, y2, width, height, center_x, center_y, area, anchor_x, anchor_y, polar_distance, polar_angle`
- `tracking_raw`: `np.ndarray[(64, 16), uint32]`
  - this is the native tracking and prediction block as ABI words
- `keypoints`: `np.ndarray[(64, 17, 3), float32]`

Important:

- This is a zero-copy borrowed view into the current shared-memory result slot.
- Only the first `num_detections` rows are valid for the current frame.
- The arrays are full-capacity slot views; slice them in Python with `[:num_detections]` if needed.
- Do not retain these arrays across later frames, across async work, or after stopping/destroying the session.
- Detection, pose, and segmentation share one global capacity of 64 published
  detections per frame, not 64 per class. Any one class may consume any number
  of those slots after parser retention, confidence gating, NMS, class
  priority, ignore-region, polar-ring, and target-delay filtering.
- Pose returns 17 keypoints inside each valid detection row. Segmentation
  publishes at most 64 companion masks, with mask `i` corresponding to
  detection `i` for each valid mask entry.
- Drawing is independent of publication. Draw settings can hide a returned
  detection, while target delay can temporarily draw an accepted detection and
  publish `num_detections == 0`. ROI, origin, OCR-region, and other diagnostic
  overlays are not inference result objects.

For a segmentation model, call `segmentation_results_raw()` after
`wait_for_results_raw()`. It returns:

```python
(
    frame_sequence,
    format,
    mask_count,
    mask_width,
    mask_height,
    mask_row_bytes,
    mask_plane_stride,
    frame_width,
    frame_height,
    (mask_origin_x, mask_origin_y, mask_scale_x, mask_scale_y),
    masks,
)
```

`masks` is a zero-copy read-only memory view containing `mask_count` consecutive
mask planes. Mask `i` corresponds exactly to detection `i`. Each plane contains
`mask_height` rows of `mask_row_bytes`; bit `x & 7` in byte `x >> 3` represents
mask pixel `x`. `mask_plane_stride` is rounded to a 64-byte boundary while rows
remain tightly packed. Convert a mask-grid coordinate to the source frame with
`frame_x = mask_origin_x + mask_x * mask_scale_x` and the equivalent Y
expression. The view is borrowed from a reusable result slot and must not be
retained across frames.

## Settings API

These calls mutate engine settings on an existing `InferenceSession`.

### Thresholds and selection

| Call | Signature | Accepted shapes | Notes |
|------|-----------|-----------------|-------|
| `set_confidence_threshold` | `set_confidence_threshold(threshold: float, class_ids=None)` | `class_ids=None`, `"all"`, `int`, iterable of `int` | `None` applies globally |
| `clear_class_overrides` | `clear_class_overrides(class_ids=None)` | `class_ids=None`, `"all"`, `int`, iterable of `int` | Clears per-class overrides or all overrides |
| `set_nms_threshold` | `set_nms_threshold(threshold: float)` | exact | NMS threshold |
| `set_segmentation_mask_threshold` | `set_segmentation_mask_threshold(threshold: float)` | `0 < threshold < 1` | Binary mask probability threshold; default `0.5` |
| `set_segmentation_mask_opacity` | `set_segmentation_mask_opacity(opacity: int)` | `0..255` | Overlay opacity; default `128` |
| `set_class_priority` | `set_class_priority(priority_spec)` | `None`, `int`, iterable of `int`, iterable of groups | Group order sets priority tiers |
| `set_class_priority_recommended` | `set_class_priority_recommended()` | exact | Applies the current model's recommended class priority, falling back to `0` |
| `set_use_recommended_class_priority` | `set_use_recommended_class_priority(enabled: bool)` | exact | Enables or disables automatic use of recommended class priority across model loads |
| `set_sort_method` | `set_sort_method(method: int)` | use `SORT_DISTANCE` or `SORT_CONFIDENCE` | Sort mode |

`set_class_priority(priority_spec)` accepted forms:

- `None`: clear class priority
- `5`: single top-priority class
- `[5, 2, 7]`: three priority tiers of one class each
- `[[5, 6], [2], [7, 8]]`: grouped tiers with equal priority inside a group

Class-priority behavior:

- Only the highest-priority tier represented in the parser's retained candidates is published. Lower tiers do not backfill unused detection slots.
- Classes grouped in the winning tier have equal priority and share the global 64-detection capacity.
- `[5, 2, 7]` publishes only class `5` when a retained class `5` candidate exists; otherwise it publishes only class `2`, then only class `7` if neither higher tier is represented.
- The parser retains at most 64 candidates globally by confidence before runtime priority-tier selection. Priority tiers do not reserve capacity during parser selection.
- The winning tier is selected before ignore-region and polar-ring rejection. If those filters remove every candidate in that tier, lower tiers are not reconsidered for that frame.

Confidence behavior:

- Global confidence applies to classes without a per-class override.
- Per-class confidence overrides are applied during parser confidence gating, so a class can use a lower threshold than global without lowering the threshold for every other class.
- The override applies to the model-selected class for a candidate; it does not relabel a candidate to a lower-scoring class.

### Geometry and filtering regions

| Call | Signature | Notes |
|------|-----------|-------|
| `clear_roi` | `clear_roi()` | Disables ROI and uses the full frame |
| `set_roi` | `set_roi(center_x: float, center_y: float, width_pct: float, aspect_w: float = 16.0, aspect_h: float = 9.0)` | Sets ROI using normalized center and width percentage |
| `set_roi_pixels` | `set_roi_pixels(center_x: float, center_y: float, width_px: int, height_px: int)` | Sets ROI using fixed pixel dimensions |
| `set_roi_to_model_size` | `set_roi_to_model_size(center_x: float = 0.5, center_y: float = 0.5)` | Uses the current model input size as a fixed pixel ROI |
| `set_roi_to_recommended_size` | `set_roi_to_recommended_size(center_x: float = 0.5, center_y: float = 0.5)` | Uses the current model's recommended ROI size from manifest metadata |
| `set_use_recommended_roi` | `set_use_recommended_roi(enabled: bool)` | Enables or disables automatic use of recommended ROI size across model loads |
| `set_polar_origin` | `set_polar_origin(pct_x: float = 0.5, pct_y: float = 0.5)` | Sets normalized origin for distance/angle |
| `set_polar_origin_ring` | `set_polar_origin_ring(radius_pct: float, colors_bgr, segment_ms: int = 250, limit_detections: bool = False)` | Optional visual ring and optional max-distance filter around the polar origin |
| `set_limit_detections_to_polar_origin_ring` | `set_limit_detections_to_polar_origin_ring(enabled: bool)` | Enables or disables anchor-point filtering against the current ring radius |
| `set_polar_origin_ring_focus` | `set_polar_origin_ring_focus(enabled: bool)` | Enables or disables focused ring radius after a valid selected target |
| `set_anchor_point` | `set_anchor_point(pct_x: float = 0.5, pct_y: float = 0.5, flat_x: float = 0.0, flat_y: float = 0.0, class_ids=None)` or `set_anchor_point(profiles, class_ids=[...])` | Static anchor point globally/per class, or class-specific button-selected anchor point |
| `set_pose_anchor_point` | `set_pose_anchor_point(mode=None, class_ids=None)` | Pose-only keypoint-derived anchor mode with bbox-anchor fallback |
| `set_anchor_prediction` | `set_anchor_prediction(x_enabled=False, y_enabled=False, response_delay_ms=0.0, lead_x=1.0, lead_y=1.0)` | Robust selected-target motion prediction with direct command-induced camera compensation |
| `record_anchor_prediction_motion` | `record_anchor_prediction_motion(timestamp_ns: int, velocity_x: float, velocity_y: float)` | Records the final command's signed camera velocity in screen-heights per second |
| `set_target_delay_ms` | `set_target_delay_ms(ms: int)` | Global minimum continuous detection age before detections are published |
| `set_ignore_region` | `set_ignore_region(x1: float, y1: float, x2: float, y2: float)` | Ignore region corners |

ROI behavior:

- `set_roi(...)` is normalized-only.
- `set_roi_pixels(...)`, `set_roi_to_model_size(...)`, and `set_roi_to_recommended_size(...)` all use fixed pixel dimensions.
- Fixed pixel ROI sizes are not normalized by frame resolution. A `640x640` ROI stays `640x640` on 1080p, 1440p, and 4K frames.
- `set_roi_to_recommended_size(...)` raises `InferenceCoreError` if the current model has no recommended ROI metadata.
- New sessions default to using recommended ROI and recommended class priority on initial create/load.
- `clear_roi()`, `set_roi(...)`, `set_roi_pixels(...)`, and `set_roi_to_model_size(...)` disable the recommended-ROI preference until you call `set_use_recommended_roi(True)` or `set_roi_to_recommended_size(...)`.
- `set_class_priority(...)` disables the recommended-class-priority preference until you call `set_use_recommended_class_priority(True)` or `set_class_priority_recommended()`.
- `set_use_recommended_roi(True)` keeps the preference enabled for future model loads and applies the current model's recommended ROI immediately when metadata exists.
- `set_use_recommended_class_priority(True)` keeps the preference enabled for future model loads and applies the current model's recommended class priority immediately. If the model has no class-priority metadata, it falls back to `0`.

Anchor prediction behavior:

- Positions and velocities are normalized by frame height internally. The
  predictor therefore produces the same geometry at 720p, 1080p, 1440p, and
  4K.
- All transitions use measured monotonic timestamps in seconds. There is no
  assumed inference FPS.
- Warmup is based on elapsed observation time rather than frame count. Recent
  statistically significant motion also prevents a transient filter reversal
  from leading in the opposite direction.
- During acquisition, a rolling 24 ms test detects camera motion that is
  aligned with the anchor error and still pulling that error toward the
  crosshair. Velocity state is reinitialized during this phase instead of
  learning the pull as target movement.
- Projected target motion passes through an 18 ms continuous-time exponential
  smoother. This adds bounded temporal smoothing without introducing an
  inference-frame-rate dependency.
- Filtered target position uses a separate 12 ms continuous-time smoother
  before the prediction offset is applied.
- Detection size/confidence and camera-response uncertainty contribute to
  measurement covariance, reducing lead from box jitter and imperfect engine
  response models.
- The projection horizon is the measured age of the captured frame plus
  `response_delay_ms`.
- `response_delay_ms` aligns the recorded final-command camera velocity with
  the motion visible in captured frames.
- Helios integrates command-induced camera motion over each observation
  interval and adds it back to the raw anchor displacement before estimating
  natural target motion. Initial aim toward a stationary target therefore does
  not become target lead.
- The predicted offset is target-only. The active aim engine predicts its own
  virtual-crosshair motion separately, so Helios does not subtract or project
  future aim motion a second time.
- Record final-command motion continuously, including zero samples, from one
  producer. `timestamp_ns` must use the same steady monotonic clock domain as
  frame and result timestamps. Velocities are signed screen-heights per second
  after aim, recoil, and manual-input composition.

```python
session.set_anchor_prediction(
    x_enabled=True,
    y_enabled=False,
    response_delay_ms=58.0,
    lead_x=1.0,
    lead_y=1.0,
)

session.record_anchor_prediction_motion(
    timestamp_ns,
    camera_velocity_x,
    camera_velocity_y,
)
```

`set_polar_origin_ring(...)` accepted `colors_bgr` forms:

- empty or falsy with `radius_pct > 0`: draws the ring in black (default fallback)
- empty or falsy with `radius_pct <= 0`: disables the ring radius
- iterable of packed integers
- iterable of `(b, g, r)` tuples

Behavior details:

- Maximum of 32 colors are consumed.
- `segment_ms` is clamped to at least `1`.
- Packed integer color layout is the low-byte BGR layout used by the binding: `b | (g << 8) | (r << 16)`.
- `radius_pct` is normalized `0..1` against the distance from the polar origin to the farthest active ROI corner, or the farthest frame corner when ROI is disabled.
- If `limit_detections=True` or `set_limit_detections_to_polar_origin_ring(True)` is active, detections whose resolved anchor point falls outside the ring radius are dropped before they are returned or drawn.
- `set_polar_origin_ring_focus(True)` narrows the active ring radius toward the smallest radius that contains the valid selected target's full bounding box. It does not focus on delayed or otherwise unpublished candidates.
- This polar ring filter is separate from ROI. ROI changes the inference crop; the ring filter only rejects already-resolved detections by anchor distance.
- The drawn polar-origin cross and drawn anchor marks are also resolution-aware now; they scale from the frame size instead of using fixed pixel sizes.
- `set_target_delay_ms(0)` disables delayed target validation. Non-zero values publish `num_detections=0` until the global continuous detection age reaches the threshold.
- During the delay window, accepted candidates may still be drawn with the normal secondary-target overlay style; they are not published as valid detections until the delay passes.
- The delay timer is not class-specific. It carries across class swaps and resets after one second without accepted detections.

`set_pose_anchor_point(mode=None, class_ids=None)` accepted modes:

- `None` or `"none"`: disable pose-keypoint anchor override
- `"head"` or `POSE_ANCHOR_HEAD`: use the center of the available head keypoints (`nose`, eyes, ears), with X averaged against shoulder center when both shoulders are available
- `"chest"` or `POSE_ANCHOR_CHEST`: use the shoulder center shifted 15% toward the hip center when hips are available
- `"abdomen"` or `POSE_ANCHOR_ABDOMEN`: use the hip center shifted 20% toward the shoulder center when shoulders are available

Behavior details:

- This only affects pose models.
- The pose anchor is tried before the normal bbox anchor.
- Head keeps its head-derived Y coordinate; shoulders only bias the X coordinate.
- Chest and abdomen fall back to their source pair center when the opposite torso pair is missing.
- If the requested pose anchor cannot be resolved from available keypoints, the runtime falls back to the existing bbox anchor for that class.

`set_anchor_point(profiles, class_ids=[...])` accepted button-profile form:

```python
# Use this anchor for class 0 while BUTTON_8 / LT / L2 is pressed.
engine.set_anchor_point([(7, (0.5, 0.32))], class_ids=[0])
```

Behavior details:

- The anchor tuple is `(pct_x, pct_y)` or `(pct_x, pct_y, flat_x, flat_y)`.
- Each profile is `(button_index, anchor_tuple)`.
- Button indices are `0..20`; pressed means the button value is greater than `25.0`.
- Up to 8 profiles are consumed, matching the search-clause limit.
- Profiles are evaluated in list order. The first pressed button wins.
- Button profiles require at least one explicit class id.
- An empty profile list clears button-selected anchors for the selected class scope and returns that class to its static anchor.
- Static numeric calls still work. A static class call clears button profiles for the selected class scope.
- Class-specific button profiles override that class's static anchor while matched. If no profile matches, the class static anchor is used. If neither exists, the global static anchor is used.

### OCR controls

OCR is a separate lower-priority result path. It does not change `wait_for_results_raw()` or its detection views.

| Call | Signature | Notes |
|------|-----------|-------|
| `set_ocr_enabled` | `set_ocr_enabled(enabled: bool)` | Enables or disables OCR scheduling |
| `set_ocr_skip_when_detections` | `set_ocr_skip_when_detections(enabled: bool)` | Default is enabled; skips OCR for frames with published detections |
| `set_draw_ocr_region` | `set_draw_ocr_region(enabled: bool)` | Enables or disables drawing configured OCR region corners |
| `clear_ocr_regions` | `clear_ocr_regions()` | Removes all configured OCR regions |
| `set_ocr_region_pixels` | `set_ocr_region_pixels(region_id: int, x: int, y: int, width: int, height: int)` | Configures a pixel-space region |
| `set_ocr_region_normalized` | `set_ocr_region_normalized(region_id: int, x1: float, y1: float, x2: float, y2: float)` | Configures a normalized corner region |
| `set_ocr_region_enabled` | `set_ocr_region_enabled(region_id: int, enabled: bool)` | Enables or disables one configured region |
| `remove_ocr_region` | `remove_ocr_region(region_id: int)` | Removes one region |
| `get_ocr_result` | `get_ocr_result(region_id: int) -> dict or None` | Returns the latest OCR result for that region, if one exists |
| `set_color_ocr_region` | `set_color_ocr_region(b: int, g: int, r: int, region_id: int = None)` | Sets the global OCR region color or one configured region override |

OCR defaults:

| Setting | Default |
|---|---|
| OCR scheduling | disabled |
| Skip OCR when object detections exist | enabled |
| Configured regions | none |
| OCR region drawing | enabled |
| OCR region color | `#c059df` for every region ID, passed as BGR `(223, 89, 192)` |
| Per-region color | none; only changes when explicitly set by `region_id` |
| New pixel/normalized region | enabled immediately, result status starts as `OCR_STATUS_EMPTY` |
| Region scheduling order | round-robin across enabled regions, starting at the first configured slot |
| OCR complete event | nonsignaled until OCR work publishes a result |
| Latest result for an unconfigured or empty region | unavailable; `get_ocr_result(...)` returns `None` |
| Disabled region result | `OCR_STATUS_DISABLED` |

OCR behavior:

- The recognizer is PP-OCRv6 small ONNX running through ONNX Runtime DirectML. There is no alternate OCR backend, full-frame CPU OCR path, or CPU execution-provider fallback.
- The OCR runtime binds the recognizer to the fixed input shape `[1, 3, 48, 320]`; the full graph must assign to DirectML.
- OCR is disabled by default.
- OCR requires caller-defined regions; it does not scan the full frame.
- The OCR DirectML session is created only when OCR is enabled and at least one enabled region exists. Scripts that do not configure OCR do not initialize OCR.
- For OCR-only scripts, create an empty session with `create_inference_engine()`, configure at least one OCR region, enable OCR, then call `start()`.
- OCR processes at most one enabled region per scheduled frame, round-robin.
- OCR work is latest-only. If the worker is still busy, older pending OCR frames are dropped.
- If `set_ocr_skip_when_detections(True)` is active, frames with object detections do not schedule OCR.
- OCR prepares only the selected region from the clean frame before overlay drawing, then the OCR worker runs the recognizer from that prepared tensor. It does not copy the full frame.
- OCR results are separate from detection results.
- OCR region corners draw on processed frames for every configured region while `set_draw_ocr_region(True)` is active, even when OCR scheduling is disabled. When fuser output is enabled, the same OCR region corners are emitted to the fuser command buffer in the inference thread.
- In-frame OCR corner pixels are drawn just outside the configured OCR region so the overlay does not write into the pixels sampled by OCR.
- OCR corner lengths are derived from each region's width and height so opposing corner segments keep a gap on small regions.

`get_ocr_result(...)` returns:

| Key | Type | Meaning |
|-----|------|---------|
| `frame_sequence` | `int` | Frame sequence associated with the OCR result |
| `timestamp_ns` | `int` | Frame timestamp |
| `region_id` | `int` | Caller-owned OCR region id |
| `status` | `int` | One of the `OCR_STATUS_*` constants |
| `confidence` | `float` | Recognizer confidence |
| `text` | `str` | UTF-8 decoded OCR text |
| `text_bytes` | `int` | Original UTF-8 byte count before Python decoding |
| `frame_width`, `frame_height` | `int` | Source frame dimensions |
| `x`, `y`, `width`, `height` | `int` | Pixel-space region used for OCR |
| `flags` | `int` | Bitmask; `OCR_FLAG_TRUNCATED` means the fixed ABI text buffer truncated the result |

```python
engine = inference.create_inference_engine()
engine.set_ocr_region_pixels(100, 80, 40, 360, 80)
engine.set_color_ocr_region(223, 89, 192)
engine.set_ocr_skip_when_detections(True)
engine.set_ocr_enabled(True)
engine.start()

result = engine.get_ocr_result(100)
if result and result["status"] == inference.OCR_STATUS_READY:
    text = result["text"]
```

### Visualization toggles

| Call | Signature |
|------|-----------|
| `set_draw_detections` | `set_draw_detections(enabled: bool, class_ids=None)` |
| `set_draw_segmentation_masks` | `set_draw_segmentation_masks(enabled: bool, class_ids=None)` |
| `set_bbox_thickness` | `set_bbox_thickness(thickness: int)` |
| `set_draw_ocr_region` | `set_draw_ocr_region(enabled: bool)` |
| `set_draw_origin_cross` | `set_draw_origin_cross(enabled: bool)` |
| `set_draw_anchor_point` | `set_draw_anchor_point(enabled: bool)` |
| `set_draw_roi` | `set_draw_roi(enabled: bool)` |
| `set_draw_benchmarks` | `set_draw_benchmarks(enabled: bool)` |
| `set_draw_confidence` | `set_draw_confidence(enabled: bool)` |
| `set_draw_ignore_region` | `set_draw_ignore_region(enabled: bool)` |
| `set_draw_origin_line` | `set_draw_origin_line(enabled: bool)` |
| `set_draw_zoomed_roi` | `set_draw_zoomed_roi(enabled: bool)` |
| `set_draw_keypoints` | `set_draw_keypoints(enabled: bool, class_ids=None)` |
| `set_draw_skeleton` | `set_draw_skeleton(enabled: bool, class_ids=None)` |

Drawing behavior:

- `set_draw_detections(...)` controls bounding-box drawing only.
- Segmentation masks draw by default. `set_draw_segmentation_masks(...)` controls
  the mask overlay globally or for selected classes without changing the raw
  mask result.
- `set_draw_confidence(...)` draws one quantized confidence digit (`0..9`) centered above each detection bbox.
- Confidence overlay defaults to disabled.
- `set_bbox_thickness(...)` is clamped to `1` or `2`; default is `2`.
- `set_draw_roi(...)`, `set_draw_zoomed_roi(...)`, and `set_draw_ocr_region(...)` draw corner markers, not full rectangles.
- Corner marker horizontal and vertical segments are derived from each region's width and height and capped below half the side length, so opposing segments do not touch on small regions.
- `set_draw_anchor_point(...)` draws the normal anchor. When input lead has a valid estimate, the selected target gets a predicted-anchor marker in the configured anchor color and the raw selected anchor is shown in secondary gray.
- `set_draw_keypoints(...)` and `set_draw_skeleton(...)` are evaluated independently for pose models.
- Pose keypoints and skeleton can render without bounding boxes.
- `set_draw_origin_line(...)` now uses a 1px non-AA line for lower overhead.
- For the three draw calls above, `class_ids` accepts `None`, a single `int`, or an iterable of `int`.
- To draw only selected classes, disable the feature globally, then enable it for specific classes.
- `clear_class_overrides(...)` also clears per-class draw overrides for bbox, keypoints, and skeleton.

Examples:

```python
# Draw keypoints without pose bounding boxes
engine.set_draw_detections(False)
engine.set_draw_keypoints(True)
engine.set_draw_skeleton(False)

# Draw bounding boxes only for classes 0 and 2
engine.set_draw_detections(False)
engine.set_draw_detections(True, class_ids=[0, 2])

# Draw pose keypoints only for class 0
engine.set_draw_keypoints(False)
engine.set_draw_keypoints(True, class_ids=[0])
```

### Color setters

| Call | Signature | Color order seen by Python caller |
|------|-----------|-----------------------------------|
| `set_color_bbox` | `set_color_bbox(b: int, g: int, r: int, class_ids=None)` | `B, G, R` |
| `set_color_segmentation_mask` | `set_color_segmentation_mask(b: int, g: int, r: int, class_ids=None)` | `B, G, R` |
| `set_color_origin` | `set_color_origin(b: int, g: int, r: int)` | `B, G, R` |
| `set_color_anchor` | `set_color_anchor(b: int, g: int, r: int, class_ids=None)` | `B, G, R` |
| `set_color_roi` | `set_color_roi(b: int, g: int, r: int)` | `B, G, R` |
| `set_color_ignore_region` | `set_color_ignore_region(b: int, g: int, r: int)` | `B, G, R` |
| `set_color_origin_line` | `set_color_origin_line(b: int, g: int, r: int)` | `B, G, R` |
| `set_color_zoomed_roi` | `set_color_zoomed_roi(b: int, g: int, r: int)` | `B, G, R` |
| `set_color_ocr_region` | `set_color_ocr_region(b: int, g: int, r: int, region_id: int = None)` | `B, G, R` |
| `set_color_keypoints` | `set_color_keypoints(b: int, g: int, r: int, class_ids=None)` | `B, G, R` |
| `set_color_skeleton` | `set_color_skeleton(b: int, g: int, r: int, class_ids=None)` | `B, G, R` |

Important:

- `set_color_keypoints(...)` and `set_color_skeleton(...)` now use the same `B, G, R` argument order as the other color setters.
- The color setters in this binding now consistently take `B, G, R`.
- For `set_color_keypoints(...)` and `set_color_skeleton(...)`, `class_ids` accepts `None`, a single `int`, or an iterable of `int`.

### Zoom and search controls

| Call | Signature | Notes |
|------|-----------|-------|
| `set_zoom_enabled` | `set_zoom_enabled(enabled: bool)` | Master zoom toggle |
| `set_zoom_button_enabled` | `set_zoom_button_enabled(enabled: bool)` | Enables button-driven zoom |
| `set_zoom_step` | `set_zoom_step(step_pct: float)` | Zoom step percent |
| `set_zoom_max` | `set_zoom_max(max_pct: float)` | Max zoom percent |
| `set_zoom_hold_ms` | `set_zoom_hold_ms(ms: int)` | Hold duration |
| `set_zoom_buttons` | `set_zoom_buttons(button_indices)` | Iterable of button indices `0..20` |
| `set_search_buttons` | `set_search_buttons(button_indices)` | Three accepted shapes, documented below |

`set_zoom_buttons(button_indices)`:

- Consumes button indices `0..20`
- Builds a bitmask
- Out-of-range indices are ignored

`set_search_buttons(button_indices)` accepted forms:

1. Simple pressed-mask form:

```python
engine.set_search_buttons([0, 1, 5])
```

2. Flat condition form:

```python
engine.set_search_buttons([(0, 1), (1, 0)])
```

3. Clause form, up to 8 clauses:

```python
engine.set_search_buttons([
    [(0, 1), (1, 1)],
    [(2, 1)]
])
```

Interpretation from binding code:

- Button indices must be in `0..20`
- Runtime button values greater than `25.0` are considered pressed; values at or below `25.0` are considered released
- In condition and clause forms, state value `0` means "released"
- In condition and clause forms, state value `1` means "pressed"
- Other state values raise `ValueError`
- Empty input clears search activation buttons and returns to always-search behavior
- Any non-empty input enables button-gated search activation automatically

### Pose-specific controls

| Call | Signature | Notes |
|------|-----------|-------|
| `set_keypoint_conf_threshold` | `set_keypoint_conf_threshold(threshold: float)` | Keypoint confidence gate |
| `set_keypoint_mask` | `set_keypoint_mask(keypoints=None)` | `None` enables all 17 keypoints |
| `set_keypoint_radius` | `set_keypoint_radius(radius: int)` | Draw radius |

`set_keypoint_mask(...)` already gives per-keypoint control inside the enabled pose overlay path.

`set_keypoint_mask(keypoints=None)` behavior:

- `None` sets mask to `0x1FFFF`, enabling keypoints `0..16`
- Iterable entries outside `0..16` are ignored

## Calls That Exist Natively But Are Not Exposed Here

These exist in `InferenceCore.h` but are not wrapped as public Python functions in the Python module:

- `infcore_shutdown`
- `infcore_has_tensorrt`
- `infcore_list_compute_adapters`
- `infcore_set_compute_adapter`
- `infcore_get_compute_adapter`

If an external project needs these from Python, the binding has to be extended first.

## Minimal Settings-Only Example

```python
from helios import inference

engine = inference.create_inference_engine(model_uuid)

engine.set_confidence_threshold(0.55)
engine.set_confidence_threshold(0.75, class_ids=[0, 2])
engine.set_nms_threshold(0.45)
engine.set_class_priority([[0], [2, 3], [5]])
engine.set_roi_to_recommended_size()
engine.set_anchor_point(0.5, 0.85)
engine.set_polar_origin(0.5, 0.5)
engine.set_polar_origin_ring(0.20, None, limit_detections=True)
engine.set_ignore_region(0.0, 0.0, 0.1, 0.1)
engine.set_sort_method(inference.SORT_DISTANCE)
engine.set_ocr_region_pixels(100, 80, 40, 360, 80)
engine.set_color_ocr_region(223, 89, 192)
engine.set_ocr_skip_when_detections(True)
engine.set_ocr_enabled(True)

engine.set_draw_detections(True)
engine.set_color_bbox(0, 255, 0)
engine.set_draw_origin_cross(True)
engine.set_color_origin(0, 0, 255)
```

## Host Requirement

This binding is supported only inside the packaged CV Python host. Third-party
Python scripts use `from helios import inference` from that host; they do not
copy or initialize `inference_core.pyd` themselves. The host supplies the
authorized runtime identity, current frame sequence, and inference trigger
before user `process()` runs.

# InferenceCore Native Scripting API

This document is the exact native scripting contract for direct C++ access to the selected `InferenceCoreTrt10.dll` or `InferenceCoreTrt11.dll` from:

- CV Python hybrid scripts: Python main script + native helper DLL
- pure CV C++ scripts running under `CVCppWrapper.exe`

It is written for AI agents and external script projects that need exact call order and ABI expectations.

## Scope

- Python `helios.inference` is the canonical host-authoritative, wait-only scripting path and is implemented by the native `inference_core` module.
- This document covers the parallel native path added for C++ callers.
- The native path uses the same result shared memory, event signaling, and per-frame sequencing that the Python raw path already uses.

The installed public contract is under
`versions/<version>/sdk/include/helios/`:

- `InferenceCore.h`
- `InferenceCoreApi.h`
- `HeliosInferenceResultsABI.h`
- `HeliosInferenceSegmentationResultsABI.h`
- `HeliosInferenceOcrABI.h`
- `HeliosHostFunctions.h`
- `HeliosInferenceHostTriggerABI.h`
- `HeliosInferenceSDK.hpp`

The native hosts and runtime implementations remain private Helios source; they
are not part of the distributed SDK.

## Current Runtime Layout

This native scripting path assumes the current packaged client layout only:

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
- `versions/<version>/lib/cv_cpp/CVCppWrapper.exe`
- `versions/<version>/lib/py/` for Python-side `.pyd` modules

Current host expectations:

- CV Python hybrid native helpers run inside the selected `python.exe`, with `cvpython_host.pyd` owning the native frame loop
- pure CV C++ script DLLs run inside `CVCppWrapper.exe`
- both hosts resolve the TensorRT-major-specific InferenceCore DLL from the packaged `lib/` directory
- both InferenceCore loaders resolve and validate every export they use
- host state is exposed as independent named functions, not a table
- CV C++ scripts expose independent `create`, `process`, and `destroy` functions
- a missing required function is rejected explicitly
- the SDK requires `helios_host_get_root_path` and does not infer the package root from the current process path

### ABI stability policy

The public ABI and SDK are unversioned. Do not add version constants, version
fields, version exports, generation identifiers, or version gates. Existing
exports, function signatures, calling conventions, field meanings, and
published shared-memory strides remain stable. In particular,
`HeliosInferenceOcrResult` retains its 360-byte stride so deployed native
scripts remain binary-compatible.

Every callable boundary is an independent named C export. There are no exchanged
function tables and no whole-interface size checks. Native functionality is
additive: add another named export and leave every existing export unchanged.
Existing callers ignore functions they do not know. A caller that needs another
function resolves that exact name and fails explicitly only when it is absent.
Adding a function remains the same ABI. Python scripts do not compile against
the C ABI.

Published data structures keep fixed sizes and field offsets. If new data cannot
fit an existing reserved field, add a new function with a new structure instead
of changing an existing structure.

## What Exists Now

The runtime now has three supported access patterns:

1. Python-only
   - Use `from helios import inference` from Python.
2. CV Python hybrid
   - The selected Python process runs as usual.
   - A native helper DLL resolves the host functions it needs from `cvpython_host.pyd`.
   - The helper talks directly to the selected TensorRT-major-specific InferenceCore DLL.
3. Pure CV C++
   - A native script DLL resolves the host functions it needs from `CVCppWrapper.exe`.
   - The script DLL talks directly to the selected TensorRT-major-specific InferenceCore DLL.

The object-inference result ABI size and field offsets are unchanged for both runtime backends:

- frame data still comes from the shared video ring buffer
- inference is still triggered by frame sequence
- completion is still event-driven
- raw results are still read from the same result slot mapping
- backend selection follows the Helios Compute GPU preference; `auto` is the default and selects NVIDIA/TensorRT first, then AMD/DirectML, then another DirectML GPU
- a missing compatible Auto GPU or unavailable explicit GPU is a hard error
- NVIDIA selections use TensorRT engine caches (`.ecache` / `.eecache`)
- DirectML selections use ONNX source weights (`.onnx` / `.ennx`)
- Every loadable model manifest entry must contain exactly `detect`, `pose`,
  or `segment` as its task. Missing, differently-cased, and unsupported values
  are hard model-load errors.
- the TensorRT path registers the mapped video ring memory with CUDA and uploads frames directly from that shared BGR memory. It still requires page-locked output buffers. If CUDA host-memory registration/allocation fails, inference start/model load fails instead of falling back to a staged upload path.
- the DirectML path preprocesses the selected frame or ROI into a reused RGB CHW input tensor, then runs the ONNX model through ONNX Runtime DirectML. Object models must assign fully to DirectML except for the single standard ONNX `Mod` node used by supported YOLO end-to-end output graphs; that node may execute through the CPU provider because DirectML does not support its `int64` tensor type. Any other CPU assignment fails model loading.
- OCR is a separate DirectML/ONNX Runtime sidecar. It has its own region table, worker thread, shared result block, and completion event.
- OCR does not write text into the object detection result array.
- Supported modes are independent: scripts may use no session, object inference only, OCR only, or object inference plus OCR. Object model loading and OCR startup fail independently when the requested GPU path cannot initialize.

## Session and Concurrency Model

- One `Session` owns at most one loaded object model at a time. `loadModel(...)`
  replaces that model; it does not append another model to the same engine.
- A script may create and start multiple sessions. The host registers every
  active session and triggers each one for the same selected frame sequence, so
  multiple sessions are the supported way to run multiple object models.
- Each additional session owns its own inference worker, result mappings,
  completion event, model resources, and optional OCR runtime. This is real
  additional GPU/CPU work, not model batching, and can increase latency and
  jitter when the selected GPU is saturated.
- One session supports up to 16 configured OCR regions. Its single lower-priority
  OCR worker processes at most one enabled region per scheduled frame in
  round-robin order; regions are not run in parallel within that session.
- InferenceCore does not load or schedule scripts. The CV Python/C++ host owns
  script execution and registers the inference sessions created by that active
  script. There is no global one-session restriction inside InferenceCore.

## Agent Rules

1. Resolve required host functions from the current process.
2. Create an inference session through `Helios::Inference::Session`.
3. Call `start()` before waiting for results.
4. In packaged hosts, inference for frame `S` is triggered by the host before user `process()` runs.
5. Use `waitForResultsRaw()` near the end of that same frame to collect the matching result.
6. Do not let script code own inference cadence. The host is authoritative for frame selection and inference start.
7. Treat raw result pointers as transient views.
8. Destroy the session when finished.
9. For protected models, rely on the host-provided `script_type`, `script_hash`, and `execution_grant`. Helios never exposes the account session token to scripts.
10. Recommended ROI size and recommended class priority are enabled by default on a new session and after model loads until you switch to another mode.
11. If model metadata does not provide a recommended ROI, runtime fallback is `640x640`.
12. If model metadata does not provide a recommended class priority, runtime fallback is class `0`.
13. Treat OCR as a lower-priority side result. Configure explicit OCR regions, enable OCR, and read OCR results by `region_id`.
14. Do not expect OCR to process the whole frame. The current OCR path is recognizer-only over caller-defined regions.
15. A model discovery/load failure is control-path state, not a per-frame event. Report each distinct failure once and suppress identical repeats until the requested model, error, or refresh state changes.

## Host Functions

Both hosts export these independent symbols:

```c
uint64_t helios_host_get_current_frame_sequence(void);
uint64_t helios_host_get_current_frame_timestamp_ns(void);
uint32_t helios_host_get_process_id(void);
const char* helios_host_get_video_ring_buffer_name(void);
const char* helios_host_get_script_type(void);
const char* helios_host_get_script_hash(void);
const char* helios_host_get_execution_grant(void);
const char* helios_host_get_root_path(void);
```

Declared by the installed public header:

- `versions/<version>/sdk/include/helios/HeliosHostFunctions.h`

Current host values:

- CV Python host returns `script_type = "py"`
- CV C++ wrapper returns `script_type = "dll"`

## Native SDK Entry Point

Use:

```cpp
#include "HeliosCVSDK.h"
```

That header now includes:

- the existing CV C++ SDK
- `HeliosInferenceSDK.hpp`

The complete release-matched developer kit is installed under
`versions/<version>/sdk/` and described in
`THIRD_PARTY_SCRIPT_SDK.md`. The wrapper is header-only and does not require an
InferenceCore import library.

`InferenceCoreApi.h` is the single function manifest used by the DLL header,
the C++ SDK loader, the Python binding, and the release export verifier.
Consumers resolve each function they require and fail clearly when that export
is absent.

Main native type:

```cpp
Helios::Inference::Session
```

This wrapper loads the matching `InferenceCoreTrt10.dll` or `InferenceCoreTrt11.dll`, pulls auth/runtime context from the host export, and attaches to the raw result mapping and completion event.

Script-facing behavior:

- packaged hosts trigger inference for the selected frame
- script code waits for the matching result
- manual frame triggering is an internal SDK detail, not part of the intended script contract

Current packaged-host requirement:

- `HeliosApp.exe` runs from `lib/`
- `CVCppWrapper.exe` runs from `lib/cv_cpp/`
- `cvpython_host.pyd` runs from `lib/py/` inside the selected Python process
- the selected TensorRT-major-specific InferenceCore DLL is loaded from `lib/`

For the absolute lowest overhead:

- prefer `waitForResultsRaw()`
- parse the returned slot directly in C++
- only convert to your own compact struct for Python if Python actually needs the data

## Canonical Native Call Order

```cpp
#include "HeliosCVSDK.h"

using Helios::Inference::Session;

Session engine = Session::create(modelUuid);
if (!engine.valid()) {
    return;
}

// Recommended ROI size and recommended class priority are enabled by default.
engine.start(); // defaults to host ring buffer + host helios pid

// OCR-only scripts can create an empty session with Session::create(),
// configure OCR regions, enable OCR, and then start the session.

engine.setConfidenceThreshold(0.50f);
engine.setNmsThreshold(0.45f);
engine.setRoi(0.5f, 0.5f, 1.0f, 16.0f, 9.0f);
engine.setSortMethod(INFCORE_SORT_DISTANCE);

for (;;) {
    const HeliosInferenceResultsBlock* raw = engine.waitForResultsRaw();
    if (!raw) {
        continue;
    }

    // Consume raw->detections / raw->num_detections here.
}

engine.destroy();
```

## Frame Sync Semantics

The native path is frame-synchronized.

In the packaged hosts (`CVCppWrapper.exe` and `cvpython_host.pyd`), the flow for frame `S` is:

1. Host selects frame sequence `S`.
2. Host publishes `current_frame_sequence = S`.
3. Host triggers all registered native inference sessions for exact sequence `S`.
4. User `process()` runs on frame `S`.
5. `waitForResultsRaw(timeoutMs)` waits on the inference-complete event. The
   default is `INFINITE`, so the host cannot advance this session to another
   frame before the matching result is published. Passing an explicit finite
   timeout opts into dropping that result if the deadline expires.
6. It selects the slot at `S % HELIOS_INFERENCE_RESULT_SLOT_COUNT`.
7. It verifies `slot->write_sequence == S`.
8. It returns the raw slot only if the sequence matches.

Host frame-trigger registration is required for this packaged scripting path.

`waitForResultsRaw()` does not trigger inference on demand. It only waits for the result associated with the host-published frame sequence. If host trigger registration is unavailable, session startup fails instead of silently falling back to a different timing path.

That means:

- results are tied to a specific frame sequence
- callers do not get an arbitrary "latest result"
- the packaged native path overlaps script work with inference for the same frame
- the native path waits for the matching frame result
- no extra copy is required on the raw path

`pause()` persists across frames. While paused, the registered host callback
rejects frame triggers before calling InferenceCore, and any pending request is
discarded. Because the host trigger runs before user `process()`, calling
`pause()` or `resume()` during a frame changes trigger behavior beginning with
the next host-selected frame. A wait after either transition returns immediately
until that next frame has been triggered. Work that was already in flight when
`pause()` was called may finish, but it is not consumed as the session's current
result.

The request, pause/resume, dataset-save, and OCR-worker handoffs are
same-process implementation details and use `WaitOnAddress` on lock-free atomic
predicates. Script-facing completion remains a named event because the script
SDK and raw result contract publish that event by name. This signaling change
does not alter the public ABI or script call order.

Python-host shutdown cancellation uses a separate internal host export and is
not part of the public SDK.

For hybrid use inside the selected Python process, `HeliosInferenceSDK.hpp`
resolves host functions from `cvpython_host.pyd` and participates in the same
pre-trigger flow as the Python binding.

Hybrid frame buffers are borrowed host views, not helper-owned copies.

- `CVWorker.process(frame)` receives a NumPy view over the host frame memory.
- Native helper wrappers should pass `frame.ctypes.data`, `frame.shape[1]`, `frame.shape[0]`, and `frame.strides[0]` directly to the helper DLL.
- Do not call `np.ascontiguousarray(frame)` on the hot path just to satisfy a packed-row helper ABI.
- Native helpers must honor the published row stride when wrapping the frame in NumPy or OpenCV.
- Drawing on that borrowed frame updates the displayed host frame directly.

## Result ABI

Defined in:

- `src/core/crypto/HeliosInferenceResultsABI.h`

Top-level types:

- `HeliosInferenceResultsArray`
- `HeliosInferenceResultsBlock`
- `HeliosInferenceDetection`
- `HeliosInferenceKeypoint`

Segmentation models retain the detection result ABI above and publish masks
through the companion `HeliosInferenceSegmentationResultsArray`. Call
`Session::segmentationResultsRaw()` after `waitForResultsRaw()`; mask `i`
corresponds to detection `i` for the same frame sequence.

Masks use `HELIOS_INFERENCE_SEGMENTATION_FORMAT_BITMASK_LSB`. Each plane has
`mask_height` rows of `mask_row_bytes`, and bit `x & 7` of byte `x >> 3`
represents pixel `x`. Mask planes begin on 64-byte boundaries; rows stay tightly
packed. `mask_origin_*` and `mask_scale_*` map mask-grid
coordinates back to the source frame. The mapping is borrowed, fixed-capacity,
and published before the normal inference completion sequence.

Mask overlays are enabled by default with opacity `128`. The global and
per-class draw/color setters affect visualization only; the packed raw masks
are always published for segmentation results.

Raw block fields:

- `write_sequence`
- `frame_sequence`
- `timestamp_ns`
- `num_detections`
- `model_type`
- `inference_time_ms`
- `frame_width`
- `frame_height`
- `_reserved[HELIOS_INFERENCE_RESULT_RUNTIME_ERROR_INDEX]`
- `detections[64]`

Capacity and drawing contract:

- Detection, pose, and segmentation sessions share one global capacity of 64
  published detections per frame. There is no separate 64-object allowance per
  class and no per-class quota.
- A class may occupy any number of those slots, subject to parser retention,
  confidence gating, NMS, class priority, ignore-region, polar-ring, and target
  delay rules.
- Pose uses the same detection slots and adds 17 keypoints to each published
  detection. Segmentation publishes at most 64 companion masks; mask `i`
  corresponds to detection `i` for each published mask entry.
- `num_detections` is authoritative. Only entries
  `[0, num_detections)` are valid for the frame.
- Drawing is not the result contract. A returned detection can be hidden by
  global or per-class draw settings, and target delay can temporarily draw an
  accepted detection while publishing `num_detections = 0`. ROI, origin,
  ignore-region, OCR-region, and benchmark overlays are also not returned as
  detected objects.

Per-detection fields:

- `class_id`
- `confidence`
- `x1`, `y1`, `x2`, `y2`
- `width`, `height`
- `center_x`, `center_y`
- `area`
- `anchor_x`, `anchor_y`
- `polar_distance`, `polar_angle`
- `predicted_anchor_x`, `predicted_anchor_y`
- `prediction_offset_x`, `prediction_offset_y`
- `prediction_input_x`, `prediction_input_y`
- `prediction_target_motion_x`, `prediction_target_motion_y`
- `continuous_detection_age_ms`
- `prediction_quality`
- `prediction_flags`
- `keypoints[17]`

Notes:

- `model_type` uses the same values as `InferenceCore.h`
- the runtime-error reserved word is `INFCORE_OK` for a normal frame. A nonzero
  value identifies a hard asynchronous backend failure; call
  `infcore_get_runtime_error()` for its text. `Session::waitForResultsRaw()`
  performs this check and stores the text in `Library::lastErrorString()`
  before returning `nullptr`.
- keypoints are populated for pose models
- anchor prediction is disabled by default and configured atomically with
  `InfcoreAnchorPredictionConfig`
- coordinates are normalized by frame height and dynamics use measured seconds,
  making the estimator resolution- and frame-rate-independent
- the predictor uses a robust continuous-time stationary/cruise/maneuver IMM,
  confidence/box-derived measurement covariance, target-switch innovation
  gating, covariance-aware projection, and time-based warmup
- statistically confirmed recent motion prevents a transient filter reversal
  from projecting the anchor in the opposite direction
- a rolling 24 ms acquisition test detects when command-induced camera motion
  is aligned with the anchor error and is still contracting that error; Helios
  reinitializes velocity state during this pull instead of learning it as
  target motion
- projected target motion uses an 18 ms continuous-time exponential smoother,
  so its behavior is frame-rate-independent and detector jitter cannot produce
  instantaneous lead reversals
- the filtered target position uses a separate 12 ms continuous-time smoother
  before the prediction offset is applied
- measurement uncertainty scales with detection size, confidence, timing
  uncertainty, and command-induced camera travel
- the host records the camera velocity caused by its final aim command with
  `infcore_record_anchor_prediction_motion`; Helios integrates that signal over
  each detection interval and adds it back to the observed screen displacement
  before estimating natural target motion
- the predicted offset contains target motion only. The active aim engine owns
  prediction of its virtual crosshair and must not be predicted a second time
  by Helios
- the projection horizon is measured frame age plus configured response delay
- `prediction_input_x/y` is the integrated command-induced camera displacement
  removed from the most recent observed interval, in output pixels
- prediction flags are enabled `1`, warmup `2`, valid `4`, clamped `8`, track
  reset `16`, camera compensated `32`, and configured input unavailable `64`
- while anchor prediction has a valid estimate and anchor drawing is enabled,
  InferenceCore draws the selected target's predicted anchor with the configured
  anchor color and its raw detection anchor in secondary gray
- raw structs are POD and safe to parse directly in C++

`InfcoreAnchorPredictionConfig` fields:

| Field | Valid values | Meaning |
|---|---:|---|
| `struct_size` | `sizeof(InfcoreAnchorPredictionConfig)` | Exact structure size required by the current ABI |
| `x_enabled`, `y_enabled` | `0` or `1` | Enables projection independently per screen axis |
| `response_delay_ms` | `0..1000` | Command-to-visible camera delay used to align recorded aim motion and included in the projection horizon |
| `lead_x`, `lead_y` | `0..8` | Per-axis target-motion projection multipliers |
| `_reserved` | all zero | Reserved for future ABI fields |

`infcore_record_anchor_prediction_motion` records one sample of the camera
motion induced by the final command sent to the game:

| Argument | Contract |
|---|---|
| `timestamp_ns` | Nonzero `std::chrono::steady_clock` timestamp for the command |
| `velocity_x`, `velocity_y` | Signed camera velocity in screen-heights per second |

Call it continuously from one producer, including samples whose velocity is
zero. The timestamps must use the same monotonic clock domain as frame and
result timestamps. The values must describe the final command after aim,
recoil, and manual-input composition. Helios consumes the samples through a
fixed-capacity SPSC queue; a full queue fails explicitly instead of dropping or
silently replacing motion history.

## OCR ABI

Defined in:

- `src/core/crypto/HeliosInferenceOcrABI.h`

OCR uses PP-OCRv6 small recognition ONNX through ONNX Runtime DirectML. This is the only OCR backend in the native runtime. There is no alternate OCR backend, full-frame CPU OCR path, or CPU execution-provider fallback. The OCR runtime binds the recognizer to the fixed input shape `[1, 3, 48, 320]`; the full graph must assign to DirectML. The OCR DirectML session is created only when OCR is enabled and at least one enabled region exists. Missing runtime files, model files, or unsupported DirectML graph assignment fail explicitly when OCR is requested: during `infcore_start_inference(...)` if OCR was configured before start, or during the OCR setter that activates it after start.

OCR is region-based:

- maximum regions: `HELIOS_INFERENCE_OCR_MAX_REGIONS = 16`
- maximum text bytes per result: `HELIOS_INFERENCE_OCR_MAX_TEXT_BYTES = 256`
- supported region modes:
  - `infcore_set_ocr_region_pixels(engine, region_id, x, y, width, height)`
  - `infcore_set_ocr_region_normalized(engine, region_id, x1, y1, x2, y2)`
- `region_id` is caller-owned and is used to fetch the result later
- OCR is disabled by default
- `skip_when_detections` is enabled by default, so frames with published object detections do not schedule OCR unless disabled by the caller
- OCR region corner drawing is enabled by default and can be disabled with `infcore_set_draw_ocr_region(...)` / `Session::setDrawOcrRegion(...)`
- When fuser output is enabled, OCR region corner drawing is emitted to the same batched fuser command buffer as normal inference-core drawings
- OCR color setters use BGR component order, matching the other overlay color setters
- OCR-only scripts should create an empty `Session` with `Session::create()`, configure at least one region, enable OCR, then call `start()`

OCR defaults:

| Setting | Default |
|---|---|
| OCR scheduling | disabled |
| Skip OCR when object detections exist | enabled |
| Configured regions | none |
| OCR region drawing | enabled |
| OCR region color | `#c059df` for every region ID, stored as BGR `(223, 89, 192)` |
| Per-region color | none; only changes when explicitly set by `region_id` |
| New pixel/normalized region | enabled immediately, result status starts as `HELIOS_INFERENCE_OCR_STATUS_EMPTY` |
| Region scheduling order | round-robin across enabled regions, starting at the first configured slot |
| OCR complete event | nonsignaled until OCR work publishes a result |
| Latest result for an unconfigured or empty region | unavailable; `getOcrResult(...)` returns false |
| Disabled region result | `HELIOS_INFERENCE_OCR_STATUS_DISABLED` |

Scheduling behavior:

- object inference remains the primary path
- the inference thread prepares the selected OCR ROI from the clean frame after detection results are final and before overlay drawing
- OCR runs on a lower-priority worker thread
- at most one active OCR region is processed per scheduled frame, round-robin across enabled regions
- OCR work is latest-only; if the worker is busy, older pending OCR frames are dropped
- OCR copies and preprocesses only the selected ROI into the recognizer input tensor, never the full frame
- the worker runs the recognizer from that prepared tensor and does not reread the video ring

OCR result access:

```cpp
HeliosInferenceOcrResult ocr{};
if (engine.getOcrResult(regionId, ocr) && ocr.status == HELIOS_INFERENCE_OCR_STATUS_READY) {
    // ocr.text is UTF-8 and ocr.text_bytes is the byte count.
}
```

Raw OCR mapping/event names are available from:

- `infcore_get_ocr_results_name(...)`
- `infcore_get_ocr_complete_event_name(...)`
- `Session::ocrResultsName()`
- `Session::ocrCompleteEventName()`

`HeliosInferenceOcrResultsBlock::regions` begins on a 64-byte boundary. The
published `HeliosInferenceOcrResult` record remains 360 bytes for ABI
compatibility, so later records are not individually cache-line aligned. Raw
mapping consumers must validate `struct_size` against
`sizeof(HeliosInferenceOcrResultsBlock)` and use the shipped header types
instead of inventing a record stride.

OCR region color:

```cpp
engine.setColorOcrRegion(223, 89, 192);          // global default: #c059df
engine.setColorOcrRegion(regionId, 0, 255, 255); // per-region override
```

OCR region drawing:

```cpp
engine.setDrawOcrRegion(false); // disables all OCR region corner drawing
```

In-frame OCR corner pixels are drawn just outside the configured OCR region so the overlay does not write into the pixels sampled by OCR.

Top-level OCR result fields:

- `write_sequence`
- `frame_sequence`
- `timestamp_ns`
- `region_id`
- `status`
- `confidence`
- `text_bytes`
- `frame_width`, `frame_height`
- `x`, `y`, `width`, `height`
- `flags`
- `text[256]`

Status values:

- `HELIOS_INFERENCE_OCR_STATUS_EMPTY = 0`
- `HELIOS_INFERENCE_OCR_STATUS_READY = 1`
- `HELIOS_INFERENCE_OCR_STATUS_NO_TEXT = 2`
- `HELIOS_INFERENCE_OCR_STATUS_ERROR = 3`
- `HELIOS_INFERENCE_OCR_STATUS_DISABLED = 4`

Flags:

- `HELIOS_INFERENCE_OCR_FLAG_TRUNCATED`: result text exceeded the fixed ABI buffer

## Raw Result Lifetime

`waitForResultsRaw()` returns a direct view into the mapped results block.

Lifetime rule:

- valid until the next call that can replace or detach results

Practical rule for agents:

- parse or copy the fields you need immediately
- do not keep the returned pointer across later inference calls

## Setter Mapping

The native `Session` mirrors the Python setter surface with C++ names.

The C ABI, C++ SDK, and Python API do not expose a per-session backend selector. Backend selection follows the Helios Compute GPU preference passed to the selected TensorRT-major-specific InferenceCore DLL; the default value is `auto`.

Examples:

| Python | Native C++ |
|---|---|
| `set_confidence_threshold` | `setConfidenceThreshold` |
| `set_nms_threshold` | `setNmsThreshold` |
| `set_segmentation_mask_threshold` | `setSegmentationMaskThreshold` |
| `set_draw_segmentation_masks` | `setDrawSegmentationMasks` |
| `set_segmentation_mask_opacity` | `setSegmentationMaskOpacity` |
| `set_color_segmentation_mask` | `setColorSegmentationMask` |
| `clear_roi` | `clearRoi` |
| `set_roi` | `setRoi` |
| `set_roi_pixels` | `setRoiPixels` |
| `set_roi_model_size` | `setRoiModelSize` |
| `set_roi_recommended_size` | `setRoiRecommendedSize` |
| `set_use_recommended_roi` | `setUseRecommendedRoi` |
| `set_draw_detections` | `setDrawDetections` |
| `set_draw_benchmarks` | `setDrawBenchmarks` |
| `set_draw_confidence` | `setDrawConfidence` |
| `set_polar_origin` | `setPolarOrigin` |
| `set_polar_origin_ring` | `setPolarOriginRing` |
| `set_polar_origin_ring_focus` | `setPolarOriginRingFocus` |
| `set_anchor_point` | `setAnchorPoint` |
| button-selected anchors | `setAnchorButtonProfiles` |
| `set_pose_anchor_point` | `setPoseAnchorPoint` |
| `infcore_set_anchor_prediction` | `setAnchorPrediction` |
| `infcore_record_anchor_prediction_motion` | `recordAnchorPredictionMotion` |
| `set_ocr_enabled` | `setOcrEnabled` |
| `set_ocr_skip_when_detections` | `setOcrSkipWhenDetections` |
| `set_draw_ocr_region` | `setDrawOcrRegion` |
| `set_ocr_region_pixels` | `setOcrRegionPixels` |
| `set_ocr_region_normalized` | `setOcrRegionNormalized` |
| `set_ocr_region_enabled` | `setOcrRegionEnabled` |
| `remove_ocr_region` | `removeOcrRegion` |
| `get_ocr_result` | `getOcrResult` |
| `set_color_ocr_region` | `setColorOcrRegion` |
| `set_target_delay_ms` | `setTargetDelayMs` |
| `set_class_priority` | `setClassPriority` |
| `set_class_priority_recommended` | `setClassPriorityRecommended` |
| `set_use_recommended_class_priority` | `setUseRecommendedClassPriority` |
| `set_sort_method` | `setSortMethod` |
| `set_zoom_enabled` | `setZoomEnabled` |
| `set_search_buttons` | `setSearchButtons` |
| `set_draw_keypoints` | `setDrawKeypoints` |
| `set_draw_skeleton` | `setDrawSkeleton` |
| `set_keypoint_conf_threshold` | `setKeypointConfThreshold` |
| `set_color_bbox` | `setColorBbox` |
| `set_color_anchor` | `setColorAnchor` |
| `set_color_keypoints` | `setColorKeypoints` |
| `set_color_skeleton` | `setColorSkeleton` |

Visualization notes:

- `setDrawRoi(...)`, `setDrawZoomedRoi(...)`, and `setDrawOcrRegion(...)` render corner markers, not full rectangles.
- Corner marker horizontal and vertical segments are derived from each region's width and height and capped below half the side length, so opposing segments do not touch on small regions.

Search-button activation uses simple boolean state semantics:

- button value `> 25.0` = pressed
- button value `<= 25.0` = released

The C++ SDK keeps one logical setter name per Python method:

- `setSearchButtons(uint32_t mask)` for a simple pressed-mask
- `setSearchButtons(const int* buttonIndices, const int* states, int count)` for one AND clause
- `setSearchButtons(const uint32_t* pressedMasks, const uint32_t* releasedMasks, int clauseCount)` for OR-of-AND clauses
- Empty input or zero clauses clears search activation buttons and returns to always-search behavior
- Any non-empty search button configuration enables button-gated search activation automatically

For the Python setters that accept `class_ids=None`, the C++ SDK uses optional `(const int* classIds = nullptr, int classCount = 0)` parameters on the same setter name. Passing `nullptr` or `classCount <= 0` targets all classes.

Confidence notes:

- Global confidence applies to classes without a per-class override.
- Per-class confidence overrides are applied during parser confidence gating, so a class can use a lower threshold than global without lowering the threshold for every other class.
- The override applies to the model-selected class for a candidate; it does not relabel a candidate to a lower-scoring class.

Class-priority notes:

- Only the highest-priority tier represented in the parser's retained candidates is published. Lower tiers do not backfill unused detection slots.
- Classes grouped in the winning tier have equal priority and share the global 64-detection capacity.
- A priority list `{5, 2, 7}` publishes only class `5` when a retained class `5` candidate exists; otherwise it publishes only class `2`, then only class `7` if neither higher tier is represented.
- The parser retains at most 64 candidates globally by confidence before runtime priority-tier selection. Priority tiers do not reserve capacity during parser selection.
- The winning tier is selected before ignore-region and polar-ring rejection. If those filters remove every candidate in that tier, lower tiers are not reconsidered for that frame.

Pose-anchor notes:

- `POSE_ANCHOR_HEAD` uses the center of the available head keypoints (`nose`, eyes, ears), with X averaged against shoulder center when both shoulders are available.
- `POSE_ANCHOR_CHEST` uses the shoulder center shifted 15% toward the hip center when hips are available.
- `POSE_ANCHOR_ABDOMEN` uses the hip center shifted 20% toward the shoulder center when shoulders are available.

Button-selected anchor notes:

- `setAnchorButtonProfiles(buttonIndices, anchors, profileCount, classIds, classCount)` configures up to 8 class-specific anchor profiles.
- `anchors` is a flat float array with four values per profile: `pct_x, pct_y, flat_x, flat_y`.
- A button is considered pressed when its value is greater than `25.0`.
- Profiles are first-match wins.
- A matched profile overrides that class's static anchor. If none match, the class static anchor is used. If neither exists, the global static anchor is used.
- `classIds` must contain at least one class id.
- Passing null buttons/anchors or `profileCount <= 0` clears button-selected anchors for the selected class scope.

Polar ring focus notes:

- `setPolarOriginRingFocus(true)` narrows the active ring radius toward the smallest radius that contains the valid selected target's full bounding box.
- Focus only uses published valid detections, so target-delay candidates do not shrink the ring before they become valid.
- When no valid target is selected, the focused radius grows back toward the configured ring radius using inference-core constants.

Target-delay notes:

- `setTargetDelayMs(0)` disables delayed target validation.
- Non-zero values publish `num_detections = 0` until the global continuous detection age reaches the threshold.
- During the delay window, accepted candidates may still be drawn with the normal secondary-target overlay style; they are not published as valid detections until the delay passes.
- The delay timer is not class-specific, carries across class swaps, and resets after one second without accepted detections.

Session lifecycle mirrors Python too:

- `start`
- `pause`
- `resume`
- `stop`
- `destroy`

Recommended-setting behavior:

- New sessions default to using recommended ROI size and recommended class priority on initial create/load.
- If the loaded model has no recommended ROI metadata, the engine uses an effective recommended ROI of `640x640`.
- If the loaded model has no recommended class-priority metadata, the engine uses class `0`.
- `clearRoi()`, `setRoi(...)`, `setRoiPixels(...)`, and `setRoiModelSize(...)` disable the recommended-ROI preference until you call `setUseRecommendedRoi(true)` or `setRoiRecommendedSize(...)`.
- `setClassPriority(...)` disables the recommended-class-priority preference until you call `setUseRecommendedClassPriority(true)` or `setClassPriorityRecommended()`.
- `setUseRecommendedRoi(true)` keeps the preference enabled for future model loads and applies the current model's recommended ROI immediately. If the model has no recommended-ROI metadata, it falls back to `640x640`.
- `setUseRecommendedClassPriority(true)` keeps the preference enabled for future model loads and applies the current model's recommended class priority immediately. If the model has no class-priority metadata, it falls back to class `0`.
- `loadModel`
- `unloadModel`
- `waitForResultsRaw`
- `loadedModelDescription`
- `runtimeError`
- `lastRequestedSequence`

`Session::loadedModelDescription()` and
`infcore_get_loaded_model_description()` return the manifest name, version,
task, exact loaded input dimensions, precision, and backend as one
human-readable string. The Python binding prints this description after initial
and hot loads. The technical UUID/cache key remains available through model
metadata but is not used as the successful-load label.

`triggerFrame(...)` is internal host-trigger plumbing in the packaged path. Script code should not call or own inference start.

## Protected Models

Protected DirectML `.ennx` models and TensorRT `.eecache` engines use one runtime authorization and key-delivery flow for both script hosts. Plain `.onnx` and `.ecache` files do not request a decryption key.

### Trusted host requirement

`InferenceCoreTrt10.dll` and `InferenceCoreTrt11.dll` are not general-purpose SDK
DLLs. Before exposing model, adapter, probe, build, or load functionality, each
DLL verifies its own exact hash and one of these manifest-authorized hosts:

- `HeliosApp.exe`, for application-owned operations such as Engine Builder;
- `CVCppWrapper.exe`, after its one-shot child-launch authorization succeeds;
- `cvpython_host.pyd`, after its one-shot child-launch authorization succeeds
  inside the selected Python process.

The runtime host functions must identify the live, manifest-authorized Helios
process and its expected video ring. Environment variables and caller-supplied
PIDs are inputs after this check; they are not accepted as proof of a trusted
host. Loading a copied InferenceCore DLL from another process is unsupported and
fails generically.

These checks run during DLL/model setup only. They add no per-frame work,
copies, allocations, locks, polling, or thread hops.

The packaged child host also reopens the selected `.dll` or `.py` file,
recomputes its SHA-256 against the identity authorized by Helios, resolves the
path from that verified handle, and retains the handle without write or delete
sharing until execution ends. This closes the file-swap window between grant
creation and script execution and is likewise startup-only.

### Runtime identity

The host provides:

- `script_type`
- `script_hash`
- `execution_grant`

The trusted host exposes these values through direct host functions and mirrors
them into the process environment consumed by the selected TensorRT-major-specific InferenceCore DLL:

- `HELIOS_SCRIPT_TYPE`
- `HELIOS_SCRIPT_HASH`
- `HELIOS_EXECUTION_GRANT`

The Helios application hashes the exact selected script and obtains a scoped
execution grant before launching the packaged child. The account token is used
only by the Helios application for that exchange; it is not included in the
launch payload, child environment, Python module, or native host functions. The
fixed production routes are:

- `POST https://www.inputsense.com/api/scripts/index_v5.php?route=runtime-grant`
- `POST https://www.inputsense.com/api/scripts/index_v5.php?route=onnx/key`

Responses are signed and bound to the original request timestamp/nonce. The native runtime rejects a different host or route instead of accepting a caller-provided endpoint.

### Supported runtime script types

- `py` for CV Python
- `dll` for CV C++

### Server grant flow

1. The Helios application hashes the exact selected local script file.
2. The application authenticates once to the grant route and receives a grant bound to the account session, grant audience, permission, script type, and exact script hash.
3. The packaged child re-verifies and read-locks that exact script before publishing `script_type` plus `script_hash` through direct host functions.
4. On protected model load, the selected TensorRT-major-specific InferenceCore DLL submits only that grant and the bound runtime identity. The server matches:
   - `script_type`
   - `script_hash`
   against an approved uploaded script record.
5. The key route resolves the referenced account session authoritatively and rejects expired or revoked sessions.
6. The key route requires the exact runtime identity, uploaded-script access, protected-model access, and valid grant before returning a protected decryption key.
7. Authentication/tag verification must succeed before an encrypted model or engine is loaded.

### Access and offline behavior

- An authenticated user may build a public encrypted model.
- A private or locked model additionally requires author, administrator, or
  explicit model authorization.
- Runtime loading also requires the approved script identity and execution
  grant described above.
- Every `.ennx` and `.eecache` open requests fresh online authorization.
  Building an encrypted engine cache does not create an offline entitlement.
- Existing ENNX v1 and encrypted-engine-cache files retain their UUID/salt key
  derivation and require no migration or user action.

### Hybrid CV Python behavior

For a Python main script plus native helper:

- the Python host hashes the selected `.py` file
- the helper DLL resolves those host identity functions
- all sessions in the process reuse the same runtime identity and grant state

### Pure CV C++ behavior

For a CV C++ script:

- `CVCppPlugin` hashes the selected `.dll`
- the wrapper exposes that identity to the script DLL through direct functions and environment
- `CVCppPlugin` obtains the grant before launching the wrapper; the wrapper receives no account token

## Hybrid Pattern

Recommended split:

1. Python owns orchestration and high-level module composition.
2. Native helper owns:
   - session creation
   - trigger/wait
   - raw result parsing
   - target selection / compact native state
3. Python only consumes the compact parsed struct it actually needs.

This avoids turning raw detections into Python objects unless the script explicitly wants them.

## Minimal Hybrid Helper Pattern

```cpp
struct TargetState {
    bool valid{false};
    float centerX{0.0f};
    float centerY{0.0f};
    float confidence{0.0f};
    uint32_t classId{0};
};

TargetState updateTarget(Session& engine) {
    TargetState out;
    const HeliosInferenceResultsBlock* raw = engine.waitForResultsRaw();
    if (!raw || raw->num_detections == 0) {
        return out;
    }

    const HeliosInferenceDetection& det = raw->detections[0];
    out.valid = true;
    out.centerX = det.center_x;
    out.centerY = det.center_y;
    out.confidence = det.confidence;
    out.classId = det.class_id;
    return out;
}
```

## Minimal Hybrid OCR Pattern

Configure OCR once after the helper creates or starts its `Session`:

```cpp
constexpr uint32_t kAmmoTextRegion = 100;

void configureOcr(Session& engine) {
    engine.setOcrRegionPixels(kAmmoTextRegion, 80, 40, 360, 80);
    engine.setColorOcrRegion(223, 89, 192);
    engine.setOcrSkipWhenDetections(true);
    engine.setOcrEnabled(true);
}
```

Read OCR as a separate, lower-priority side result. It is not part of the detection result block and may lag the frame that triggered it:

```cpp
struct OcrText {
    bool ready{false};
    uint64_t frameSequence{0};
    float confidence{0.0f};
    char text[HELIOS_INFERENCE_OCR_MAX_TEXT_BYTES]{};
};

OcrText readOcr(Session& engine) {
    OcrText out;
    HeliosInferenceOcrResult result{};
    if (!engine.getOcrResult(kAmmoTextRegion, result) ||
        result.status != HELIOS_INFERENCE_OCR_STATUS_READY) {
        return out;
    }

    out.ready = true;
    out.frameSequence = result.frame_sequence;
    out.confidence = result.confidence;
    const uint32_t bytes = result.text_bytes < HELIOS_INFERENCE_OCR_MAX_TEXT_BYTES - 1
        ? result.text_bytes
        : HELIOS_INFERENCE_OCR_MAX_TEXT_BYTES - 1;
    std::memcpy(out.text, result.text, bytes);
    out.text[bytes] = '\0';
    return out;
}
```

Hybrid helpers that need event-driven OCR reads can open `Session::ocrCompleteEventName()` directly. Most helpers should just read the latest OCR result after their normal inference update and use `frame_sequence` to reject stale text if needed.

## Authentication, Errors, And Model Metadata

- The Helios application owns the authenticated account session and exchanges it for a script-scoped execution grant before child launch. Scripts and `InferenceCore` never receive or read the account token. There is no public session-token setter or host-context token callback.
- Native error code and debug text belong to the calling thread. Read `infcore_get_last_error()` / `infcore_get_debug_log()` immediately after the failed call, on that same thread. A later operation may replace or clear them.
- Model-load diagnostics identify the manifest name, version, and author before
  the exact model/engine ID, selected backend and GPU, and native backend
  cause. TensorRT load failures include the cache profile and available
  TensorRT/CUDA diagnostics. Credentials, decryption keys, and decrypted model
  data are never included.
- `INFCORE_ERROR_INFERENCE` (`17`) is an asynchronous hard backend failure.
  Direct ABI callers read it from
  `_reserved[HELIOS_INFERENCE_RESULT_RUNTIME_ERROR_INDEX]` and then call
  `infcore_get_runtime_error(engine, buffer, size)`. The returned error belongs
  to the currently loaded model and remains set until that model is replaced.
- `HeliosInferenceSDK.hpp::Library::lastErrorString()` reports SDK/Windows failures as well as the matching core failure. Do not query it as a historical log after an unrelated operation.
- `infcore_list_models(nullptr, 0)` returns the required JSON byte count excluding the NUL terminator. Allocate `count + 1` bytes, then call it again with that exact buffer. The packaged SDK does this automatically and therefore has no fixed model-count/64 KiB limit.
- An empty `Library::listModelsJson()` result means discovery failed; use `lastErrorString()`. A nonempty result that fails parsing is a caller JSON/parser error and must be reported as such rather than replaced with the core's last error.
- Native JSON exports are ASCII JSON. Parsers must implement standard `\uXXXX` decoding, including valid UTF-16 surrogate pairs, and must reject malformed or unpaired surrogates.

## Important Limits

- The native SDK is Windows-only.
- The ABI is C/POD based. Do not treat internal C++ implementation types as public ABI.
- The raw results block is read-only from script code.
- Agents should not include `InferenceCoreInternal.hpp`.
- Native JSON exports are NUL-terminated ASCII JSON. Unicode metadata is encoded with standard `\u` escapes and round-trips losslessly through a conforming JSON parser.
- `infcore_list_models(...)` filters by the selected object-inference backend. NVIDIA/TensorRT selections receive TensorRT cache entries. DirectML selections receive ONNX/ENNX entries only.
- Packaged hosts pass `HELIOS_COMPUTE_ADAPTER` from Helios Preferences before loading the selected TensorRT-major-specific InferenceCore DLL; the default value is `auto`.

## File Map

All distributed files below are relative to
`versions/<version>/sdk/include/helios/`:

- Native engine C API: `InferenceCore.h`, with its function manifest in
  `InferenceCoreApi.h`
- Public raw results ABI: `HeliosInferenceResultsABI.h`
- Public segmentation results ABI: `HeliosInferenceSegmentationResultsABI.h`
- Public OCR results ABI: `HeliosInferenceOcrABI.h`
- Public host function signatures: `HeliosHostFunctions.h`
- Public host trigger ABI: `HeliosInferenceHostTriggerABI.h`
- Native C++ scripting wrapper: `HeliosInferenceSDK.hpp`

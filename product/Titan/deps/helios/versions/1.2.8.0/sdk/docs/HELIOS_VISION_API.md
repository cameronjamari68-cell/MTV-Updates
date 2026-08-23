# Helios Vision, Meter, and Overlay APIs

This is the canonical scripting reference for Helios frame analysis and drawing:

- `helios.vision`: prepared BGR contour, norm, and template matching
- `helios.meter`: user-fed meter measurements, timing, speed, and ETA
- `helios.overlay`: direct frame drawing and batched fuser commands

Vision and Meter call the same synchronous native implementation in `HeliosVision.dll`; neither binding contains a separate algorithm or fallback path. Overlay uses the script host's native drawing path and fixed command buffers.

Vision and Meter are active development packages. Their function and data
contracts may change while they are being completed; they are not yet part of
the finalized additive-only surface. The direct named-function architecture is
fixed and remains unversioned. Once Vision and Meter are explicitly finalized,
their existing contracts become stable and later functionality is added through
new named functions and new data structures. Overlay is already stable.

Vision runs on the calling script thread. A call does not enqueue work, wait on a worker, acquire a frame lock, or introduce a thread hop. The call returns only after its result is ready.

## Source of Truth

- `src/core/vision/HeliosVision.h`
- `src/core/vision/HeliosVision.cpp`
- `src/core/vision/ContourFinder.cpp`
- `src/core/vision/Matchers.cpp`
- `src/core/vision/HeliosMeter.h`
- `src/core/vision/Meter.cpp`
- `src/core/FuserOverlayTypes.hpp`
- `src/core/FuserOverlayAccessors.hpp`
- `src/plugins/cv_python/vision_module.cpp`
- `src/plugins/cv_python/CVEmbeddedRuntime.py`
- `src/plugins/cv_cpp/sdk/HeliosCVSDK.h`
- `src/plugins/cv_cpp/sdk/HeliosVisionSDK.hpp`

## Runtime Layout

- Native runtime: `lib/HeliosVision.dll`
- Python extension: `lib/py/_helios_vision.pyd`
- Python package: registered by `cvpython_host.pyd`; no Python package files are deployed
- C++ Vision and Meter SDK: `sdk/include/helios/HeliosVisionSDK.hpp`
- C++ aggregate SDK and overlay API: `sdk/include/helios/HeliosCVSDK.h`
- C ABI headers: `sdk/include/helios/HeliosVision.h` and
  `sdk/include/helios/HeliosMeter.h`

Vision and Meter share the native DLL but expose independent named C functions.
Consumers resolve only the functions they use. Adding another exported function
does not change any existing linkage or create another ABI.
See `THIRD_PARTY_SCRIPT_SDK.md` for the complete separately distributable
header set.

## Frame Contract

Per-frame Vision operations automatically consume the Helios host's active `HxWx3` 8-bit BGR frame. Meter updates consume caller-supplied points and only the current frame's dimensions, sequence, and timestamp. Scripts do not pass the current frame to `find()`, `match()`, or `update(point, release_point)`. The host publishes one thread-local borrowed descriptor for the duration of `process()`, and `HeliosVision.dll` reads it directly. It does not reacquire the ring slot, clone the current frame, or convert it to grayscale, HSV, YCrCb, or another intermediate format.

`HeliosVisionFrameView` contains:

- `data`: borrowed, read-only pixel pointer
- `data_size`: accessible byte span
- `width`, `height`, and `stride`: actual frame layout
- `pixel_format`: currently `HELIOS_VISION_PIXEL_FORMAT_BGR8`
- `frame_sequence` and `timestamp_ns`: metadata from the script host

The descriptor is valid only on the script thread while its `process()` callback is active. The native operation may use `data` only for the duration of the call. It does not own, pin, retain, or extend the lifetime of a video-ring slot. Calling a current-frame Vision or Meter operation outside `process()` returns `HELIOS_VISION_STATUS_NO_CURRENT_FRAME`.

Reference and template constructor inputs remain explicit because they are persistent prepared data rather than the current frame. Python accepts those images through a NumPy-compatible `__array_interface__` with `uint8` shape `(height, width, 3)` and strides `(row_stride, 3, 1)`. C++ accepts `cv::Mat` BGR views and preserves `step`. Padded rows are supported; transposed, reversed, planar, and channel-sliced views are rejected.

## BGR Color Ranges

A basic color range contains only inclusive `low` and `high` BGR triplets. This is the same threshold rule as `cv::inRange`/`cv2.inRange`; basic contour detection does not apply hidden brightness, channel, halo, or training tolerances.

Up to 32 exact ranges can be combined into the same mask. Touching pixels from any active range participate in the same contour. Per-range participation is populated by optional detailed color analysis; bit `n` in `color_mask` corresponds to color entry `n`.

## ContourFinder

`ContourFinder` is a prepared object. Its immutable search geometry, grouping mode, optional shape, and result storage are prepared once. Color ranges can be replaced without rebuilding the object through `ContourFinder.set_colors(...)` in Python, `setColors(...)` in C++, or `contour_finder_set_colors` in the C ABI. The number of ranges must remain unchanged.

Configuration fields are expressed in a reference resolution, normally `1920x1080`:

- `roi`: `(x, y, width, height)` search rectangle
- `width`: inclusive `(minimum, maximum)` component width
- `height`: inclusive `(minimum, maximum)` component height
- `reference_size`: coordinate system for the ROI and size ranges
- `connectivity`: currently `CONNECTIVITY_8`, matching the external contour pipeline
- `grouping`: independent connected components or all matching pixels in the ROI
- `max_results`: fixed result capacity
- `shape`: optional 2D `uint8` occupancy mask

The ROI and width range scale on the horizontal axis; the ROI and height range scale on the vertical axis. This keeps the configured search resolution-agnostic while returning result bounds in actual-frame pixels.

The basic native finder is an exact threshold-and-contour pipeline: it writes one reusable one-byte ROI mask with `cv::inRange`, extracts external simple contours with OpenCV's link-runs implementation, then applies inclusive width and height bounds. Link-runs avoids the additional temporary image used by the general `findContours` implementation. Basic `find()` does not compute color histograms, train colors, grow halos, or rescan accepted contours.

Grouping modes:

- `GROUP_CONNECTED`: return each accepted connected component independently. Different configured colors combine when their accepted pixels touch according to the selected connectivity.
- `GROUP_ALL_IN_ROI`: combine all accepted pixels in the ROI into one result, including disconnected parts. Width, height, color, and optional shape checks apply to the combined bounds.

Results are largest first by contour area. If more accepted components exist than `max_results`, Vision keeps the largest fixed-size set and marks the result view `truncated`.

### Optional Shape Filter

The shape input is a 2D `uint8` mask: zero is background and any nonzero value is foreground. It is normalized once into a `32x32` occupancy grid. Each candidate is normalized into the same grid, making the comparison independent of its pixel dimensions.

`shape_tolerance` dilates occupancy by zero to four grid cells before the symmetric comparison. `shape_score` is the minimum accepted score. The reported score is a symmetric precision/recall F1 score in `[0, 1]`; contour confidence is the geometric mean of color and shape scores when a shape is enabled.

This filter is intended for coarse silhouettes such as arcs, rainbows, bars, and irregular HUD outlines. It avoids expensive full-resolution contour descriptors while rejecting rectangular blobs with the same bounding dimensions.

### Result Data

Each basic contour reports:

- bounds and centroid in actual-frame pixels
- contour area and bounding-box fill ratio
- optional shape score and combined confidence
- result index and generation

Detailed per-result, per-color statistics are optional. They are computed only when `finder.color_stats(...)` is called. They use fixed 256-bin B, G, and R histograms and report:

- pixel count
- raw minimum and maximum
- 5th percentile, median, and 95th percentile
- mean BGR
- core and halo pixel counts

The percentiles are the preferred input for training because they reject isolated color outliers better than the raw extrema. Pixels matching multiple ranges are assigned to their best range for these per-color statistics.

### Result Lifetime

The C ABI returns a borrowed `HeliosVisionContourResults` view owned by its finder. Its data remains valid only until the next `contour_finder_find` or `contour_finder_set_colors` call, or finder destruction. Use `generation` to detect that a live view has advanced.

Python returns the same reusable `ContourResults` sequence on every `find()` call. Indexing the sequence materializes one basic Python dictionary; it does not run advanced color analysis. Do not treat a saved `ContourResults` object as a snapshot; copy the fields you need before the next call.

`finder.results` returns that same live sequence without running another search. It exposes `generation` and `truncated`; each indexed entry contains `bounds`, `centroid`, `area`, `fill_ratio`, `shape_score`, `confidence`, `result_index`, and `generation`. Call `finder.color_stats(result_index, color_index=0)` explicitly when advanced color analysis is required.

## NormMatcher

`NormMatcher` copies and packs its reference image or reference ROI once at construction. `match(roi)` then compares a same-size region of the current Helios frame directly against that prepared reference.

Methods:

- `NORM_L1`: sum of absolute channel differences (SAD)
- `NORM_L2_SQUARED`: sum of squared channel differences (SSD)

The result contains the raw sum, BGR channel sample count, mean error, and normalized similarity. Similarity is `1.0` for an exact match and approaches `0.0` as mean channel error approaches 255. The L2 result reports mean squared error, while its similarity is normalized after taking the root.

The requested match ROI must have exactly the prepared reference width and height. No resize or resampling is performed.

## TemplateMatcher

`TemplateMatcher` copies and packs its template or template ROI once at construction. `find(roi)` searches every valid integer position in the current Helios frame's search ROI and returns only the best match. It does not allocate or expose a response map.

Methods:

- `TEMPLATE_SAD`: sum of absolute differences; lower raw value is better
- `TEMPLATE_SSD`: sum of squared differences; lower raw value is better
- `TEMPLATE_NCC`: normalized cross-correlation; higher correlation is better

All methods expose a score normalized to `[0, 1]`, where `1.0` is best. SAD and SSD use exact early termination when a candidate can no longer beat the current best value. NCC reports `raw_value` as zero and maps correlation from `[-1, 1]` to `[0, 1]`.

The search ROI must be at least as large as the prepared template. Returned bounds are in actual-frame pixels.

## Python Surface

Constructors and call shapes:

```python
BgrRange(low, high)

ContourFinder(
    colors, roi, width, height,
    reference_size=(1920, 1080),
    connectivity=CONNECTIVITY_8,
    grouping=GROUP_CONNECTED,
    max_results=64,
    shape=None,
    shape_tolerance=1,
    shape_score=0.72,
)
ContourFinder.find() -> ContourResults
ContourFinder.set_colors(colors) -> None
ContourFinder.color_stats(result_index, color_index=0) -> dict

NormMatcher(reference, roi=None, method=NORM_L1)
NormMatcher.match(roi=None) -> NormMatcher

TemplateMatcher(template, roi=None, method=TEMPLATE_SAD)
TemplateMatcher.find(roi=None) -> TemplateMatcher
```

`roi` is `(x, y, width, height)` and width/height limits are inclusive `(minimum, maximum)` pairs. A color entry may be a `BgrRange` or the equivalent `(low_bgr, high_bgr)` sequence.

```python
from helios import vision

purple = vision.BgrRange(
    low=(105, 35, 115),
    high=(180, 95, 205),
)

finder = vision.ContourFinder(
    colors=[purple],
    roi=(700, 180, 520, 760),
    width=(8, 240),
    height=(20, 700),
    reference_size=(1920, 1080),
    connectivity=vision.CONNECTIVITY_8,
    grouping=vision.GROUP_CONNECTED,
    max_results=16,
)

def process(frame):
    results = finder.find()
    for contour in results:
        bounds = contour["bounds"]
```

Prepared matcher construction follows the same pattern:

```python
norm = vision.NormMatcher(reference, method=vision.NORM_L1)
norm.match(roi=(100, 100, 64, 32))
if norm.similarity > 0.98:
    ...

template = vision.TemplateMatcher(icon, method=vision.TEMPLATE_SAD)
template.find(roi=(0, 0, 640, 360))
if template.score > 0.95:
    x, y, width, height = template.bounds
```

The matcher returned by `match` or `find` owns the latest scalar result properties; no separate Python result object is required on the hot path. Norm exposes `raw_value`, `sample_count`, `mean_error`, and `similarity`. Template exposes `found`, `bounds`, `raw_value`, and `score`. Reading tuple or dictionary properties does allocate normal Python objects.

## C++ Surface

`HeliosVisionSDK.hpp` provides move-only RAII wrappers in `Helios::Vision`. They validate explicit reference/template `cv::Mat` inputs, resolve the required DLL functions by name, and destroy opaque handles automatically. Current-frame operations take no frame argument; `HeliosVision.dll` obtains the host's active borrowed descriptor directly.

```cpp
#include "HeliosVisionSDK.hpp"

#include <array>

namespace HV = Helios::Vision;

const std::array colors{
    HV::bgrRange(HV::bgr(105, 35, 115), HV::bgr(180, 95, 205)),
};

HV::ContourFinderConfig config{};
config.colors = colors;
config.roi = {700, 180, 520, 760};
config.width = {8, 240};
config.height = {20, 700};
config.connectivity = HV::Connectivity::Eight;
config.grouping = HV::Grouping::Connected;

HV::ContourFinder finder(config);

void process(const cv::Mat& frame) {
    const HV::ContourResultsView results = finder.find();
    for (const HV::Contour& contour : results.contours) {
        if (contour.confidence >= 0.8f) {
            // Use contour.bounds while this result generation is current.
        }
    }
}
```

`ContourResultsView::contours` is a borrowed `std::span`; the next `find()` or `setColors()` call invalidates the logical result view. `colorStats(resultIndex, colorIndex)` returns a caller-owned POD copy.

Matchers return their small POD results by value:

```cpp
HV::NormMatcher norm(reference, nullptr, HV::NormMethod::L1);
const HV::NormResult normResult = norm.match(&roi);

HV::TemplateMatcher templ(icon, nullptr, HV::TemplateMethod::Sad);
const HV::TemplateResult match = templ.find(&searchRoi);
```

`makeFrameView(...)` remains available only for preparing explicit reference/template image descriptors. Current-frame operations have one path and cannot accept an alternate frame. Invalid OpenCV layouts raise `std::invalid_argument`; a native status failure raises `Helios::Vision::Error` with the original status value.

## C ABI

Vision exports:

```c
const char* helios_vision_status_message(int32_t status);
int32_t helios_vision_contour_finder_create(...);
void helios_vision_contour_finder_destroy(...);
int32_t helios_vision_contour_finder_set_colors(...);
int32_t helios_vision_contour_finder_find(...);
int32_t helios_vision_contour_finder_color_stats(...);
int32_t helios_vision_norm_matcher_create(...);
void helios_vision_norm_matcher_destroy(...);
int32_t helios_vision_norm_matcher_match(...);
int32_t helios_vision_template_matcher_create(...);
void helios_vision_template_matcher_destroy(...);
int32_t helios_vision_template_matcher_find(...);
```

A consumer resolves each function it uses by its exact name. Opaque-handle
create/destroy pairs and contour, norm, and template calls are synchronous.
Current-frame C calls do not take a frame pointer; they consume the host
descriptor active on the calling script thread.
Status codes are returned as `HeliosVisionStatus`;
`helios_vision_status_message` provides a stable diagnostic string.

Only POD structures, function pointers, and opaque handle declarations cross
the DLL boundary. Native classes, storage, and algorithms remain private to
`HeliosVision.dll`; no OpenCV type crosses the C ABI. Configuration and prepared
image data are copied during creation where necessary. Opaque handles are not
thread-safe for simultaneous calls; keep one prepared object on the script
thread or provide external ownership synchronization.

## Meter

`helios.meter` is deliberately only a user-fed measurement object. It performs no image detection, contour search, color matching, direction inference, tracking, or training. Script logic supplies two current-frame pixel points: the detected meter point and its green/release point. The native object owns normalized geometry, timing, and optional Overlay rendering.

`update(point, release_point)` maps both points into Helios's standard `1920x1080` design space, measures the active path between them, and derives closing speed, elapsed time, and estimated time to release from the current frame's monotonic timestamp. This handles diagonal meters, keeps geometry independent of capture resolution, and keeps timing independent of frame rate. Meter reads no frame pixels.

X coordinates scale by `1920 / frame_width`; Y coordinates scale by `1080 / frame_height`. The public constants are `meter.DESIGN_WIDTH == 1920` and `meter.DESIGN_HEIGHT == 1080` in Python, or `Helios::Meter::DesignWidth` and `DesignHeight` in C++.

### Meter State

The latest state exposes:

- `meter_id`, `sample_count`, `frame_sequence`, and `timestamp_ns`
- `point` and `release_point`: supplied current-frame pixel points
- `roi`: the inclusive current-frame pixel rectangle enclosing the active path
- `point_design` and `release_point_design`: normalized design-space points
- `delta_design`: `release_point_design - point_design`
- `control_point_design`: the active quadratic control point; the midpoint for a straight path
- `path_algorithm`: `PATH_STRAIGHT` or `PATH_QUADRATIC_BEZIER`
- `straight_distance`: direct Euclidean distance between the points
- `distance` and `previous_distance`
- `distance_delta`: `previous_distance - distance`
- `speed`: distance units per second; positive means approaching release and negative means moving away
- `delta_time` and `elapsed_time` in seconds
- `time_to_release`: `distance / speed` while approaching, otherwise `-1.0`

Distance, distance delta, and speed use 1920x1080 design pixels and design pixels per second. Both supplied points must be inside the current frame. Coincident points produce zero distance and a `1x1` ROI. The first sample, or a sample whose timestamp did not advance, establishes a new timing baseline with zero speed. `reset()` clears all samples and timing while preserving the object's `meter_id` and settings.

### Meter Paths

The default `PATH_STRAIGHT` path is the direct segment between the meter and release points. `PATH_QUADRATIC_BEZIER` models a consistently curved or banana-shaped meter without running a Python callback in the hot loop. Its signed `curvature` selects the side of the chord and sets the curve's midpoint bulge as a fraction of straight distance: `0.25` means the curve midpoint is displaced by 25% of the chord length. Zero curvature produces the straight geometry.

The active curve changes `distance`, `distance_delta`, `speed`, `time_to_release`, `control_point_design`, and `roi`. Arc length uses the closed-form quadratic integral. `segments` is only the number of Overlay line segments used to display the curve, from 2 through 64; it does not change the measured arc length. Changing path settings resets the timing samples so a geometry-model change cannot create a false speed spike.

More path shapes can be added later as new named algorithms without exposing an expression evaluator or adding per-frame language callbacks.

### Meter Visuals

Visuals are enabled by default and use RGB cyan `(104, 244, 255, 255)`. Each update can send the following directly through the native Overlay path:

- one bounding box around the active meter path
- one straight distance line, or the configured number of quadratic curve segments
- independently enabled ETA, speed, distance, and elapsed-time metrics

The enabled metric lines form one compact stack. The stack is placed above the ROI when it fits, below when that is the usable side, and clamped into the 1920x1080 design space near frame edges. Disabling a metric removes its line and closes the gap automatically.

The default target is `overlay.BOTH`: the borrowed frame is drawn in place and the same commands are offered to Fuser. If Fuser is disabled, its half of `BOTH` is simply not published. Helios's global Disable Overlay Drawing preference still suppresses both targets without stopping meter calculations.

### Python Meter Surface

```python
Meter()
Meter.update(point, release_point) -> Meter
Meter.set_path(algorithm=PATH_STRAIGHT, curvature=0.25, segments=16) -> Meter
Meter.set_visuals(
    enabled=True,
    show_bbox=True,
    show_path=True,
    show_distance=True,
    show_speed=True,
    show_time_to_release=True,
    show_elapsed_time=True,
    bbox_color=(104, 244, 255, 255),
    path_color=(104, 244, 255, 255),
    metrics_color=(104, 244, 255, 255),
    bbox_thickness=1,
    path_thickness=1,
    text_scale=2,
    text_gap=5,
    target=overlay.BOTH,
) -> Meter
Meter.reset() -> None
```

```python
from helios import meter, overlay, vision

purple = vision.BgrRange((105, 35, 115), (180, 95, 205))
finder = vision.ContourFinder(
    colors=[purple],
    roi=(700, 180, 520, 760),
    width=(8, 160),
    height=(20, 700),
)
tracker = meter.Meter()
tracker.set_path(
    algorithm=meter.PATH_QUADRATIC_BEZIER,
    curvature=0.25,
    segments=16,
)
tracker.set_visuals(
    show_elapsed_time=False,
    bbox_color=meter.DEFAULT_COLOR,
    path_color=meter.DEFAULT_COLOR,
    metrics_color=meter.DEFAULT_COLOR,
    target=overlay.BOTH,
)

def process(frame):
    contours = finder.find()
    measurement = detect_meter_in_python(contours)
    if measurement is None:
        return

    meter_point, release_point = measurement
    tracker.update(point=meter_point, release_point=release_point)
    use_meter(tracker.distance, tracker.speed, tracker.time_to_release)
```

`update()`, `set_path()`, and `set_visuals()` return the same Meter object. A color accepts RGB or RGBA; omitted alpha defaults to 255. `path_settings` and `visual_settings` return dictionaries containing the active settings. The state properties are `meter_id`, `sample_count`, `frame_sequence`, `timestamp_ns`, `point`, `release_point`, `roi`, `point_design`, `release_point_design`, `delta_design`, `control_point_design`, `path_algorithm`, `straight_distance`, `distance`, `previous_distance`, `distance_delta`, `speed`, `delta_time`, `elapsed_time`, and `time_to_release`.

### C++ Meter Surface

```cpp
#include <helios/HeliosVisionSDK.hpp>

namespace HM = Helios::Meter;
namespace HV = Helios::Vision;

HM::Meter tracker;

HM::PathSettings path;
path.algorithm = HM::PathAlgorithm::QuadraticBezier;
path.curvature = 0.25;
path.segments = 16;
tracker.setPath(path);

HM::VisualSettings visuals;
visuals.showElapsedTime = false;
visuals.target = HM::VisualTarget::Both;
visuals.bboxColor = HV::rgba(104, 244, 255);
visuals.pathColor = HV::rgba(104, 244, 255);
visuals.metricsColor = HV::rgba(104, 244, 255);
tracker.setVisuals(visuals);

void process(const cv::Mat& frame) {
    const HV::PointI32 meterPoint = detectMeterPoint(frame);
    const HV::PointI32 releasePoint = detectReleasePoint(frame);
    const HM::State& state = tracker.update(meterPoint, releasePoint);
    useMeter(state.distance, state.speed, state.time_to_release_seconds);
}
```

`update()` and `state()` return object-owned state overwritten by the next update or reset. `setPath()`/`pathSettings()` and `setVisuals()`/`visualSettings()` are the typed C++ equivalents of the Python configuration calls.

### Meter C ABI

```c
int32_t helios_meter_create(HeliosMeter** out_meter);
void helios_meter_destroy(HeliosMeter* meter);
int32_t helios_meter_update(
    HeliosMeter* meter,
    const HeliosVisionPointI32* point,
    const HeliosVisionPointI32* release_point,
    const HeliosMeterState** out_state);
const HeliosMeterState* helios_meter_state(const HeliosMeter* meter);
int32_t helios_meter_set_path_settings(
    HeliosMeter* meter,
    const HeliosMeterPathSettings* settings);
const HeliosMeterPathSettings* helios_meter_path_settings(const HeliosMeter* meter);
int32_t helios_meter_set_visual_settings(
    HeliosMeter* meter,
    const HeliosMeterVisualSettings* settings);
const HeliosMeterVisualSettings* helios_meter_visual_settings(const HeliosMeter* meter);
void helios_meter_reset(HeliosMeter* meter);
```

Each function is resolved independently by name and uses an opaque Meter handle.
The state storage and timing history stay private to `HeliosVision.dll`.

## Overlay

Overlay is the small native drawing API for CV Python and CV C++. OpenCV drawing remains unchanged and independent. Calls are valid inside the script `process()` frame callback.

Every primitive selects one target:

| Target | Python | C++ | Behavior |
|---|---|---|---|
| Frame | `overlay.FRAME` | `Helios::Overlay::Target::Frame` | Draw directly into the borrowed frame |
| Fuser | `overlay.FUSER` | `Helios::Overlay::Target::Fuser` | Append a compact fuser command |
| Both | `overlay.BOTH` | `Helios::Overlay::Target::Both` | Perform both operations |

Both is the default. Frame drawing adds no frame copy. Fuser commands accumulate in a fixed per-frame buffer and are batch-published after `process()` returns; inference and OCR commands join the same frame batch regardless of which producer finishes first. Exceeding the capacity fails the script frame instead of allocating or silently dropping commands.

Coordinates default to current-frame pixels. `DESIGN_1080P`/`CoordinateSpace::Design1080p` scales X by frame width, Y by frame height, and lengths by frame height from a 1920x1080 design space.

### Python Overlay Surface

```python
from helios import overlay

def process(frame):
    overlay.rect(100, 100, 300, 220, color=(0, 255, 0), thickness=2)
    overlay.circle(200, 160, 20, color=(255, 0, 0), filled=True,
                   target=overlay.BOTH)
    overlay.text(
        "TARGET", 960, 90,
        color=(255, 255, 255), alpha=230, scale=2,
        background_color=(0, 0, 0), background_alpha=120,
        padding=6, anchor=overlay.TOP_CENTER,
        space=overlay.DESIGN_1080P)
    return frame, bytearray()
```

The callable primitives are `line`, `rect`/`rectangle`, `circle`, and `text`. Helios colors are RGB with optional alpha. Text supports nine anchors, background color/alpha, padding, and integer scale. Rectangles and circles fill when `filled=True` or thickness is nonpositive.

### C++ Overlay Surface

```cpp
#include "HeliosCVSDK.h"

Helios::Overlay::rect(
    100, 100, 300, 220,
    Helios::Overlay::Color{0, 255, 0}, 2,
    Helios::Overlay::Target::Both);

Helios::Overlay::text(
    960, 90, "TARGET",
    Helios::Overlay::Color{255, 255, 255, 230}, 2,
    Helios::Overlay::Target::Fuser,
    Helios::Overlay::TextAnchor::TopCenter,
    Helios::Overlay::CoordinateSpace::Design1080p,
    Helios::Overlay::Color{0, 0, 0, 120}, 6);
```

OpenCV convenience overloads accept `cv::Point` and `cv::Scalar`; `cv::Scalar` keeps BGR order and is converted internally. Use `Helios::Overlay::Color{r, g, b, a}` for explicit RGBA values.

The four C++ Overlay entry points are exported by both `CVCppWrapper.exe` and the CV Python `_helios_controls.pyd` module. Native helper DLLs running inside a CV Python script may therefore include `HeliosCVSDK.h` and use the same `Helios::Overlay` surface. Their calls use the current CV Python frame context: `Frame` draws into the borrowed frame, while `Fuser` commands join the host's fixed command batch. The host publishes that batch after `process()` returns; a later inference or OCR contribution refreshes the same sequence without replacing the script commands.

The low-level shared-memory contract is declared by `FuserOverlayTypes.hpp` and `FuserOverlayAccessors.hpp`. Most packaged CV scripts should use the high-level `HeliosCVSDK.h` API instead of writing ring commands directly.

### Global Drawing Toggle

`Preferences > Other > Video Display > Disable Overlay Drawing` is unchecked by
default. Enabling it skips packaged script, inference, and OCR drawing for both
the borrowed frame and fuser output without stopping processing or result
production. Direct low-level fuser users must check `isDrawingEnabled()` before
writing commands.

## Performance Contract

- Current frames are borrowed and read in place: zero frame copies.
- There is no image-format conversion, frame copy, response map, worker thread, queue, lock, or thread hop.
- Basic contour detection writes one reusable one-byte mask for the configured ROI, matching `inRange`; it does not copy the BGR frame. OpenCV's link-runs contour extraction avoids a second temporary image.
- Mask and result storage are reused. OpenCV may grow contour point storage for a more fragmented result; subsequent calls can reuse retained capacity.
- Norm references and templates incur one packed copy during object construction; matching performs no per-frame image copy or allocation.
- SAD and SSD inner loops use AVX2 vector operations with scalar tails.
- Python passes no current-frame object into native Vision/Meter calls and reuses native result objects. Python tuples and dictionaries are created only when their exposed properties or contour entries are read.
- Meter reads no frame pixels and performs no detection. Geometry, metrics, formatting buffers, and placement use fixed-size stack/scalar state with no allocation, lock, conversion, queue, callback, or thread hop. A straight visual update emits at most one rectangle, one line, and four text calls; a curved visual update replaces the line with 2–64 line calls. Disabling visuals emits no Overlay calls.
- Overlay frame primitives touch only drawn pixels; fuser primitives append fixed-size commands to one sequence-owned batch. A late inference or OCR contribution can refresh that same batch without allocating or copying the frame.

Execution time still scales with ROI area, number of matching runs, template dimensions, and template search area. Keep ROIs and size ranges as tight as the game permits; the API does not claim that every possible full-frame search is sub-millisecond.

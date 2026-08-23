# Third-Party Script SDK

Helios supports third-party Python scripts and C++ script DLLs inside the
packaged CV Python and CV C++ hosts. The hosts own frame selection, shared-memory
names, authentication, runtime DLL loading, and inference triggering.

The SDK is not a standalone Helios runtime. Developers compile or write scripts
against these interfaces, then run them through Helios.

## Python

Python scripts do not need a separately distributed SDK package. The selected
Helios Python host registers the public `helios` package and resolves its native
modules from the packaged `lib/py` directory. No Python shim files are deployed.
Python scripts do not compile against the native ABI and do not need rebuilding
when native functions are added.

Use only the canonical package:

```python
from helios import controls, inference, meter, overlay, vision
```

- `controls` reads input/report state and sends CV output.
- `inference` creates sessions and exposes only
  `wait_for_results_raw()` for object results.
- `vision` operates synchronously on the host's current borrowed BGR frame; `meter` normalizes caller-supplied meter/release points, measures a straight or configured curved path, derives timing metrics, and can render its ROI/path/metrics through Overlay. Their per-frame calls take no frame argument.
- `overlay` draws on the borrowed frame, the fuser output, or both.
- A worker may define `close()` for teardown; the host calls it once before
  releasing the worker.

The native `.pyd` files and runtime DLLs remain part of Helios. Do not copy or
redistribute them with a script. Importing `inference_core` in an unrelated
Python host is unsupported because the packaged host supplies the frame,
identity, and trigger contracts.

## C++

The complete developer kit for the installed Helios release is at:

```text
versions/<version>/sdk/
```

It includes the public headers, C++ and Python examples, and the matching API
references under `docs/`.

CppScripts accepts the stable Helios installation directory through
`HELIOS_INSTALL_DIR`, selects the highest valid numeric version using the same
rule as the launcher, and requires that version's complete SDK. The configured
path does not need to change after a Helios update.

Use `include/helios/HeliosCVSDK.h` as the aggregate include. It supplies the C++
script adapter, controller/keyboard/mouse input and report views, direct CV
output, overlay, inference, OCR, Vision, and Meter.

`HeliosInputABI.h` is the canonical C-compatible controller, keyboard, and
mouse memory layout. The aggregate C++ API and the hybrid-input helper both use
these exact types; there are no duplicate native layout definitions.

The actual DLL boundary is the narrow C contract in
`HeliosCVScriptABI.h`. A script exports three independent functions:

```text
helios_cv_script_create
helios_cv_script_process
helios_cv_script_destroy
```

`HELIOS_CV_SCRIPT(WorkerType)` defines those exports. Adding another script
function later does not change these exports or require a callback-table change.

The SDK wrappers resolve host exports and Helios runtime DLLs dynamically.
Third-party script DLLs do not link an InferenceCore or HeliosVision import
library and must not compile private runtime implementation files.

When Helios starts `CVCppWrapper.exe`, it applies the Python executable selected
under `Preferences > Python Core` to the child environment. The exact absolute
path is available as `HELIOS_PYTHON_EXECUTABLE`; the selected environment's
runtime directories lead `PATH`, and `VIRTUAL_ENV` or `CONDA_PREFIX` is set when
applicable. A process launched normally by the script therefore inherits the
selection. Prefer the explicit executable variable when constructing a process
command so paths containing spaces and non-ASCII characters remain unambiguous.
The wrapper admits only the selected runtime's base directory to its safe DLL
search set before loading the script, so a single native script DLL may link the
matching Python embedding library without copying the Python DLL beside it.

Build requirements are Windows x64, C++23, the Helios release's documented MSVC
toolset, OpenCV version, and dynamic runtime library settings. The C++ template,
`CVTest`, and their CMake project are included under `examples/cpp`. The Python
`CVTest` example is under `examples/python`.

## Specialized Native Headers

For a native helper loaded by a CV Python script:

```text
HeliosHybridInputSDK.hpp
```

This header maps the host-published controller, keyboard, and mouse blocks
directly using `HeliosInputABI.h`. Keep all SDK headers from the same release.

For direct low-level fuser integration:

```text
FuserOverlayTypes.hpp
FuserOverlayAccessors.hpp
```

Normal scripts should use `Helios::Overlay` or `helios.overlay`. Those APIs join
the host-owned per-frame command batch correctly. A low-level writer must use
the host-provided `HELIOS_FUSER_OVERLAY` mapping, obey latest-frame publication
semantics, check `isDrawingEnabled()`, and never retain slot pointers.

These specialized headers are included in the SDK so the installed kit is
self-contained. Normal CV C++ scripts should use `Helios::Controls` and
`Helios::Overlay`.

The fuser layout is guarded by compile-time size and offset assertions but has
no runtime compatibility shim. Its two headers must exactly match the target
Helios release.

## Lifetime Rules

- `Helios::Frame::image` is a borrowed, stride-aware, zero-copy BGR view valid
  only for the current `process()` call.
- `waitForResultsRaw()` and `wait_for_results_raw()` return borrowed result-ring
  views and wait indefinitely by default to preserve one-to-one frame
  synchronization. Consume them promptly and do not retain them across later
  frames. Passing a finite timeout explicitly opts into abandoning that frame's
  result.
- A hard TensorRT or DirectML runtime failure is surfaced as
  `INFCORE_ERROR_INFERENCE` with model name, backend/GPU, and the native cause;
  it is never returned as an ordinary zero-detection frame.
- Successful Python model loads print the manifest name, task, exact loaded
  dimensions, precision, and backend. Native callers can obtain the same text
  through `Session::loadedModelDescription()`.
- Vision and Meter result views belong to their prepared object and are
  overwritten by its next `find()`, `match()`, or `update()` call.
- Fuser slot and command storage belongs to the shared ring.

## Overlay Drawing Preference

`Preferences > Other > Video Display > Disable Overlay Drawing` is unchecked by
default. When enabled, the packaged script, inference, and OCR drawing paths
skip frame and fuser drawing while processing and result production continue.
Low-level fuser users must honor the shared `drawingEnabled` flag themselves.

## Exact ABI Contract

The Helios ABI and SDK are unversioned. They contain no ABI version constants,
version fields, version exports, generations, or version gates. Every callable
boundary is an independent named C export. A caller resolves the exact function
it needs and calls it when present.

`InferenceCoreApi.h` is the single function manifest used to declare the C API,
generate both native loaders, and verify every required InferenceCore export in
the release build. Vision, Meter, host, and CV-script calls use the same direct
function-by-name rule. No public boundary exchanges a function table.

The direct-call architecture and the finished Controls, Overlay, OCR,
Inference, host-function, and CV-script lifecycle surfaces are stable. Their
existing exports, function signatures, published structure layouts, field
meanings, and shared-memory strides do not change. New functionality is
additive: add another export and, when new data is required, introduce a new
structure for that function. An existing caller ignores functions it does not
know about. A caller that requires another function fails clearly only when
that specific function is absent. Adding a function is still the same ABI.

Vision and Meter are still under active development. Their function and data
contracts may be revised until those packages are explicitly declared finished.
That development status does not permit reintroducing function tables, ABI
numbers, or runtime compatibility gates. Once finalized, Vision and Meter use
the same additive-only rule as the finished surfaces.

Before executing a selected script, the packaged child host reopens it
read-only, verifies its SHA-256 against the identity authorized by Helios, and
keeps that handle open without write or delete sharing for the script lifetime.
This prevents the authorized path from being swapped between the application
check and child execution. It is startup-only work and adds nothing to the
per-frame path.

Native C++ scripts execute arbitrary code in the isolated CV C++ host process.
Helios treats running one as an explicit user decision, not as proof that its
developer is trusted. The installed SDK contains public declarations, examples,
and API documentation. It contains no private implementation sources, import
libraries, credentials, or runtime authorization material.

Detailed references are published on the Helios website:

- `InferenceCore_Native_Scripting_API.md`
- `InferenceCore_Python_API.md`
- `HELIOS_VISION_API.md`

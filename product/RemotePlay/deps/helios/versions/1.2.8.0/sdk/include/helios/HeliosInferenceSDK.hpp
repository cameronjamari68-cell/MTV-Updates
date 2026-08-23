#pragma once

#include "InferenceCore.h"
#include "HeliosHostFunctions.h"
#include "HeliosInferenceHostTriggerABI.h"
#include "HeliosInferenceOcrABI.h"
#include "HeliosInferenceResultsABI.h"
#include "HeliosInferenceSegmentationResultsABI.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Helios::Inference {

namespace detail {

// Current packaged runtime layout expected by this SDK:
//   <install-root>\Helios.exe
//   <install-root>\versions\<version>\lib\HeliosApp.exe
//   <install-root>\versions\<version>\lib\InferenceCoreTrt10.dll
//   <install-root>\versions\<version>\lib\InferenceCoreTrt11.dll
//   <install-root>\versions\<version>\lib\onnxruntime.dll
//   <install-root>\versions\<version>\lib\onnxruntime_providers_shared.dll
//   <install-root>\versions\<version>\lib\DirectML.dll
//   <install-root>\versions\<version>\lib\ocr\ppocrv6_small_rec\inference.onnx
//   <install-root>\versions\<version>\lib\ocr\ppocrv6_small_rec\inference.yml
//   <root>\lib\cv_cpp\CVCppWrapper.exe
//   or a selected Python interpreter with cvpython_host.pyd loaded.
// This loader requires the host to export the package root function, then
// loads the TensorRT-major-specific InferenceCore DLL from <root>\lib.

inline std::wstring wideFromUtf8(std::string_view value) {
    if (value.empty()
        || value.size() > static_cast<size_t>((std::numeric_limits<int>::max)())) {
        return {};
    }

    const int size = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0);
    if (size <= 0) {
        return {};
    }

    std::wstring wide(static_cast<size_t>(size), L'\0');
    if (MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(value.size()),
            wide.data(),
            size) != size) {
        return {};
    }
    return wide;
}

inline std::string utf8FromWide(std::wstring_view value) {
    if (value.empty()
        || value.size() > static_cast<size_t>((std::numeric_limits<int>::max)())) {
        return {};
    }

    const int size = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (size <= 0) {
        return {};
    }

    std::string utf8(static_cast<size_t>(size), '\0');
    if (WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(value.size()),
            utf8.data(),
            size,
            nullptr,
            nullptr) != size) {
        return {};
    }
    return utf8;
}

inline std::wstring joinPath(const std::wstring& left, const std::wstring& right) {
    if (left.empty()) {
        return right;
    }
    if (right.empty()) {
        return left;
    }
    if (left.back() == L'\\' || left.back() == L'/') {
        return left + right;
    }
    return left + L'\\' + right;
}

inline void setProcessEnvironment(const char* name, const char* value) {
    _putenv_s(name, value);
    SetEnvironmentVariableA(name, value);
}

inline HMODULE hostModuleWithExport(const char* exportName) {
    HMODULE hostModule = GetModuleHandleW(nullptr);
    if (hostModule && GetProcAddress(hostModule, exportName)) {
        return hostModule;
    }

    hostModule = GetModuleHandleW(L"cvpython_host.pyd");
    if (hostModule && GetProcAddress(hostModule, exportName)) {
        return hostModule;
    }

    return nullptr;
}

template <typename Function>
inline Function hostFunction(const char* name) {
    HMODULE hostModule = hostModuleWithExport(name);
    return hostModule
        ? reinterpret_cast<Function>(GetProcAddress(hostModule, name))
        : nullptr;
}

inline std::wstring heliosRootDirectory() {
    static const auto getRootPath =
        hostFunction<helios_host_get_root_path_fn>("helios_host_get_root_path");
    const char* hostRoot = getRootPath ? getRootPath() : nullptr;
    if (!hostRoot || !*hostRoot) {
        return {};
    }

    return wideFromUtf8(hostRoot);
}

inline std::wstring inferenceCorePath(std::string* error) {
    const std::wstring heliosRoot = heliosRootDirectory();
    if (heliosRoot.empty()) {
        return {};
    }

    const std::wstring libDirectory = joinPath(heliosRoot, L"lib");
    wchar_t tensorRtMajor[8] = {};
    const DWORD majorLength = GetEnvironmentVariableW(
        L"HELIOS_TENSORRT_MAJOR",
        tensorRtMajor,
        static_cast<DWORD>(sizeof(tensorRtMajor) / sizeof(tensorRtMajor[0])));
    if (majorLength == 0) {
        return joinPath(libDirectory, L"InferenceCoreTrt10.dll");
    }
    if (majorLength >= sizeof(tensorRtMajor) / sizeof(tensorRtMajor[0])) {
        if (error) {
            *error = "HELIOS_TENSORRT_MAJOR is invalid";
        }
        return {};
    }
    if (wcscmp(tensorRtMajor, L"10") == 0) {
        return joinPath(libDirectory, L"InferenceCoreTrt10.dll");
    }
    if (wcscmp(tensorRtMajor, L"11") == 0) {
        return joinPath(libDirectory, L"InferenceCoreTrt11.dll");
    }

    if (error) {
        *error = "Unsupported TensorRT major " + utf8FromWide(tensorRtMajor)
            + "; this release supports TensorRT 10 and 11";
    }
    return {};
}

inline void applyHostEnvironment() {
    const auto setHostString = [](const char* name, auto getter) {
        const char* value = getter ? getter() : "";
        setProcessEnvironment(name, value ? value : "");
    };

    static const auto getScriptType =
        hostFunction<helios_host_get_script_type_fn>("helios_host_get_script_type");
    static const auto getScriptHash =
        hostFunction<helios_host_get_script_hash_fn>("helios_host_get_script_hash");
    static const auto getExecutionGrant =
        hostFunction<helios_host_get_execution_grant_fn>("helios_host_get_execution_grant");
    static const auto getRootPath =
        hostFunction<helios_host_get_root_path_fn>("helios_host_get_root_path");
    static const auto getRingBufferName =
        hostFunction<helios_host_get_video_ring_buffer_name_fn>("helios_host_get_video_ring_buffer_name");
    static const auto getProcessId =
        hostFunction<helios_host_get_process_id_fn>("helios_host_get_process_id");

    setHostString("HELIOS_SCRIPT_TYPE", getScriptType);
    setHostString("HELIOS_SCRIPT_HASH", getScriptHash);
    setHostString("HELIOS_EXECUTION_GRANT", getExecutionGrant);
    setHostString("HELIOS_PATH", getRootPath);

    const char* ringBufferName = getRingBufferName ? getRingBufferName() : "";
    ringBufferName = ringBufferName ? ringBufferName : "";
    setProcessEnvironment("HELIOS_VIDEO_RING_BUFFER", ringBufferName);

    char processId[32] = {};
    if (getProcessId) {
        const uint32_t pid = getProcessId();
        if (pid != 0) {
            _snprintf_s(processId, sizeof(processId), _TRUNCATE, "%u", pid);
        }
    }
    setProcessEnvironment("HELIOS_PROCESS_ID", processId);
}

template <typename T>
inline bool loadProc(HMODULE module, const char* name, T* target) {
    if (!module || !name || !target) {
        return false;
    }
    *target = reinterpret_cast<T>(GetProcAddress(module, name));
    return *target != nullptr;
}

} // namespace detail

class Session;

class HostContext {
public:
    static uint64_t currentFrameTimestampNs() {
        static const auto function = detail::hostFunction<helios_host_get_current_frame_timestamp_ns_fn>(
            "helios_host_get_current_frame_timestamp_ns");
        return function ? function() : 0;
    }

    static uint32_t heliosProcessId() {
        static const auto function = detail::hostFunction<helios_host_get_process_id_fn>(
            "helios_host_get_process_id");
        return function ? function() : 0;
    }

    static std::string videoRingBufferName() {
        static const auto function = detail::hostFunction<helios_host_get_video_ring_buffer_name_fn>(
            "helios_host_get_video_ring_buffer_name");
        const char* name = function ? function() : nullptr;
        return name ? std::string(name) : std::string();
    }

private:
    friend class Session;

    static bool registerFrameTrigger(HeliosInferenceFrameTriggerCallback callback, void* userData) {
        HMODULE hostModule = detail::hostModuleWithExport("helios_register_inference_frame_trigger");
        if (!hostModule) {
            return false;
        }
        auto registerFn = reinterpret_cast<helios_register_inference_frame_trigger_fn>(
            GetProcAddress(hostModule, "helios_register_inference_frame_trigger"));
        return registerFn ? registerFn(callback, userData) : false;
    }

    static void unregisterFrameTrigger(HeliosInferenceFrameTriggerCallback callback, void* userData) {
        HMODULE hostModule = detail::hostModuleWithExport("helios_unregister_inference_frame_trigger");
        if (!hostModule) {
            return;
        }
        auto unregisterFn = reinterpret_cast<helios_unregister_inference_frame_trigger_fn>(
            GetProcAddress(hostModule, "helios_unregister_inference_frame_trigger"));
        if (unregisterFn) {
            unregisterFn(callback, userData);
        }
    }
};

class Library {
public:
    static Library& instance();

    Library(const Library&) = delete;
    Library& operator=(const Library&) = delete;

    bool ensureLoaded();
    std::string lastErrorString();
    void setLastError(std::string error);
    std::string listModelsJson();

#define DECLARE_MEMBER(return_type, name, signature) \
    using name##_fn = return_type (*) signature; \
    name##_fn name{nullptr};
    INFCORE_ABI_FUNCTIONS(DECLARE_MEMBER)
#undef DECLARE_MEMBER

private:
    Library() = default;
    ~Library() = default;
    bool bind(HMODULE module);

    HMODULE m_module{nullptr};
    inline static thread_local std::string s_lastError{};
};

class Session {
public:
    Session() = default;
    explicit Session(InferenceEngineHandle handle);
    Session(Session&& other) noexcept;
    Session& operator=(Session&& other) noexcept;
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    ~Session();

    // Backend selection follows the Helios Compute GPU preference supplied by the host.
    // The default host selection is auto; explicit GPU selections remain strict.
    static Session create(const char* uuid = nullptr);

    bool valid() const;
    InferenceEngineHandle nativeHandle() const;
    bool start(const char* ringBufferName = nullptr, uint32_t heliosPid = 0);
    void stop();
    void destroy();
    bool loadModel(const char* uuid);
    bool unloadModel();
    void pause();
    void resume();
    bool isPaused() const;
    // The default preserves one-to-one frame synchronization. A finite timeout
    // explicitly opts into abandoning that frame's result.
    const HeliosInferenceResultsBlock* waitForResultsRaw(uint32_t timeoutMs = INFINITE);
    const HeliosInferenceResultsArray* resultsArray() const;
    const HeliosInferenceSegmentationResultsBlock* segmentationResultsRaw() const;
    const HeliosInferenceSegmentationResultsArray* segmentationResultsArray() const;
    uint64_t lastRequestedSequence() const;
    std::string resultsName() const;
    std::string segmentationResultsName() const;
    std::string completeEventName() const;
    std::string loadedModelDescription() const;
    std::string runtimeError() const;

    void setConfidenceThreshold(float threshold, const int* classIds = nullptr, int classCount = 0);
    void setNmsThreshold(float threshold);
    bool setSegmentationMaskThreshold(float threshold);
    void setDrawSegmentationMasks(bool enabled, const int* classIds = nullptr, int classCount = 0);
    void setSegmentationMaskOpacity(uint8_t opacity);
    void setColorSegmentationMask(uint8_t b, uint8_t g, uint8_t r, const int* classIds = nullptr, int classCount = 0);
    void clearRoi();
    void setRoi(float centerX, float centerY, float widthPct, float aspectW, float aspectH);
    void setRoiPixels(float centerX, float centerY, int widthPx, int heightPx);
    void setRoiModelSize(float centerX, float centerY);
    bool setRoiRecommendedSize(float centerX, float centerY);
    bool setUseRecommendedRoi(bool enabled);
    void setDrawDetections(bool enabled, const int* classIds = nullptr, int classCount = 0);
    void setBboxThickness(int thickness);
    void setDrawOriginCross(bool enabled);
    void setDrawAnchorPoint(bool enabled);
    void setTargetDelayMs(int ms);
    void setDrawRoi(bool enabled);
    void setDrawBenchmarks(bool enabled);
    void setDrawConfidence(bool enabled);
    void setPolarOrigin(float x, float y);
    void setPolarOriginRing(float radiusPct, const uint32_t* colors, int colorCount, uint32_t segmentMs = 250, bool limitDetections = false);
    void setLimitDetectionsToPolarOriginRing(bool enabled);
    void setPolarOriginRingFocus(bool enabled);
    void setAnchorPoint(float pctX, float pctY, float flatX = 0.0f, float flatY = 0.0f, const int* classIds = nullptr, int classCount = 0);
    void setAnchorButtonProfiles(const int* buttonIndices, const float* anchors, int profileCount, const int* classIds, int classCount);
    void setPoseAnchorPoint(int mode = INFCORE_POSE_ANCHOR_NONE, const int* classIds = nullptr, int classCount = 0);
    bool setAnchorPrediction(const InfcoreAnchorPredictionConfig& config);
    bool recordAnchorPredictionMotion(uint64_t timestampNs, float velocityX, float velocityY);
    bool setOcrEnabled(bool enabled);
    bool setOcrSkipWhenDetections(bool enabled);
    void setDrawOcrRegion(bool enabled);
    bool clearOcrRegions();
    bool setOcrRegionPixels(uint32_t regionId, uint32_t x, uint32_t y, uint32_t width, uint32_t height);
    bool setOcrRegionNormalized(uint32_t regionId, float x1, float y1, float x2, float y2);
    bool setOcrRegionEnabled(uint32_t regionId, bool enabled);
    bool removeOcrRegion(uint32_t regionId);
    bool getOcrResult(uint32_t regionId, HeliosInferenceOcrResult& out) const;
    std::string ocrResultsName() const;
    std::string ocrCompleteEventName() const;
    bool setColorOcrRegion(uint8_t b, uint8_t g, uint8_t r);
    bool setColorOcrRegion(uint32_t regionId, uint8_t b, uint8_t g, uint8_t r);
    void setColorBbox(uint8_t b, uint8_t g, uint8_t r, const int* classIds = nullptr, int classCount = 0);
    void setColorOrigin(uint8_t b, uint8_t g, uint8_t r);
    void setColorAnchor(uint8_t b, uint8_t g, uint8_t r, const int* classIds = nullptr, int classCount = 0);
    void setColorRoi(uint8_t b, uint8_t g, uint8_t r);
    void setClassPriority(const int* data, int totalCount, const int* groupSizes, int numGroups);
    bool setClassPriorityRecommended();
    bool setUseRecommendedClassPriority(bool enabled);
    void clearClassOverrides(const int* classIds = nullptr, int classCount = 0);
    void setIgnoreRegion(float x1, float y1, float x2, float y2);
    void setDrawIgnoreRegion(bool enabled);
    void setColorIgnoreRegion(uint8_t b, uint8_t g, uint8_t r);
    void setSortMethod(int method);
    void setDrawOriginLine(bool enabled);
    void setColorOriginLine(uint8_t b, uint8_t g, uint8_t r);
    void setZoomEnabled(bool enabled);
    void setZoomButtonEnabled(bool enabled);
    void setZoomStep(float value);
    void setZoomMax(float value);
    void setZoomHoldMs(int value);
    void setDrawZoomedRoi(bool enabled);
    void setColorZoomedRoi(uint8_t b, uint8_t g, uint8_t r);
    void setZoomButtons(uint32_t mask);
    void setSearchButtons(uint32_t mask);
    void setSearchButtons(const int* buttonIndices, const int* states, int count);
    void setSearchButtons(const uint32_t* pressedMasks, const uint32_t* releasedMasks, int clauseCount);
    void setDrawKeypoints(bool enabled, const int* classIds = nullptr, int classCount = 0);
    void setDrawSkeleton(bool enabled, const int* classIds = nullptr, int classCount = 0);
    void setKeypointConfThreshold(float threshold);
    void setKeypointMask(uint32_t mask);
    void setColorKeypoints(uint8_t b, uint8_t g, uint8_t r, const int* classIds = nullptr, int classCount = 0);
    void setColorSkeleton(uint8_t b, uint8_t g, uint8_t r, const int* classIds = nullptr, int classCount = 0);
    void setKeypointRadius(int radius);

private:
    uint64_t triggerFrame(uint64_t sequence);
    static void HELIOS_INFERENCE_HOST_TRIGGER_CALL triggerHostFrame(void* userData, uint64_t sequence);
    bool registerHostFrameTrigger();
    void unregisterHostFrameTrigger();
    void detachResults();

    InferenceEngineHandle m_handle{nullptr};
    HANDLE m_resultsMapping{nullptr};
    const HeliosInferenceResultsArray* m_resultsArray{nullptr};
    HANDLE m_segmentationResultsMapping{nullptr};
    const HeliosInferenceSegmentationResultsArray* m_segmentationResultsArray{nullptr};
    HANDLE m_completeEvent{nullptr};
    uint64_t m_lastRequestedSequence{0};
    bool m_hostFrameTriggerRegistered{false};
    bool m_paused{false};
};

inline Library& Library::instance() {
    static Library library;
    return library;
}

inline bool Library::ensureLoaded() {
    detail::applyHostEnvironment();

    if (m_module) {
        return true;
    }

    std::string corePathError;
    const std::wstring dllPath = detail::inferenceCorePath(&corePathError);
    if (dllPath.empty()) {
        s_lastError = corePathError.empty()
            ? "Helios host does not export helios_host_get_root_path."
            : std::move(corePathError);
        return false;
    }

    SetDllDirectoryW(L"");
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_USER_DIRS);

    HMODULE module = LoadLibraryExW(dllPath.c_str(), nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_USER_DIRS);
    if (!module) {
        const DWORD error = GetLastError();
        s_lastError = "Failed to load the selected InferenceCore from "
            + detail::utf8FromWide(dllPath)
            + " (error "
            + std::to_string(static_cast<unsigned long>(error))
            + ")";
        return false;
    }

    if (!bind(module)) {
        FreeLibrary(module);
        return false;
    }

    m_module = module;
    s_lastError.clear();
    if (infcore_init() != INFCORE_OK) {
        s_lastError = lastErrorString();
        if (s_lastError.empty()) {
            s_lastError = "Failed to initialize InferenceCore";
        }
        FreeLibrary(module);
        m_module = nullptr;
        return false;
    }

    return true;
}

inline std::string Library::lastErrorString() {
    if (!s_lastError.empty()) {
        return s_lastError;
    }
    if (!m_module) {
        return {};
    }

    std::array<char, 8192> buffer{};
    const int size = infcore_get_debug_log(buffer.data(), static_cast<int>(buffer.size()));
    if (size > 0) {
        return std::string(buffer.data(), static_cast<size_t>(size));
    }
    return {};
}

inline void Library::setLastError(std::string error) {
    s_lastError = std::move(error);
}

inline std::string Library::listModelsJson() {
    if (!ensureLoaded()) {
        return {};
    }

    s_lastError.clear();
    const int requiredSize = infcore_list_models(nullptr, 0);
    if (requiredSize <= 0) {
        s_lastError = lastErrorString();
        if (s_lastError.empty()) {
            s_lastError = "Failed to determine the model list size";
        }
        return {};
    }

    std::vector<char> buffer(static_cast<size_t>(requiredSize) + 1, '\0');
    const int size = infcore_list_models(buffer.data(), static_cast<int>(buffer.size()));
    if (size <= 0) {
        s_lastError = lastErrorString();
        if (s_lastError.empty()) {
            s_lastError = "Failed to list available models";
        }
        return {};
    }
    s_lastError.clear();
    return std::string(buffer.data(), static_cast<size_t>(size));
}

inline bool Library::bind(HMODULE module) {
#define LOAD_MEMBER(return_type, name, signature) \
    if (!detail::loadProc(module, #name, &name)) { \
        s_lastError = std::string("Missing InferenceCore export: ") + #name; \
        return false; \
    }
    INFCORE_ABI_FUNCTIONS(LOAD_MEMBER)
#undef LOAD_MEMBER
    return true;
}

inline Session::Session(InferenceEngineHandle handle)
    : m_handle(handle) {}

inline Session::Session(Session&& other) noexcept {
    *this = std::move(other);
}

inline Session& Session::operator=(Session&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    const bool otherWasRegistered = other.m_hostFrameTriggerRegistered;
    if (otherWasRegistered) {
        HostContext::unregisterFrameTrigger(&Session::triggerHostFrame, &other);
        other.m_hostFrameTriggerRegistered = false;
    }

    destroy();
    m_handle = other.m_handle;
    m_resultsMapping = other.m_resultsMapping;
    m_resultsArray = other.m_resultsArray;
    m_segmentationResultsMapping = other.m_segmentationResultsMapping;
    m_segmentationResultsArray = other.m_segmentationResultsArray;
    m_completeEvent = other.m_completeEvent;
    m_lastRequestedSequence = other.m_lastRequestedSequence;
    m_hostFrameTriggerRegistered = false;
    m_paused = other.m_paused;
    other.m_handle = nullptr;
    other.m_resultsMapping = nullptr;
    other.m_resultsArray = nullptr;
    other.m_segmentationResultsMapping = nullptr;
    other.m_segmentationResultsArray = nullptr;
    other.m_completeEvent = nullptr;
    other.m_lastRequestedSequence = 0;
    other.m_paused = false;
    if (otherWasRegistered) {
        registerHostFrameTrigger();
    }
    return *this;
}

inline Session::~Session() {
    destroy();
}

inline Session Session::create(const char* uuid) {
    Library& library = Library::instance();
    if (!library.ensureLoaded()) {
        return {};
    }

    library.setLastError({});
    return Session(library.infcore_create_engine(uuid));
}

inline bool Session::valid() const {
    return m_handle != nullptr;
}

inline InferenceEngineHandle Session::nativeHandle() const {
    return m_handle;
}

inline bool Session::start(const char* ringBufferName, uint32_t heliosPid) {
    Library& library = Library::instance();
    if (!valid()) {
        library.setLastError("Cannot start an invalid inference session");
        return false;
    }

    if (!library.ensureLoaded()) {
        return false;
    }
    library.setLastError({});

    std::string hostRingBuffer;
    if (!ringBufferName || !*ringBufferName) {
        hostRingBuffer = HostContext::videoRingBufferName();
        ringBufferName = hostRingBuffer.empty() ? nullptr : hostRingBuffer.c_str();
    }

    if (heliosPid == 0) {
        heliosPid = HostContext::heliosProcessId();
    }

    if (library.infcore_start_inference(m_handle, ringBufferName, heliosPid) != INFCORE_OK) {
        return false;
    }
    m_paused = false;

    detachResults();

    std::array<char, 256> nameBuffer{};
    const int resultsNameSize = library.infcore_get_results_name(m_handle, nameBuffer.data(), static_cast<int>(nameBuffer.size()));
    if (resultsNameSize <= 0) {
        library.setLastError("InferenceCore did not provide a results mapping name");
        return false;
    }

    const std::wstring resultsName = detail::wideFromUtf8(std::string(nameBuffer.data(), static_cast<size_t>(resultsNameSize)));
    m_resultsMapping = OpenFileMappingW(FILE_MAP_READ, FALSE, resultsName.c_str());
    if (!m_resultsMapping) {
        const DWORD windowsError = GetLastError();
        library.setLastError(
            "Failed to open inference results mapping (Windows error "
                + std::to_string(windowsError) + ")");
        library.infcore_stop_inference(m_handle);
        return false;
    }

    m_resultsArray = static_cast<const HeliosInferenceResultsArray*>(
        MapViewOfFile(m_resultsMapping, FILE_MAP_READ, 0, 0, sizeof(HeliosInferenceResultsArray)));
    if (!m_resultsArray) {
        const DWORD windowsError = GetLastError();
        library.setLastError(
            "Failed to map inference results memory (Windows error "
                + std::to_string(windowsError) + ")");
        detachResults();
        library.infcore_stop_inference(m_handle);
        return false;
    }

    nameBuffer.fill('\0');
    const int segmentationResultsNameSize =
        library.infcore_get_segmentation_results_name(
            m_handle,
            nameBuffer.data(),
            static_cast<int>(nameBuffer.size()));
    if (segmentationResultsNameSize <= 0) {
        library.setLastError(
            "InferenceCore did not provide a segmentation results mapping name");
        detachResults();
        library.infcore_stop_inference(m_handle);
        return false;
    }

    const std::wstring segmentationResultsName = detail::wideFromUtf8(
        std::string(
            nameBuffer.data(),
            static_cast<size_t>(segmentationResultsNameSize)));
    m_segmentationResultsMapping = OpenFileMappingW(
        FILE_MAP_READ,
        FALSE,
        segmentationResultsName.c_str());
    if (!m_segmentationResultsMapping) {
        library.setLastError(
            "Failed to open segmentation results mapping (Windows error "
                + std::to_string(GetLastError()) + ")");
        detachResults();
        library.infcore_stop_inference(m_handle);
        return false;
    }

    m_segmentationResultsArray =
        static_cast<const HeliosInferenceSegmentationResultsArray*>(
            MapViewOfFile(
                m_segmentationResultsMapping,
                FILE_MAP_READ,
                0,
                0,
                sizeof(HeliosInferenceSegmentationResultsArray)));
    if (!m_segmentationResultsArray) {
        library.setLastError(
            "Failed to map segmentation results memory (Windows error "
                + std::to_string(GetLastError()) + ")");
        detachResults();
        library.infcore_stop_inference(m_handle);
        return false;
    }

    nameBuffer.fill('\0');
    const int eventNameSize = library.infcore_get_complete_event_name(m_handle, nameBuffer.data(), static_cast<int>(nameBuffer.size()));
    if (eventNameSize <= 0) {
        library.setLastError("InferenceCore did not provide a completion event name");
        detachResults();
        library.infcore_stop_inference(m_handle);
        return false;
    }

    const std::wstring eventName = detail::wideFromUtf8(std::string(nameBuffer.data(), static_cast<size_t>(eventNameSize)));
    m_completeEvent = OpenEventW(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, eventName.c_str());
    if (!m_completeEvent) {
        const DWORD windowsError = GetLastError();
        library.setLastError(
            "Failed to open inference completion event (Windows error "
                + std::to_string(windowsError) + ")");
        detachResults();
        library.infcore_stop_inference(m_handle);
        return false;
    }

    if (!registerHostFrameTrigger()) {
        library.setLastError("The host rejected the inference frame trigger registration");
        detachResults();
        library.infcore_stop_inference(m_handle);
        return false;
    }
    return true;
}

inline void Session::stop() {
    if (!valid()) {
        return;
    }
    unregisterHostFrameTrigger();
    Library::instance().infcore_stop_inference(m_handle);
    detachResults();
    m_lastRequestedSequence = 0;
    m_paused = false;
}

inline void Session::destroy() {
    if (!valid()) {
        return;
    }
    stop();
    Library::instance().infcore_destroy_engine(m_handle);
    m_handle = nullptr;
}

inline bool Session::loadModel(const char* uuid) {
    Library& library = Library::instance();
    if (!valid()) {
        library.setLastError("Cannot load a model into an invalid inference session");
        return false;
    }
    if (!library.ensureLoaded()) {
        return false;
    }
    library.setLastError({});
    m_lastRequestedSequence = 0;
    return library.infcore_load_model(m_handle, uuid) == INFCORE_OK;
}

inline bool Session::unloadModel() {
    Library& library = Library::instance();
    if (!valid()) {
        library.setLastError("Cannot unload a model from an invalid inference session");
        return false;
    }
    if (!library.ensureLoaded()) {
        return false;
    }
    library.setLastError({});
    m_lastRequestedSequence = 0;
    return library.infcore_unload_model(m_handle) == INFCORE_OK;
}

inline void Session::pause() {
    if (valid()) {
        m_paused = true;
        m_lastRequestedSequence = 0;
        Library::instance().infcore_pause_engine(m_handle);
    }
}

inline void Session::resume() {
    if (valid()) {
        Library::instance().infcore_resume_engine(m_handle);
        m_paused = false;
    }
}

inline bool Session::isPaused() const {
    return valid() && Library::instance().infcore_is_paused(m_handle) != 0;
}

inline uint64_t Session::triggerFrame(uint64_t sequence) {
    if (!valid() || m_paused || sequence == 0) {
        return 0;
    }
    Library::instance().infcore_trigger_frame(m_handle, sequence);
    m_lastRequestedSequence = sequence;
    return sequence;
}

inline void HELIOS_INFERENCE_HOST_TRIGGER_CALL Session::triggerHostFrame(void* userData, uint64_t sequence) {
    auto* session = static_cast<Session*>(userData);
    if (!session || !session->valid() || sequence == 0 || session->m_lastRequestedSequence == sequence) {
        return;
    }
    session->triggerFrame(sequence);
}

inline bool Session::registerHostFrameTrigger() {
    if (m_hostFrameTriggerRegistered || !valid()) {
        return m_hostFrameTriggerRegistered;
    }
    m_hostFrameTriggerRegistered = HostContext::registerFrameTrigger(&Session::triggerHostFrame, this);
    return m_hostFrameTriggerRegistered;
}

inline void Session::unregisterHostFrameTrigger() {
    if (!m_hostFrameTriggerRegistered) {
        return;
    }
    HostContext::unregisterFrameTrigger(&Session::triggerHostFrame, this);
    m_hostFrameTriggerRegistered = false;
}

inline const HeliosInferenceResultsBlock* Session::waitForResultsRaw(uint32_t timeoutMs) {
    if (!valid() || !m_resultsArray || !m_completeEvent) {
        return nullptr;
    }

    const uint64_t desiredSequence = m_lastRequestedSequence;
    if (desiredSequence == 0) {
        return nullptr;
    }

    const uint32_t slotIndex = static_cast<uint32_t>(desiredSequence % HELIOS_INFERENCE_RESULT_SLOT_COUNT);
    const HeliosInferenceResultsBlock* slot = &m_resultsArray->slots[slotIndex];
    const auto* completedSequence =
        reinterpret_cast<const std::atomic<uint64_t>*>(
            const_cast<const uint64_t*>(&slot->write_sequence));
    if (completedSequence->load(std::memory_order_acquire) != desiredSequence) {
        const bool waitIndefinitely = timeoutMs == INFINITE;
        const ULONGLONG deadlineMs = waitIndefinitely
            ? 0
            : (GetTickCount64() + static_cast<ULONGLONG>(timeoutMs));

        while (completedSequence->load(std::memory_order_acquire) != desiredSequence) {
            ResetEvent(m_completeEvent);
            if (completedSequence->load(std::memory_order_acquire) == desiredSequence) {
                break;
            }

            DWORD waitMs = INFINITE;
            if (!waitIndefinitely) {
                const ULONGLONG nowMs = GetTickCount64();
                if (nowMs >= deadlineMs) {
                    return nullptr;
                }
                const ULONGLONG remainingMs = deadlineMs - nowMs;
                waitMs = static_cast<DWORD>(remainingMs > static_cast<ULONGLONG>(MAXDWORD)
                    ? MAXDWORD
                    : remainingMs);
            }

            const DWORD waitResult = WaitForSingleObject(m_completeEvent, waitMs);
            if (waitResult != WAIT_OBJECT_0) {
                return nullptr;
            }
        }
    }
    if (slot->_reserved[
            HELIOS_INFERENCE_RESULT_RUNTIME_ERROR_INDEX] !=
        INFCORE_OK) {
        std::string error = runtimeError();
        Library::instance().setLastError(
            error.empty()
                ? "The inference backend stopped after a runtime failure"
                : std::move(error));
        return nullptr;
    }
    return slot;
}

inline const HeliosInferenceResultsArray* Session::resultsArray() const {
    return m_resultsArray;
}

inline const HeliosInferenceSegmentationResultsBlock*
Session::segmentationResultsRaw() const {
    if (!m_segmentationResultsArray || m_lastRequestedSequence == 0) {
        return nullptr;
    }
    const uint32_t slotIndex = static_cast<uint32_t>(
        m_lastRequestedSequence % HELIOS_INFERENCE_SEGMENTATION_SLOT_COUNT);
    const HeliosInferenceSegmentationResultsBlock* slot =
        &m_segmentationResultsArray->slots[slotIndex];
    return slot->write_sequence == m_lastRequestedSequence ? slot : nullptr;
}

inline const HeliosInferenceSegmentationResultsArray*
Session::segmentationResultsArray() const {
    return m_segmentationResultsArray;
}

inline uint64_t Session::lastRequestedSequence() const {
    return m_lastRequestedSequence;
}

inline std::string Session::resultsName() const {
    std::array<char, 256> buffer{};
    const int size = valid() ? Library::instance().infcore_get_results_name(m_handle, buffer.data(), static_cast<int>(buffer.size())) : 0;
    return size > 0 ? std::string(buffer.data(), static_cast<size_t>(size)) : std::string();
}

inline std::string Session::segmentationResultsName() const {
    std::array<char, 256> buffer{};
    const int size = valid()
        ? Library::instance().infcore_get_segmentation_results_name(
              m_handle,
              buffer.data(),
              static_cast<int>(buffer.size()))
        : 0;
    return size > 0
        ? std::string(buffer.data(), static_cast<size_t>(size))
        : std::string();
}

inline std::string Session::completeEventName() const {
    std::array<char, 256> buffer{};
    const int size = valid() ? Library::instance().infcore_get_complete_event_name(m_handle, buffer.data(), static_cast<int>(buffer.size())) : 0;
    return size > 0 ? std::string(buffer.data(), static_cast<size_t>(size)) : std::string();
}

inline std::string Session::loadedModelDescription() const {
    std::array<char, 1024> buffer{};
    const int size = valid()
        ? Library::instance().infcore_get_loaded_model_description(
              m_handle,
              buffer.data(),
              static_cast<int>(buffer.size()))
        : 0;
    return size > 0
        ? std::string(
              buffer.data(),
              static_cast<size_t>(size))
        : std::string();
}

inline std::string Session::runtimeError() const {
    std::array<char, 4096> buffer{};
    const int code = valid()
        ? Library::instance().infcore_get_runtime_error(
              m_handle,
              buffer.data(),
              static_cast<int>(buffer.size()))
        : INFCORE_OK;
    return code != INFCORE_OK
        ? std::string(buffer.data())
        : std::string();
}

inline void Session::detachResults() {
    if (m_resultsArray) {
        UnmapViewOfFile(m_resultsArray);
        m_resultsArray = nullptr;
    }
    if (m_resultsMapping) {
        CloseHandle(m_resultsMapping);
        m_resultsMapping = nullptr;
    }
    if (m_segmentationResultsArray) {
        UnmapViewOfFile(m_segmentationResultsArray);
        m_segmentationResultsArray = nullptr;
    }
    if (m_segmentationResultsMapping) {
        CloseHandle(m_segmentationResultsMapping);
        m_segmentationResultsMapping = nullptr;
    }
    if (m_completeEvent) {
        CloseHandle(m_completeEvent);
        m_completeEvent = nullptr;
    }
}

inline void Session::setConfidenceThreshold(float threshold, const int* classIds, int classCount) { Library::instance().infcore_set_confidence_for_classes(m_handle, threshold, classIds, classCount); }
inline void Session::setNmsThreshold(float threshold) { Library::instance().infcore_set_nms_threshold(m_handle, threshold); }
inline bool Session::setSegmentationMaskThreshold(float threshold) { return Library::instance().infcore_set_segmentation_mask_threshold(m_handle, threshold) == INFCORE_OK; }
inline void Session::setDrawSegmentationMasks(bool enabled, const int* classIds, int classCount) { Library::instance().infcore_set_draw_segmentation_masks_for_classes(m_handle, enabled ? 1 : 0, classIds, classCount); }
inline void Session::setSegmentationMaskOpacity(uint8_t opacity) { Library::instance().infcore_set_segmentation_mask_opacity(m_handle, opacity); }
inline void Session::setColorSegmentationMask(uint8_t b, uint8_t g, uint8_t r, const int* classIds, int classCount) { Library::instance().infcore_set_color_segmentation_mask_for_classes(m_handle, b, g, r, classIds, classCount); }
inline void Session::clearRoi() { Library::instance().infcore_clear_roi(m_handle); }
inline void Session::setRoi(float centerX, float centerY, float widthPct, float aspectW, float aspectH) { Library::instance().infcore_set_roi(m_handle, centerX, centerY, widthPct, aspectW, aspectH); }
inline void Session::setRoiPixels(float centerX, float centerY, int widthPx, int heightPx) { Library::instance().infcore_set_roi_pixels(m_handle, centerX, centerY, widthPx, heightPx); }
inline void Session::setRoiModelSize(float centerX, float centerY) { Library::instance().infcore_set_roi_model_size(m_handle, centerX, centerY); }
inline bool Session::setRoiRecommendedSize(float centerX, float centerY) { return Library::instance().infcore_set_roi_recommended_size(m_handle, centerX, centerY) != 0; }
inline bool Session::setUseRecommendedRoi(bool enabled) { return Library::instance().infcore_set_use_recommended_roi(m_handle, enabled ? 1 : 0) != 0; }
inline void Session::setDrawDetections(bool enabled, const int* classIds, int classCount) { Library::instance().infcore_set_draw_detections_for_classes(m_handle, enabled ? 1 : 0, classIds, classCount); }
inline void Session::setBboxThickness(int thickness) { Library::instance().infcore_set_bbox_thickness(m_handle, thickness); }
inline void Session::setDrawOriginCross(bool enabled) { Library::instance().infcore_set_draw_origin_cross(m_handle, enabled ? 1 : 0); }
inline void Session::setDrawAnchorPoint(bool enabled) { Library::instance().infcore_set_draw_anchor_point(m_handle, enabled ? 1 : 0); }
inline void Session::setTargetDelayMs(int ms) { Library::instance().infcore_set_target_delay_ms(m_handle, ms); }
inline void Session::setDrawRoi(bool enabled) { Library::instance().infcore_set_draw_roi(m_handle, enabled ? 1 : 0); }
inline void Session::setDrawBenchmarks(bool enabled) { Library::instance().infcore_set_draw_benchmarks(m_handle, enabled ? 1 : 0); }
inline void Session::setDrawConfidence(bool enabled) { Library::instance().infcore_set_draw_confidence(m_handle, enabled ? 1 : 0); }
inline void Session::setPolarOrigin(float x, float y) { Library::instance().infcore_set_polar_origin(m_handle, x, y); }
inline void Session::setPolarOriginRing(float radiusPct, const uint32_t* colors, int colorCount, uint32_t segmentMs, bool limitDetections) {
    Library::instance().infcore_set_polar_origin_ring(m_handle, radiusPct, colors, colorCount, segmentMs);
    Library::instance().infcore_set_limit_detections_to_polar_origin_ring(m_handle, limitDetections ? 1 : 0);
}
inline void Session::setLimitDetectionsToPolarOriginRing(bool enabled) { Library::instance().infcore_set_limit_detections_to_polar_origin_ring(m_handle, enabled ? 1 : 0); }
inline void Session::setPolarOriginRingFocus(bool enabled) { Library::instance().infcore_set_polar_origin_ring_focus(m_handle, enabled ? 1 : 0); }
inline void Session::setAnchorPoint(float pctX, float pctY, float flatX, float flatY, const int* classIds, int classCount) { Library::instance().infcore_set_anchor_point_for_classes(m_handle, pctX, pctY, flatX, flatY, classIds, classCount); }
inline void Session::setAnchorButtonProfiles(const int* buttonIndices, const float* anchors, int profileCount, const int* classIds, int classCount) { Library::instance().infcore_set_anchor_button_profiles_for_classes(m_handle, buttonIndices, anchors, profileCount, classIds, classCount); }
inline void Session::setPoseAnchorPoint(int mode, const int* classIds, int classCount) { Library::instance().infcore_set_pose_anchor_for_classes(m_handle, mode, classIds, classCount); }
inline bool Session::setAnchorPrediction(const InfcoreAnchorPredictionConfig& config) { return Library::instance().infcore_set_anchor_prediction(m_handle, &config) == INFCORE_OK; }
inline bool Session::recordAnchorPredictionMotion(uint64_t timestampNs, float velocityX, float velocityY) { return Library::instance().infcore_record_anchor_prediction_motion(m_handle, timestampNs, velocityX, velocityY) == INFCORE_OK; }
inline bool Session::setOcrEnabled(bool enabled) { return Library::instance().infcore_set_ocr_enabled(m_handle, enabled ? 1 : 0) == INFCORE_OK; }
inline bool Session::setOcrSkipWhenDetections(bool enabled) { return Library::instance().infcore_set_ocr_skip_when_detections(m_handle, enabled ? 1 : 0) == INFCORE_OK; }
inline void Session::setDrawOcrRegion(bool enabled) { Library::instance().infcore_set_draw_ocr_region(m_handle, enabled ? 1 : 0); }
inline bool Session::clearOcrRegions() { return Library::instance().infcore_clear_ocr_regions(m_handle) == INFCORE_OK; }
inline bool Session::setOcrRegionPixels(uint32_t regionId, uint32_t x, uint32_t y, uint32_t width, uint32_t height) { return Library::instance().infcore_set_ocr_region_pixels(m_handle, regionId, x, y, width, height) == INFCORE_OK; }
inline bool Session::setOcrRegionNormalized(uint32_t regionId, float x1, float y1, float x2, float y2) { return Library::instance().infcore_set_ocr_region_normalized(m_handle, regionId, x1, y1, x2, y2) == INFCORE_OK; }
inline bool Session::setOcrRegionEnabled(uint32_t regionId, bool enabled) { return Library::instance().infcore_set_ocr_region_enabled(m_handle, regionId, enabled ? 1 : 0) == INFCORE_OK; }
inline bool Session::removeOcrRegion(uint32_t regionId) { return Library::instance().infcore_remove_ocr_region(m_handle, regionId) == INFCORE_OK; }
inline bool Session::getOcrResult(uint32_t regionId, HeliosInferenceOcrResult& out) const { return valid() && Library::instance().infcore_get_ocr_result(m_handle, regionId, &out) != 0; }
inline std::string Session::ocrResultsName() const {
    std::array<char, 256> buffer{};
    const int size = valid() ? Library::instance().infcore_get_ocr_results_name(m_handle, buffer.data(), static_cast<int>(buffer.size())) : 0;
    return size > 0 ? std::string(buffer.data(), static_cast<size_t>(size)) : std::string();
}
inline std::string Session::ocrCompleteEventName() const {
    std::array<char, 256> buffer{};
    const int size = valid() ? Library::instance().infcore_get_ocr_complete_event_name(m_handle, buffer.data(), static_cast<int>(buffer.size())) : 0;
    return size > 0 ? std::string(buffer.data(), static_cast<size_t>(size)) : std::string();
}
inline bool Session::setColorOcrRegion(uint8_t b, uint8_t g, uint8_t r) { return Library::instance().infcore_set_color_ocr_region(m_handle, b, g, r) == INFCORE_OK; }
inline bool Session::setColorOcrRegion(uint32_t regionId, uint8_t b, uint8_t g, uint8_t r) { return Library::instance().infcore_set_color_ocr_region_id(m_handle, regionId, b, g, r) == INFCORE_OK; }
inline void Session::setColorBbox(uint8_t b, uint8_t g, uint8_t r, const int* classIds, int classCount) { Library::instance().infcore_set_color_bbox_for_classes(m_handle, b, g, r, classIds, classCount); }
inline void Session::setColorOrigin(uint8_t b, uint8_t g, uint8_t r) { Library::instance().infcore_set_color_origin(m_handle, b, g, r); }
inline void Session::setColorAnchor(uint8_t b, uint8_t g, uint8_t r, const int* classIds, int classCount) { Library::instance().infcore_set_color_anchor_for_classes(m_handle, b, g, r, classIds, classCount); }
inline void Session::setColorRoi(uint8_t b, uint8_t g, uint8_t r) { Library::instance().infcore_set_color_roi(m_handle, b, g, r); }
inline void Session::setClassPriority(const int* data, int totalCount, const int* groupSizes, int numGroups) { Library::instance().infcore_set_class_priority(m_handle, data, totalCount, groupSizes, numGroups); }
inline bool Session::setClassPriorityRecommended() { return Library::instance().infcore_set_class_priority_recommended(m_handle) != 0; }
inline bool Session::setUseRecommendedClassPriority(bool enabled) { return Library::instance().infcore_set_use_recommended_class_priority(m_handle, enabled ? 1 : 0) != 0; }
inline void Session::clearClassOverrides(const int* classIds, int classCount) { Library::instance().infcore_clear_class_overrides(m_handle, classIds, classCount); }
inline void Session::setIgnoreRegion(float x1, float y1, float x2, float y2) { Library::instance().infcore_set_ignore_region(m_handle, x1, y1, x2, y2); }
inline void Session::setDrawIgnoreRegion(bool enabled) { Library::instance().infcore_set_draw_ignore_region(m_handle, enabled ? 1 : 0); }
inline void Session::setColorIgnoreRegion(uint8_t b, uint8_t g, uint8_t r) { Library::instance().infcore_set_color_ignore_region(m_handle, b, g, r); }
inline void Session::setSortMethod(int method) { Library::instance().infcore_set_sort_method(m_handle, method); }
inline void Session::setDrawOriginLine(bool enabled) { Library::instance().infcore_set_draw_origin_line(m_handle, enabled ? 1 : 0); }
inline void Session::setColorOriginLine(uint8_t b, uint8_t g, uint8_t r) { Library::instance().infcore_set_color_origin_line(m_handle, b, g, r); }
inline void Session::setZoomEnabled(bool enabled) { Library::instance().infcore_set_zoom_enabled(m_handle, enabled ? 1 : 0); }
inline void Session::setZoomButtonEnabled(bool enabled) { Library::instance().infcore_set_zoom_button_enabled(m_handle, enabled ? 1 : 0); }
inline void Session::setZoomStep(float value) { Library::instance().infcore_set_zoom_step(m_handle, value); }
inline void Session::setZoomMax(float value) { Library::instance().infcore_set_zoom_max(m_handle, value); }
inline void Session::setZoomHoldMs(int value) { Library::instance().infcore_set_zoom_hold_ms(m_handle, value); }
inline void Session::setDrawZoomedRoi(bool enabled) { Library::instance().infcore_set_draw_zoomed_roi(m_handle, enabled ? 1 : 0); }
inline void Session::setColorZoomedRoi(uint8_t b, uint8_t g, uint8_t r) { Library::instance().infcore_set_color_zoomed_roi(m_handle, b, g, r); }
inline void Session::setZoomButtons(uint32_t mask) { Library::instance().infcore_set_zoom_buttons(m_handle, mask); }
inline void Session::setSearchButtons(uint32_t mask) { Library::instance().infcore_set_search_buttons(m_handle, mask); }
inline void Session::setSearchButtons(const int* buttonIndices, const int* states, int count) { Library::instance().infcore_set_search_button_conditions(m_handle, buttonIndices, states, count); }
inline void Session::setSearchButtons(const uint32_t* pressedMasks, const uint32_t* releasedMasks, int clauseCount) { Library::instance().infcore_set_search_clauses(m_handle, pressedMasks, releasedMasks, clauseCount); }
inline void Session::setDrawKeypoints(bool enabled, const int* classIds, int classCount) { Library::instance().infcore_set_draw_keypoints_for_classes(m_handle, enabled ? 1 : 0, classIds, classCount); }
inline void Session::setDrawSkeleton(bool enabled, const int* classIds, int classCount) { Library::instance().infcore_set_draw_skeleton_for_classes(m_handle, enabled ? 1 : 0, classIds, classCount); }
inline void Session::setKeypointConfThreshold(float threshold) { Library::instance().infcore_set_keypoint_conf_threshold(m_handle, threshold); }
inline void Session::setKeypointMask(uint32_t mask) { Library::instance().infcore_set_keypoint_mask(m_handle, mask); }
inline void Session::setColorKeypoints(uint8_t b, uint8_t g, uint8_t r, const int* classIds, int classCount) { Library::instance().infcore_set_color_keypoints_for_classes(m_handle, b, g, r, classIds, classCount); }
inline void Session::setColorSkeleton(uint8_t b, uint8_t g, uint8_t r, const int* classIds, int classCount) { Library::instance().infcore_set_color_skeleton_for_classes(m_handle, b, g, r, classIds, classCount); }
inline void Session::setKeypointRadius(int radius) { Library::instance().infcore_set_keypoint_radius(m_handle, radius); }

} // namespace Helios::Inference

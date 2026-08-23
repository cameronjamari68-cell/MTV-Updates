#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "HeliosMeter.h"
#include "HeliosVision.h"

#include <opencv2/core.hpp>
#include <windows.h>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>

namespace Helios::Vision {

using Bgr8 = HeliosVisionBgr8;
using BgrRange = HeliosVisionBgrRange;
using ColorStats = HeliosVisionColorStats;
using Contour = HeliosVisionContour;
using FrameView = HeliosVisionFrameView;
using NormResult = HeliosVisionNormResult;
using PixelRange = HeliosVisionRangeU32;
using PointF64 = HeliosVisionPointF64;
using PointI32 = HeliosVisionPointI32;
using RectI32 = HeliosVisionRectI32;
using Rgba8 = HeliosVisionRgba8;
using TemplateResult = HeliosVisionTemplateResult;

enum class Connectivity : uint32_t {
    Four = HELIOS_VISION_CONNECTIVITY_4,
    Eight = HELIOS_VISION_CONNECTIVITY_8,
};

enum class Grouping : uint32_t {
    Connected = HELIOS_VISION_GROUP_CONNECTED,
    AllInRoi = HELIOS_VISION_GROUP_ALL_IN_ROI,
};

enum class NormMethod : uint32_t {
    L1 = HELIOS_VISION_NORM_L1,
    L2Squared = HELIOS_VISION_NORM_L2_SQUARED,
};

enum class TemplateMethod : uint32_t {
    Sad = HELIOS_VISION_TEMPLATE_SAD,
    Ssd = HELIOS_VISION_TEMPLATE_SSD,
    Ncc = HELIOS_VISION_TEMPLATE_NCC,
};

constexpr Bgr8 bgr(uint8_t blue, uint8_t green, uint8_t red) noexcept {
    return {blue, green, red, 0};
}

constexpr BgrRange bgrRange(
    Bgr8 low,
    Bgr8 high) noexcept {
    BgrRange range{};
    range.low = low;
    range.high = high;
    return range;
}

constexpr Rgba8 rgba(uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha = 255) noexcept {
    return {red, green, blue, alpha};
}

struct ShapeFilter {
    // The mask is sampled into the native 32x32 shape model during construction.
    const cv::Mat* mask{nullptr};
    uint32_t toleranceCells{1};
    float minimumScore{0.72f};
};

struct ContourFinderConfig {
    std::span<const BgrRange> colors{};
    RectI32 roi{};
    PixelRange width{};
    PixelRange height{};
    uint32_t referenceWidth{1920};
    uint32_t referenceHeight{1080};
    Connectivity connectivity{Connectivity::Eight};
    Grouping grouping{Grouping::Connected};
    uint32_t maxResults{64};
    ShapeFilter shape{};
};

struct ContourResultsView {
    // Borrowed from the finder; replaced by its next find or setColors call.
    std::span<const Contour> contours{};
    uint32_t flags{HELIOS_VISION_RESULT_FLAG_NONE};
    uint64_t generation{0};

    bool truncated() const noexcept {
        return (flags & HELIOS_VISION_RESULT_FLAG_TRUNCATED) != 0;
    }
};

class Error final : public std::runtime_error {
public:
    Error(int32_t status, const char* operation, const char* message)
        : std::runtime_error(
            std::string(operation) + ": " + (message && *message ? message : "Vision operation failed")),
          m_status(status) {}

    int32_t status() const noexcept {
        return m_status;
    }

private:
    int32_t m_status;
};

namespace detail {

inline std::wstring moduleDirectory(HMODULE module) {
    std::array<wchar_t, MAX_PATH> path{};
    const DWORD length = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size()) {
        return {};
    }

    const std::wstring fullPath(path.data(), static_cast<size_t>(length));
    const size_t slash = fullPath.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring{} : fullPath.substr(0, slash);
}

inline std::wstring parentDirectory(const std::wstring& path) {
    const size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring{} : path.substr(0, slash);
}

inline std::wstring filename(const std::wstring& path) {
    const size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? path : path.substr(slash + 1);
}

inline std::wstring runtimePath() {
    if (HMODULE pythonHost = GetModuleHandleW(L"cvpython_host.pyd")) {
        const std::wstring pyDirectory = moduleDirectory(pythonHost);
        if (filename(pyDirectory) == L"py") {
            return parentDirectory(pyDirectory) + L"\\HeliosVision.dll";
        }
        return {};
    }

    const std::wstring hostDirectory = moduleDirectory(GetModuleHandleW(nullptr));
    if (filename(hostDirectory) == L"cv_cpp") {
        return parentDirectory(hostDirectory) + L"\\HeliosVision.dll";
    }
    if (filename(hostDirectory) == L"lib") {
        return hostDirectory + L"\\HeliosVision.dll";
    }
    return {};
}

struct VisionFunctions {
    decltype(&helios_vision_status_message) status_message{};
    decltype(&helios_vision_contour_finder_create) contour_finder_create{};
    decltype(&helios_vision_contour_finder_destroy) contour_finder_destroy{};
    decltype(&helios_vision_contour_finder_set_colors) contour_finder_set_colors{};
    decltype(&helios_vision_contour_finder_find) contour_finder_find{};
    decltype(&helios_vision_contour_finder_color_stats) contour_finder_color_stats{};
    decltype(&helios_vision_norm_matcher_create) norm_matcher_create{};
    decltype(&helios_vision_norm_matcher_destroy) norm_matcher_destroy{};
    decltype(&helios_vision_norm_matcher_match) norm_matcher_match{};
    decltype(&helios_vision_template_matcher_create) template_matcher_create{};
    decltype(&helios_vision_template_matcher_destroy) template_matcher_destroy{};
    decltype(&helios_vision_template_matcher_find) template_matcher_find{};
};

struct MeterFunctions {
    decltype(&helios_meter_create) meter_create{};
    decltype(&helios_meter_destroy) meter_destroy{};
    decltype(&helios_meter_update) meter_update{};
    decltype(&helios_meter_state) meter_state{};
    decltype(&helios_meter_set_path_settings) meter_set_path_settings{};
    decltype(&helios_meter_path_settings) meter_path_settings{};
    decltype(&helios_meter_set_visual_settings) meter_set_visual_settings{};
    decltype(&helios_meter_visual_settings) meter_visual_settings{};
    decltype(&helios_meter_reset) meter_reset{};
};

template <typename Function>
inline bool bindFunction(HMODULE module, Function& target, const char* name) noexcept {
    target = reinterpret_cast<Function>(GetProcAddress(module, name));
    return target != nullptr;
}

class Library {
public:
    static Library& instance() {
        static Library library;
        return library;
    }

    Library(const Library&) = delete;
    Library& operator=(const Library&) = delete;

    const VisionFunctions* visionApi() const noexcept {
        return m_ready ? &m_visionApi : nullptr;
    }

    const MeterFunctions* meterApi() const noexcept {
        return m_ready ? &m_meterApi : nullptr;
    }

    const std::string& error() const noexcept {
        return m_error;
    }

private:
    Library() {
        const std::wstring path = runtimePath();
        if (path.empty()) {
            m_error = "HeliosVision.dll cannot be resolved outside a packaged Helios script host.";
            return;
        }

        m_module = LoadLibraryExW(
            path.c_str(),
            nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!m_module) {
            m_error = "HeliosVision.dll could not be loaded.";
            return;
        }

        if (!bindFunction(m_module, m_visionApi.status_message, "helios_vision_status_message")
            || !bindFunction(m_module, m_visionApi.contour_finder_create, "helios_vision_contour_finder_create")
            || !bindFunction(m_module, m_visionApi.contour_finder_destroy, "helios_vision_contour_finder_destroy")
            || !bindFunction(m_module, m_visionApi.contour_finder_set_colors, "helios_vision_contour_finder_set_colors")
            || !bindFunction(m_module, m_visionApi.contour_finder_find, "helios_vision_contour_finder_find")
            || !bindFunction(m_module, m_visionApi.contour_finder_color_stats, "helios_vision_contour_finder_color_stats")
            || !bindFunction(m_module, m_visionApi.norm_matcher_create, "helios_vision_norm_matcher_create")
            || !bindFunction(m_module, m_visionApi.norm_matcher_destroy, "helios_vision_norm_matcher_destroy")
            || !bindFunction(m_module, m_visionApi.norm_matcher_match, "helios_vision_norm_matcher_match")
            || !bindFunction(m_module, m_visionApi.template_matcher_create, "helios_vision_template_matcher_create")
            || !bindFunction(m_module, m_visionApi.template_matcher_destroy, "helios_vision_template_matcher_destroy")
            || !bindFunction(m_module, m_visionApi.template_matcher_find, "helios_vision_template_matcher_find")
            || !bindFunction(m_module, m_meterApi.meter_create, "helios_meter_create")
            || !bindFunction(m_module, m_meterApi.meter_destroy, "helios_meter_destroy")
            || !bindFunction(m_module, m_meterApi.meter_update, "helios_meter_update")
            || !bindFunction(m_module, m_meterApi.meter_state, "helios_meter_state")
            || !bindFunction(m_module, m_meterApi.meter_set_path_settings, "helios_meter_set_path_settings")
            || !bindFunction(m_module, m_meterApi.meter_path_settings, "helios_meter_path_settings")
            || !bindFunction(m_module, m_meterApi.meter_set_visual_settings, "helios_meter_set_visual_settings")
            || !bindFunction(m_module, m_meterApi.meter_visual_settings, "helios_meter_visual_settings")
            || !bindFunction(m_module, m_meterApi.meter_reset, "helios_meter_reset")) {
            m_error = "HeliosVision.dll is missing a required function.";
            return;
        }

        m_ready = true;
    }

    ~Library() {
        if (m_module) {
            FreeLibrary(m_module);
        }
    }

    HMODULE m_module{nullptr};
    VisionFunctions m_visionApi{};
    MeterFunctions m_meterApi{};
    std::string m_error;
    bool m_ready{false};
};

inline const VisionFunctions* requireVisionApi() {
    const auto& library = Library::instance();
    const VisionFunctions* api = library.visionApi();
    if (!api) {
        throw std::runtime_error(library.error());
    }
    return api;
}

inline const MeterFunctions* requireMeterApi() {
    const auto& library = Library::instance();
    const MeterFunctions* api = library.meterApi();
    if (!api) {
        throw std::runtime_error(library.error());
    }
    return api;
}

inline void checkStatus(const VisionFunctions* api, int32_t status, const char* operation) {
    if (status != HELIOS_VISION_STATUS_OK) {
        throw Error(status, operation, api->status_message(status));
    }
}

inline uint64_t imageDataSize(const cv::Mat& image, uint32_t rowBytes) {
    const uint64_t required = static_cast<uint64_t>(image.rows - 1) * image.step[0] + rowBytes;
    if (!image.dataend || static_cast<uint64_t>(image.dataend - image.data) < required) {
        throw std::invalid_argument("OpenCV image storage is smaller than its dimensions and stride.");
    }
    return required;
}

inline FrameView makeFrameView(
    const cv::Mat& frame,
    uint64_t frameSequence,
    uint64_t timestampNs) {
    if (frame.dims != 2 || frame.empty() || frame.type() != CV_8UC3) {
        throw std::invalid_argument("Vision frames must be non-empty two-dimensional CV_8UC3 BGR images.");
    }
    const uint64_t rowBytes = static_cast<uint64_t>(frame.cols) * 3u;
    if (frame.step[0] < rowBytes || frame.step[0] > std::numeric_limits<uint32_t>::max()) {
        throw std::invalid_argument("Vision frame stride is invalid or exceeds the ABI limit.");
    }

    FrameView view{};
    view.struct_size = sizeof(view);
    view.data = frame.data;
    view.data_size = imageDataSize(frame, static_cast<uint32_t>(rowBytes));
    view.width = static_cast<uint32_t>(frame.cols);
    view.height = static_cast<uint32_t>(frame.rows);
    view.stride = static_cast<uint32_t>(frame.step[0]);
    view.pixel_format = HELIOS_VISION_PIXEL_FORMAT_BGR8;
    view.frame_sequence = frameSequence;
    view.timestamp_ns = timestampNs;
    return view;
}

inline HeliosVisionShapeView makeShapeView(const ShapeFilter& filter) {
    HeliosVisionShapeView view{};
    view.struct_size = sizeof(view);
    view.tolerance_cells = filter.toleranceCells;
    view.minimum_score = filter.minimumScore;
    if (!filter.mask) {
        return view;
    }

    const cv::Mat& mask = *filter.mask;
    if (mask.dims != 2 || mask.empty() || mask.type() != CV_8UC1) {
        throw std::invalid_argument("Contour shape masks must be non-empty two-dimensional CV_8UC1 images.");
    }
    if (mask.step[0] < static_cast<size_t>(mask.cols)
        || mask.step[0] > std::numeric_limits<uint32_t>::max()) {
        throw std::invalid_argument("Contour shape mask stride is invalid or exceeds the ABI limit.");
    }
    view.data = mask.data;
    view.data_size = imageDataSize(mask, static_cast<uint32_t>(mask.cols));
    view.width = static_cast<uint32_t>(mask.cols);
    view.height = static_cast<uint32_t>(mask.rows);
    view.stride = static_cast<uint32_t>(mask.step[0]);
    return view;
}

inline HeliosVisionContourFinderConfig makeContourConfig(const ContourFinderConfig& config) {
    if (config.colors.size() > HELIOS_VISION_MAX_COLOR_RANGES) {
        throw std::invalid_argument("A contour finder supports at most 32 BGR ranges.");
    }

    HeliosVisionContourFinderConfig native{};
    native.struct_size = sizeof(native);
    native.roi = config.roi;
    native.reference_width = config.referenceWidth;
    native.reference_height = config.referenceHeight;
    native.width = config.width;
    native.height = config.height;
    native.colors = config.colors.data();
    native.color_count = static_cast<uint32_t>(config.colors.size());
    native.connectivity = static_cast<uint32_t>(config.connectivity);
    native.grouping = static_cast<uint32_t>(config.grouping);
    native.max_results = config.maxResults;
    native.shape = makeShapeView(config.shape);
    return native;
}

} // namespace detail

inline FrameView makeFrameView(
    const cv::Mat& frame,
    uint64_t frameSequence,
    uint64_t timestampNs) {
    return detail::makeFrameView(frame, frameSequence, timestampNs);
}

inline FrameView makeFrameView(const cv::Mat& frame) {
    return detail::makeFrameView(frame, 0, 0);
}

class ContourFinder final {
public:
    explicit ContourFinder(const ContourFinderConfig& config)
        : m_api(detail::requireVisionApi()),
          m_colorCount(static_cast<uint32_t>(config.colors.size())) {
        const HeliosVisionContourFinderConfig native = detail::makeContourConfig(config);
        detail::checkStatus(
            m_api,
            m_api->contour_finder_create(&native, &m_handle),
            "ContourFinder construction");
    }

    ~ContourFinder() {
        destroy();
    }

    ContourFinder(ContourFinder&& other) noexcept
        : m_api(std::exchange(other.m_api, nullptr)),
          m_handle(std::exchange(other.m_handle, nullptr)),
          m_results(std::exchange(other.m_results, {})),
          m_colorCount(std::exchange(other.m_colorCount, 0)) {}

    ContourFinder& operator=(ContourFinder&& other) noexcept {
        if (this != &other) {
            destroy();
            m_api = std::exchange(other.m_api, nullptr);
            m_handle = std::exchange(other.m_handle, nullptr);
            m_results = std::exchange(other.m_results, {});
            m_colorCount = std::exchange(other.m_colorCount, 0);
        }
        return *this;
    }

    ContourFinder(const ContourFinder&) = delete;
    ContourFinder& operator=(const ContourFinder&) = delete;

    ContourResultsView find() {
        detail::checkStatus(
            m_api,
            m_api->contour_finder_find(m_handle, &m_results),
            "ContourFinder::find");
        return results();
    }

    ContourResultsView results() const noexcept {
        const std::span<const Contour> values = m_results.count == 0
            ? std::span<const Contour>{}
            : std::span<const Contour>{m_results.data, m_results.count};
        return {values, m_results.flags, m_results.generation};
    }

    void setColors(std::span<const BgrRange> colors) {
        if (colors.size() != m_colorCount) {
            throw std::invalid_argument("Contour color updates must preserve the configured color count.");
        }
        detail::checkStatus(
            m_api,
            m_api->contour_finder_set_colors(
                m_handle,
                colors.data(),
                static_cast<uint32_t>(colors.size())),
            "ContourFinder::setColors");
        m_results = {};
    }

    ColorStats colorStats(uint32_t resultIndex, uint32_t colorIndex) const {
        ColorStats value{};
        detail::checkStatus(
            m_api,
            m_api->contour_finder_color_stats(m_handle, resultIndex, colorIndex, &value),
            "ContourFinder::colorStats");
        return value;
    }

    uint32_t colorCount() const noexcept {
        return m_colorCount;
    }

    bool valid() const noexcept {
        return m_handle != nullptr;
    }

    HeliosVisionContourFinder* nativeHandle() const noexcept {
        return m_handle;
    }

private:
    void destroy() noexcept {
        if (m_handle) {
            m_api->contour_finder_destroy(m_handle);
            m_handle = nullptr;
        }
    }

    const detail::VisionFunctions* m_api{nullptr};
    HeliosVisionContourFinder* m_handle{nullptr};
    HeliosVisionContourResults m_results{};
    uint32_t m_colorCount{0};
};

class NormMatcher final {
public:
    explicit NormMatcher(
        const cv::Mat& reference,
        const RectI32* referenceRoi = nullptr,
        NormMethod method = NormMethod::L1)
        : NormMatcher(makeFrameView(reference), referenceRoi, method) {}

    explicit NormMatcher(
        const FrameView& reference,
        const RectI32* referenceRoi = nullptr,
        NormMethod method = NormMethod::L1)
        : m_api(detail::requireVisionApi()) {
        detail::checkStatus(
            m_api,
            m_api->norm_matcher_create(
                &reference,
                referenceRoi,
                static_cast<uint32_t>(method),
                &m_handle),
            "NormMatcher construction");
    }

    ~NormMatcher() {
        destroy();
    }

    NormMatcher(NormMatcher&& other) noexcept
        : m_api(std::exchange(other.m_api, nullptr)),
          m_handle(std::exchange(other.m_handle, nullptr)) {}

    NormMatcher& operator=(NormMatcher&& other) noexcept {
        if (this != &other) {
            destroy();
            m_api = std::exchange(other.m_api, nullptr);
            m_handle = std::exchange(other.m_handle, nullptr);
        }
        return *this;
    }

    NormMatcher(const NormMatcher&) = delete;
    NormMatcher& operator=(const NormMatcher&) = delete;

    NormResult match(const RectI32* roi = nullptr) const {
        NormResult result{};
        detail::checkStatus(
            m_api,
            m_api->norm_matcher_match(m_handle, roi, &result),
            "NormMatcher::match");
        return result;
    }

    bool valid() const noexcept {
        return m_handle != nullptr;
    }

    HeliosVisionNormMatcher* nativeHandle() const noexcept {
        return m_handle;
    }

private:
    void destroy() noexcept {
        if (m_handle) {
            m_api->norm_matcher_destroy(m_handle);
            m_handle = nullptr;
        }
    }

    const detail::VisionFunctions* m_api{nullptr};
    HeliosVisionNormMatcher* m_handle{nullptr};
};

class TemplateMatcher final {
public:
    explicit TemplateMatcher(
        const cv::Mat& image,
        const RectI32* templateRoi = nullptr,
        TemplateMethod method = TemplateMethod::Sad)
        : TemplateMatcher(makeFrameView(image), templateRoi, method) {}

    explicit TemplateMatcher(
        const FrameView& image,
        const RectI32* templateRoi = nullptr,
        TemplateMethod method = TemplateMethod::Sad)
        : m_api(detail::requireVisionApi()) {
        detail::checkStatus(
            m_api,
            m_api->template_matcher_create(
                &image,
                templateRoi,
                static_cast<uint32_t>(method),
                &m_handle),
            "TemplateMatcher construction");
    }

    ~TemplateMatcher() {
        destroy();
    }

    TemplateMatcher(TemplateMatcher&& other) noexcept
        : m_api(std::exchange(other.m_api, nullptr)),
          m_handle(std::exchange(other.m_handle, nullptr)) {}

    TemplateMatcher& operator=(TemplateMatcher&& other) noexcept {
        if (this != &other) {
            destroy();
            m_api = std::exchange(other.m_api, nullptr);
            m_handle = std::exchange(other.m_handle, nullptr);
        }
        return *this;
    }

    TemplateMatcher(const TemplateMatcher&) = delete;
    TemplateMatcher& operator=(const TemplateMatcher&) = delete;

    TemplateResult find(const RectI32* searchRoi = nullptr) const {
        TemplateResult result{};
        detail::checkStatus(
            m_api,
            m_api->template_matcher_find(m_handle, searchRoi, &result),
            "TemplateMatcher::find");
        return result;
    }

    bool valid() const noexcept {
        return m_handle != nullptr;
    }

    HeliosVisionTemplateMatcher* nativeHandle() const noexcept {
        return m_handle;
    }

private:
    void destroy() noexcept {
        if (m_handle) {
            m_api->template_matcher_destroy(m_handle);
            m_handle = nullptr;
        }
    }

    const detail::VisionFunctions* m_api{nullptr};
    HeliosVisionTemplateMatcher* m_handle{nullptr};
};

} // namespace Helios::Vision

namespace Helios::Meter {

using State = HeliosMeterState;
inline constexpr uint32_t DesignWidth = HELIOS_METER_DESIGN_WIDTH;
inline constexpr uint32_t DesignHeight = HELIOS_METER_DESIGN_HEIGHT;

enum class PathAlgorithm : uint32_t {
    Straight = HELIOS_METER_PATH_STRAIGHT,
    QuadraticBezier = HELIOS_METER_PATH_QUADRATIC_BEZIER,
};

enum class VisualTarget : uint32_t {
    Frame = HELIOS_METER_VISUAL_TARGET_FRAME,
    Fuser = HELIOS_METER_VISUAL_TARGET_FUSER,
    Both = HELIOS_METER_VISUAL_TARGET_BOTH,
};

struct PathSettings {
    PathAlgorithm algorithm{PathAlgorithm::Straight};
    double curvature{0.25};
    uint32_t segments{16};
};

struct VisualSettings {
    bool enabled{true};
    bool showBbox{true};
    bool showPath{true};
    bool showDistance{true};
    bool showSpeed{true};
    bool showTimeToRelease{true};
    bool showElapsedTime{true};
    VisualTarget target{VisualTarget::Both};
    Vision::Rgba8 bboxColor{104, 244, 255, 255};
    Vision::Rgba8 pathColor{104, 244, 255, 255};
    Vision::Rgba8 metricsColor{104, 244, 255, 255};
    uint32_t bboxThickness{1};
    uint32_t pathThickness{1};
    uint32_t textScale{2};
    uint32_t textGap{5};
};

inline HeliosMeterPathSettings nativePathSettings(const PathSettings& settings) noexcept {
    return {
        static_cast<uint32_t>(settings.algorithm),
        settings.segments,
        settings.curvature,
    };
}

inline PathSettings pathSettings(const HeliosMeterPathSettings& settings) noexcept {
    return {
        static_cast<PathAlgorithm>(settings.algorithm),
        settings.curvature,
        settings.segments,
    };
}

inline HeliosMeterVisualSettings nativeVisualSettings(const VisualSettings& settings) noexcept {
    return {
        settings.enabled ? 1u : 0u,
        settings.showBbox ? 1u : 0u,
        settings.showPath ? 1u : 0u,
        settings.showDistance ? 1u : 0u,
        settings.showSpeed ? 1u : 0u,
        settings.showTimeToRelease ? 1u : 0u,
        settings.showElapsedTime ? 1u : 0u,
        static_cast<uint32_t>(settings.target),
        settings.bboxColor,
        settings.pathColor,
        settings.metricsColor,
        settings.bboxThickness,
        settings.pathThickness,
        settings.textScale,
        settings.textGap,
    };
}

inline VisualSettings visualSettings(const HeliosMeterVisualSettings& settings) noexcept {
    return {
        settings.enabled != 0,
        settings.show_bbox != 0,
        settings.show_path != 0,
        settings.show_distance != 0,
        settings.show_speed != 0,
        settings.show_time_to_release != 0,
        settings.show_elapsed_time != 0,
        static_cast<VisualTarget>(settings.target),
        settings.bbox_color,
        settings.path_color,
        settings.metrics_color,
        settings.bbox_thickness,
        settings.path_thickness,
        settings.text_scale,
        settings.text_gap,
    };
}

class Meter final {
public:
    Meter()
        : m_visionApi(Vision::detail::requireVisionApi()),
          m_api(Vision::detail::requireMeterApi()) {
        Vision::detail::checkStatus(
            m_visionApi,
            m_api->meter_create(&m_handle),
            "Meter construction");
        m_state = m_api->meter_state(m_handle);
        if (!m_state) {
            m_api->meter_destroy(m_handle);
            m_handle = nullptr;
            throw std::runtime_error("Meter construction returned no state.");
        }
    }

    ~Meter() {
        destroy();
    }

    Meter(Meter&& other) noexcept
        : m_visionApi(std::exchange(other.m_visionApi, nullptr)),
          m_api(std::exchange(other.m_api, nullptr)),
          m_handle(std::exchange(other.m_handle, nullptr)),
          m_state(std::exchange(other.m_state, nullptr)) {}

    Meter& operator=(Meter&& other) noexcept {
        if (this != &other) {
            destroy();
            m_visionApi = std::exchange(other.m_visionApi, nullptr);
            m_api = std::exchange(other.m_api, nullptr);
            m_handle = std::exchange(other.m_handle, nullptr);
            m_state = std::exchange(other.m_state, nullptr);
        }
        return *this;
    }

    Meter(const Meter&) = delete;
    Meter& operator=(const Meter&) = delete;

    const State& update(
        const Vision::PointI32& point,
        const Vision::PointI32& releasePoint) {
        const State* state = nullptr;
        Vision::detail::checkStatus(
            m_visionApi,
            m_api->meter_update(m_handle, &point, &releasePoint, &state),
            "Meter::update");
        m_state = state;
        return *m_state;
    }

    const State& state() const noexcept {
        return *m_state;
    }

    void setPath(const PathSettings& settings) {
        const HeliosMeterPathSettings native = nativePathSettings(settings);
        Vision::detail::checkStatus(
            m_visionApi,
            m_api->meter_set_path_settings(m_handle, &native),
            "Meter::setPath");
        m_state = m_api->meter_state(m_handle);
    }

    PathSettings pathSettings() const noexcept {
        return ::Helios::Meter::pathSettings(*m_api->meter_path_settings(m_handle));
    }

    void setVisuals(const VisualSettings& settings) {
        const HeliosMeterVisualSettings native = nativeVisualSettings(settings);
        Vision::detail::checkStatus(
            m_visionApi,
            m_api->meter_set_visual_settings(m_handle, &native),
            "Meter::setVisuals");
    }

    VisualSettings visualSettings() const noexcept {
        return ::Helios::Meter::visualSettings(*m_api->meter_visual_settings(m_handle));
    }

    void reset() noexcept {
        m_api->meter_reset(m_handle);
        m_state = m_api->meter_state(m_handle);
    }

    bool valid() const noexcept {
        return m_handle != nullptr;
    }

    HeliosMeter* nativeHandle() const noexcept {
        return m_handle;
    }

private:
    void destroy() noexcept {
        if (m_handle) {
            m_api->meter_destroy(m_handle);
            m_handle = nullptr;
            m_state = nullptr;
        }
    }

    const Vision::detail::VisionFunctions* m_visionApi{nullptr};
    const Vision::detail::MeterFunctions* m_api{nullptr};
    HeliosMeter* m_handle{nullptr};
    const State* m_state{nullptr};
};

} // namespace Helios::Meter

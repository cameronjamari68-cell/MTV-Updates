#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "HeliosCVScriptABI.h"
#include "HeliosInputABI.h"
#include "HeliosInferenceSDK.hpp"
#include "HeliosVisionSDK.hpp"

#include <opencv2/opencv.hpp>
#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <span>
#include <string>

namespace Helios {

namespace Internal {

inline FARPROC findHostProcedure(const char* name)
{
    constexpr const wchar_t* hostModules[] = {
        L"CVCppWrapper.exe",
        L"_helios_controls.pyd",
    };

    for (const wchar_t* moduleName : hostModules) {
        if (HMODULE module = GetModuleHandleW(moduleName)) {
            if (FARPROC procedure = GetProcAddress(module, name)) {
                return procedure;
            }
        }
    }

    std::abort();
}

template <typename T>
T hostProcedure(const char* name)
{
    return reinterpret_cast<T>(findHostProcedure(name));
}

} // namespace Internal

namespace Controls {

namespace Controller {
inline constexpr int BUTTON_1 = 0;
inline constexpr int BUTTON_2 = 1;
inline constexpr int BUTTON_3 = 2;
inline constexpr int BUTTON_4 = 3;
inline constexpr int BUTTON_5 = 4;
inline constexpr int BUTTON_6 = 5;
inline constexpr int BUTTON_7 = 6;
inline constexpr int BUTTON_8 = 7;
inline constexpr int BUTTON_9 = 8;
inline constexpr int BUTTON_10 = 9;
inline constexpr int BUTTON_11 = 10;
inline constexpr int BUTTON_12 = 11;
inline constexpr int BUTTON_13 = 12;
inline constexpr int BUTTON_14 = 13;
inline constexpr int BUTTON_15 = 14;
inline constexpr int BUTTON_16 = 15;
inline constexpr int BUTTON_17 = 16;
inline constexpr int BUTTON_18 = 17;
inline constexpr int BUTTON_19 = 18;
inline constexpr int BUTTON_20 = 19;
inline constexpr int BUTTON_21 = 20;

inline constexpr int STICK_1_X = 21;
inline constexpr int STICK_1_Y = 22;
inline constexpr int STICK_2_X = 23;
inline constexpr int STICK_2_Y = 24;
inline constexpr int POINT_1_X = 25;
inline constexpr int POINT_1_Y = 26;
inline constexpr int POINT_2_X = 27;
inline constexpr int POINT_2_Y = 28;
inline constexpr int ACCEL_1_X = 29;
inline constexpr int ACCEL_1_Y = 30;
inline constexpr int ACCEL_1_Z = 31;
inline constexpr int ACCEL_2_X = 32;
inline constexpr int ACCEL_2_Y = 33;
inline constexpr int ACCEL_2_Z = 34;
inline constexpr int GYRO_1_X = 35;
inline constexpr int GYRO_1_Y = 36;
inline constexpr int GYRO_1_Z = 37;
inline constexpr int PADDLE_1 = 38;
inline constexpr int PADDLE_2 = 39;
inline constexpr int PADDLE_3 = 40;
inline constexpr int PADDLE_4 = 41;
} // namespace Controller

namespace KeyboardModifier {
inline constexpr std::uint32_t SHIFT_LEFT = 1u << 0;
inline constexpr std::uint32_t SHIFT_RIGHT = 1u << 1;
inline constexpr std::uint32_t CTRL_LEFT = 1u << 2;
inline constexpr std::uint32_t CTRL_RIGHT = 1u << 3;
inline constexpr std::uint32_t ALT_LEFT = 1u << 4;
inline constexpr std::uint32_t ALT_RIGHT = 1u << 5;
inline constexpr std::uint32_t WIN_LEFT = 1u << 6;
inline constexpr std::uint32_t WIN_RIGHT = 1u << 7;
inline constexpr std::uint32_t CAPS_LOCK = 1u << 8;
inline constexpr std::uint32_t NUM_LOCK = 1u << 9;
inline constexpr std::uint32_t SCROLL_LOCK = 1u << 10;
} // namespace KeyboardModifier

namespace MouseButton {
inline constexpr std::uint32_t LEFT = 1u << 0;
inline constexpr std::uint32_t RIGHT = 1u << 1;
inline constexpr std::uint32_t MIDDLE = 1u << 2;
inline constexpr std::uint32_t X1 = 1u << 3;
inline constexpr std::uint32_t X2 = 1u << 4;
} // namespace MouseButton

using SharedMemoryControllerState = HeliosControllerState;
using ControllerData = HeliosControllerData;
using KeyboardData = HeliosKeyboardData;
using MouseCoordMode = HeliosMouseCoordMode;
using MouseMotionSample = HeliosMouseMotionSample;
using MouseData = HeliosMouseData;

inline constexpr std::uint32_t MOUSE_MOTION_FLAG_IDLE_RESET =
    HELIOS_MOUSE_MOTION_FLAG_IDLE_RESET;
inline constexpr std::uint32_t MOUSE_MOTION_FLAG_RELATIVE =
    HELIOS_MOUSE_MOTION_FLAG_RELATIVE;
inline constexpr std::uint32_t MOUSE_MOTION_FLAG_DISCONTINUITY =
    HELIOS_MOUSE_MOTION_FLAG_DISCONTINUITY;

inline bool isKeyDown(const KeyboardData& data, std::uint32_t virtualKey)
{
    return virtualKey < HELIOS_KEY_COUNT && data.keys[virtualKey] != 0;
}

inline bool isMouseButtonDown(const MouseData& data, std::uint32_t buttonMask)
{
    return (data.buttons & buttonMask) != 0;
}

inline float getActual(int button)
{
    static const auto function = Internal::hostProcedure<float (*)(int)>("get_actual");
    return function(button);
}

inline float getVal(int button)
{
    static const auto function = Internal::hostProcedure<float (*)(int)>("get_val");
    return function(button);
}

inline const ControllerData* controllerInputState()
{
    static const auto function =
        Internal::hostProcedure<const ControllerData* (*)()>("controller_input_state");
    return function();
}

inline const ControllerData* controllerReportState()
{
    static const auto function =
        Internal::hostProcedure<const ControllerData* (*)()>("controller_report_state");
    return function();
}

inline const KeyboardData* keyboardInputState()
{
    static const auto function =
        Internal::hostProcedure<const KeyboardData* (*)()>("keyboard_input_state");
    return function();
}

inline const KeyboardData* keyboardReportState()
{
    static const auto function =
        Internal::hostProcedure<const KeyboardData* (*)()>("keyboard_report_state");
    return function();
}

inline const MouseData* mouseInputState()
{
    static const auto function =
        Internal::hostProcedure<const MouseData* (*)()>("mouse_input_state");
    return function();
}

inline const MouseData* mouseReportState()
{
    static const auto function =
        Internal::hostProcedure<const MouseData* (*)()>("mouse_report_state");
    return function();
}

inline void sendCvData(const std::uint8_t* data, std::size_t size)
{
    static const auto function =
        Internal::hostProcedure<void (*)(const std::uint8_t*, std::size_t)>("send_cvdata");
    function(data, size);
}

inline void sendCvData(std::span<const std::uint8_t> data)
{
    if (!data.empty()) {
        sendCvData(data.data(), data.size());
    }
}

} // namespace Controls

namespace Overlay {

struct Color {
    std::uint8_t r{255};
    std::uint8_t g{255};
    std::uint8_t b{255};
    std::uint8_t a{255};
};

enum class Target : int {
    Frame = 1,
    Fuser = 2,
    Both = 3,
};

enum class CoordinateSpace : int {
    Pixels = 0,
    Design1080p = 1,
};

enum class TextAnchor : int {
    TopLeft = 0,
    TopCenter = 1,
    TopRight = 2,
    CenterLeft = 3,
    Center = 4,
    CenterRight = 5,
    BottomLeft = 6,
    BottomCenter = 7,
    BottomRight = 8,
};

namespace Detail {

inline std::uint8_t colorByte(double value)
{
    return static_cast<std::uint8_t>(std::clamp(static_cast<int>(value), 0, 255));
}

inline Color colorFromScalar(const cv::Scalar& color)
{
    return {colorByte(color[2]), colorByte(color[1]), colorByte(color[0])};
}

inline int targetBits(Target target)
{
    const int value = static_cast<int>(target);
    if (value < static_cast<int>(Target::Frame) || value > static_cast<int>(Target::Both)) {
        std::abort();
    }
    return value;
}

inline int coordinateSpaceBits(CoordinateSpace space)
{
    const int value = static_cast<int>(space);
    if (value < static_cast<int>(CoordinateSpace::Pixels) ||
        value > static_cast<int>(CoordinateSpace::Design1080p)) {
        std::abort();
    }
    return value;
}

inline int textAnchorBits(TextAnchor anchor)
{
    const int value = static_cast<int>(anchor);
    if (value < static_cast<int>(TextAnchor::TopLeft) ||
        value > static_cast<int>(TextAnchor::BottomRight)) {
        std::abort();
    }
    return value;
}

} // namespace Detail

inline void line(
    int x1,
    int y1,
    int x2,
    int y2,
    Color color,
    int thickness = 1,
    Target target = Target::Both,
    CoordinateSpace space = CoordinateSpace::Pixels)
{
    using Function = void (*)(int, int, int, int, std::uint8_t, std::uint8_t, std::uint8_t,
                              std::uint8_t, int, int, int);
    static const auto function = Internal::hostProcedure<Function>("helios_overlay_line");
    function(x1, y1, x2, y2, color.r, color.g, color.b, color.a, thickness,
             Detail::targetBits(target), Detail::coordinateSpaceBits(space));
}

inline void line(
    cv::Point first,
    cv::Point second,
    cv::Scalar color,
    int thickness = 1,
    Target target = Target::Both,
    CoordinateSpace space = CoordinateSpace::Pixels)
{
    line(first.x, first.y, second.x, second.y, Detail::colorFromScalar(color), thickness, target, space);
}

inline void rect(
    int x1,
    int y1,
    int x2,
    int y2,
    Color color,
    int thickness = 1,
    Target target = Target::Both,
    bool filled = false,
    CoordinateSpace space = CoordinateSpace::Pixels)
{
    using Function = void (*)(int, int, int, int, std::uint8_t, std::uint8_t, std::uint8_t,
                              std::uint8_t, int, int, int, int);
    static const auto function = Internal::hostProcedure<Function>("helios_overlay_rect");
    function(x1, y1, x2, y2, color.r, color.g, color.b, color.a, thickness,
             Detail::targetBits(target), filled ? 1 : 0, Detail::coordinateSpaceBits(space));
}

inline void rect(
    cv::Point first,
    cv::Point second,
    cv::Scalar color,
    int thickness = 1,
    Target target = Target::Both,
    bool filled = false,
    CoordinateSpace space = CoordinateSpace::Pixels)
{
    rect(first.x, first.y, second.x, second.y, Detail::colorFromScalar(color), thickness,
         target, filled, space);
}

inline void circle(
    int x,
    int y,
    int radius,
    Color color,
    int thickness = 1,
    Target target = Target::Both,
    bool filled = false,
    CoordinateSpace space = CoordinateSpace::Pixels)
{
    using Function = void (*)(int, int, int, std::uint8_t, std::uint8_t, std::uint8_t,
                              std::uint8_t, int, int, int, int);
    static const auto function = Internal::hostProcedure<Function>("helios_overlay_circle");
    function(x, y, radius, color.r, color.g, color.b, color.a, thickness,
             Detail::targetBits(target), filled ? 1 : 0, Detail::coordinateSpaceBits(space));
}

inline void circle(
    cv::Point center,
    int radius,
    cv::Scalar color,
    int thickness = 1,
    Target target = Target::Both,
    bool filled = false,
    CoordinateSpace space = CoordinateSpace::Pixels)
{
    circle(center.x, center.y, radius, Detail::colorFromScalar(color), thickness, target, filled, space);
}

inline void text(
    int x,
    int y,
    const char* value,
    Color color,
    int scale = 2,
    Target target = Target::Both,
    TextAnchor anchor = TextAnchor::TopLeft,
    CoordinateSpace space = CoordinateSpace::Pixels,
    Color background = {0, 0, 0, 0},
    int padding = 0)
{
    using Function = void (*)(int, int, const char*, std::uint8_t, std::uint8_t, std::uint8_t,
                              std::uint8_t, int, int, std::uint8_t, std::uint8_t, std::uint8_t,
                              std::uint8_t, int, int, int);
    static const auto function = Internal::hostProcedure<Function>("helios_overlay_text");
    function(x, y, value, color.r, color.g, color.b, color.a, scale, Detail::targetBits(target),
             background.r, background.g, background.b, background.a, padding,
             Detail::textAnchorBits(anchor), Detail::coordinateSpaceBits(space));
}

inline void text(
    const std::string& value,
    cv::Point origin,
    cv::Scalar color,
    int scale = 2,
    Target target = Target::Both,
    TextAnchor anchor = TextAnchor::TopLeft,
    CoordinateSpace space = CoordinateSpace::Pixels,
    Color background = {0, 0, 0, 0},
    int padding = 0)
{
    text(origin.x, origin.y, value.c_str(), Detail::colorFromScalar(color), scale, target,
         anchor, space, background, padding);
}

} // namespace Overlay

struct Frame {
    cv::Mat image;
    std::uint64_t sequence;
    std::uint64_t timestampNs;
};

namespace Internal {

template <typename Worker>
void* HELIOS_CV_SCRIPT_CALL createScript(std::uint32_t width, std::uint32_t height)
{
    try {
        return new Worker(width, height);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "CV script construction failed: %s\n", error.what());
    } catch (...) {
        std::fputs("CV script construction failed with an unknown exception\n", stderr);
    }
    return nullptr;
}

template <typename Worker>
int32_t HELIOS_CV_SCRIPT_CALL processScript(void* instance, const HeliosCVFrame* frame)
{
    if (!instance || !frame || frame->struct_size < sizeof(HeliosCVFrame) ||
        frame->pixel_format != HELIOS_CV_PIXEL_FORMAT_BGR8 || !frame->data ||
        frame->width == 0 || frame->height == 0 || frame->channels != 3 ||
        frame->stride < frame->width * frame->channels) {
        return HELIOS_CV_PROCESS_INVALID_FRAME;
    }

    Frame view{
        cv::Mat(
            static_cast<int>(frame->height),
            static_cast<int>(frame->width),
            CV_8UC3,
            frame->data,
            frame->stride),
        frame->sequence,
        frame->timestamp_ns,
    };

    try {
        static_cast<Worker*>(instance)->process(view);
        return HELIOS_CV_PROCESS_OK;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "CV script processing failed: %s\n", error.what());
    } catch (...) {
        std::fputs("CV script processing failed with an unknown exception\n", stderr);
    }
    return HELIOS_CV_PROCESS_EXCEPTION;
}

template <typename Worker>
void HELIOS_CV_SCRIPT_CALL destroyScript(void* instance)
{
    delete static_cast<Worker*>(instance);
}

} // namespace Internal

} // namespace Helios

#define HELIOS_CV_SCRIPT(WorkerType) \
    extern "C" __declspec(dllexport) void* HELIOS_CV_SCRIPT_CALL \
    helios_cv_script_create(std::uint32_t width, std::uint32_t height) \
    { \
        return Helios::Internal::createScript<WorkerType>(width, height); \
    } \
    extern "C" __declspec(dllexport) std::int32_t HELIOS_CV_SCRIPT_CALL \
    helios_cv_script_process(void* instance, const HeliosCVFrame* frame) \
    { \
        return Helios::Internal::processScript<WorkerType>(instance, frame); \
    } \
    extern "C" __declspec(dllexport) void HELIOS_CV_SCRIPT_CALL \
    helios_cv_script_destroy(void* instance) \
    { \
        Helios::Internal::destroyScript<WorkerType>(instance); \
    }

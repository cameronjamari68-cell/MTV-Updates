#pragma once

#ifndef _WIN32
#error "The Helios input ABI is supported on Windows only."
#endif

#include <stddef.h>
#include <stdint.h>

#define HELIOS_CONTROLLER_BUTTON_COUNT 21u
#define HELIOS_CONTROLLER_AXIS_COUNT 21u
#define HELIOS_KEY_COUNT 320u
#define HELIOS_MOUSE_MOTION_SAMPLE_COUNT 4u

#define HELIOS_MOUSE_MOTION_FLAG_IDLE_RESET (1u << 0)
#define HELIOS_MOUSE_MOTION_FLAG_RELATIVE (1u << 1)
#define HELIOS_MOUSE_MOTION_FLAG_DISCONTINUITY (1u << 2)

#if defined(_MSC_VER)
#define HELIOS_INPUT_ALIGN_64 __declspec(align(64))
#elif defined(__GNUC__)
#define HELIOS_INPUT_ALIGN_64 __attribute__((aligned(64)))
#else
#error "The Helios input ABI requires an explicit 64-byte alignment declaration."
#endif

#ifdef __cplusplus
enum class HeliosMouseCoordMode : uint32_t {
    Relative = 0,
    Absolute = 1,
};
#else
typedef uint32_t HeliosMouseCoordMode;
#define HELIOS_MOUSE_COORD_MODE_RELATIVE 0u
#define HELIOS_MOUSE_COORD_MODE_ABSOLUTE 1u
#endif

typedef struct HELIOS_INPUT_ALIGN_64 HeliosControllerState {
    float buttons[HELIOS_CONTROLLER_BUTTON_COUNT];
    float axes[HELIOS_CONTROLLER_AXIS_COUNT];
    uint64_t timestamp;
    uint8_t isConnected;
    uint8_t padding[15];
} HeliosControllerState;

typedef struct HELIOS_INPUT_ALIGN_64 HeliosControllerData {
    HeliosControllerState state;
    float leftMotor;
    float rightMotor;
    float leftTriggerMotor;
    float rightTriggerMotor;
    float haptic3;
    float haptic4;
    uint8_t rumbleEnabled;
    uint8_t reserved[39];
} HeliosControllerData;

typedef struct HELIOS_INPUT_ALIGN_64 HeliosKeyboardData {
    uint8_t keys[HELIOS_KEY_COUNT];
    uint32_t modifiers;
    uint8_t capsLock;
    uint8_t numLock;
    uint8_t scrollLock;
    uint8_t reserved[57];
} HeliosKeyboardData;

typedef struct HeliosMouseMotionSample {
    int32_t deltaX;
    int32_t deltaY;
    uint32_t intervalUs;
} HeliosMouseMotionSample;

typedef struct HELIOS_INPUT_ALIGN_64 HeliosMouseData {
    int32_t deltaX;
    int32_t deltaY;
    int16_t wheelDeltaY;
    int16_t wheelDeltaX;
    uint32_t buttons;
    int32_t absoluteX;
    int32_t absoluteY;
    HeliosMouseCoordMode coordMode;
    uint32_t dpi;
    uint32_t screenWidth;
    uint32_t screenHeight;
    uint64_t motionSequence;
    int64_t cumulativeDeltaX;
    int64_t cumulativeDeltaY;
    uint64_t cumulativeIntervalUs;
    uint32_t motionGeneration;
    uint32_t motionFlags;
    HeliosMouseMotionSample motionSamples[HELIOS_MOUSE_MOTION_SAMPLE_COUNT];
} HeliosMouseData;

#ifdef __cplusplus
static_assert(sizeof(HeliosMouseCoordMode) == sizeof(uint32_t));
static_assert(sizeof(HeliosControllerState) == 192);
static_assert(alignof(HeliosControllerState) == 64);
static_assert(offsetof(HeliosControllerState, timestamp) == 168);
static_assert(offsetof(HeliosControllerState, isConnected) == 176);
static_assert(sizeof(HeliosControllerData) == 256);
static_assert(alignof(HeliosControllerData) == 64);
static_assert(offsetof(HeliosControllerData, leftMotor) == 192);
static_assert(offsetof(HeliosControllerData, rumbleEnabled) == 216);
static_assert(sizeof(HeliosKeyboardData) == 384);
static_assert(alignof(HeliosKeyboardData) == 64);
static_assert(offsetof(HeliosKeyboardData, modifiers) == 320);
static_assert(offsetof(HeliosKeyboardData, capsLock) == 324);
static_assert(sizeof(HeliosMouseMotionSample) == 12);
static_assert(offsetof(HeliosMouseMotionSample, deltaX) == 0);
static_assert(offsetof(HeliosMouseMotionSample, deltaY) == 4);
static_assert(offsetof(HeliosMouseMotionSample, intervalUs) == 8);
static_assert(sizeof(HeliosMouseData) == 128);
static_assert(alignof(HeliosMouseData) == 64);
static_assert(offsetof(HeliosMouseData, motionSequence) == 40);
static_assert(offsetof(HeliosMouseData, cumulativeDeltaX) == 48);
static_assert(offsetof(HeliosMouseData, cumulativeDeltaY) == 56);
static_assert(offsetof(HeliosMouseData, cumulativeIntervalUs) == 64);
static_assert(offsetof(HeliosMouseData, motionGeneration) == 72);
static_assert(offsetof(HeliosMouseData, motionFlags) == 76);
static_assert(offsetof(HeliosMouseData, motionSamples) == 80);
#else
_Static_assert(sizeof(HeliosControllerState) == 192, "HeliosControllerState layout mismatch");
_Static_assert(sizeof(HeliosControllerData) == 256, "HeliosControllerData layout mismatch");
_Static_assert(sizeof(HeliosKeyboardData) == 384, "HeliosKeyboardData layout mismatch");
_Static_assert(sizeof(HeliosMouseMotionSample) == 12, "HeliosMouseMotionSample layout mismatch");
_Static_assert(sizeof(HeliosMouseData) == 128, "HeliosMouseData layout mismatch");
#endif

#undef HELIOS_INPUT_ALIGN_64

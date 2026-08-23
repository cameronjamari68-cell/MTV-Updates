#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_MSC_VER)
#define HELIOS_INFERENCE_ALIGN_64 __declspec(align(64))
#elif defined(__GNUC__)
#define HELIOS_INFERENCE_ALIGN_64 __attribute__((aligned(64)))
#else
#error "The Helios inference ABI requires an explicit 64-byte alignment declaration."
#endif

enum {
    HELIOS_INFERENCE_RESULT_MAX_DETECTIONS = 64,
    HELIOS_INFERENCE_RESULT_SLOT_COUNT = 16,
    HELIOS_INFERENCE_RESULT_RUNTIME_ERROR_INDEX = 4,
};

enum {
    HELIOS_INFERENCE_PREDICTION_ENABLED = 1u << 0,
    HELIOS_INFERENCE_PREDICTION_WARMUP = 1u << 1,
    HELIOS_INFERENCE_PREDICTION_VALID = 1u << 2,
    HELIOS_INFERENCE_PREDICTION_CLAMPED = 1u << 3,
    HELIOS_INFERENCE_PREDICTION_TRACK_RESET = 1u << 4,
    HELIOS_INFERENCE_PREDICTION_CAMERA_COMPENSATED = 1u << 5,
    HELIOS_INFERENCE_PREDICTION_INPUT_UNAVAILABLE = 1u << 6,
};

typedef struct HeliosInferenceKeypoint {
    float x;
    float y;
    float confidence;
} HeliosInferenceKeypoint;

typedef struct HELIOS_INFERENCE_ALIGN_64 HeliosInferenceDetection {
    uint32_t class_id;
    uint32_t _reserved0[1];
    float confidence;
    float x1;
    float y1;
    float x2;
    float y2;
    float width;
    float height;
    float center_x;
    float center_y;
    float area;
    float anchor_x;
    float anchor_y;
    float polar_distance;
    float polar_angle;
    float predicted_anchor_x;
    float predicted_anchor_y;
    float prediction_offset_x;
    float prediction_offset_y;
    float prediction_input_x;
    float prediction_input_y;
    float prediction_target_motion_x;
    float prediction_target_motion_y;
    uint32_t continuous_detection_age_ms;
    float prediction_quality;
    uint32_t prediction_flags;
    uint32_t _reserved_metrics[5];
    HeliosInferenceKeypoint keypoints[17];
    uint32_t _pad[13];
} HeliosInferenceDetection;

typedef struct HELIOS_INFERENCE_ALIGN_64 HeliosInferenceResultsBlock {
    volatile uint64_t write_sequence;
    uint64_t frame_sequence;
    uint64_t timestamp_ns;
    uint32_t num_detections;
    uint32_t model_type;
    float inference_time_ms;
    uint32_t frame_width;
    uint32_t frame_height;
    uint32_t _reserved[8];
    uint32_t _pad_to_cacheline[13];
    HeliosInferenceDetection detections[HELIOS_INFERENCE_RESULT_MAX_DETECTIONS];
} HeliosInferenceResultsBlock;

typedef struct HeliosInferenceResultsArray {
    HeliosInferenceResultsBlock slots[HELIOS_INFERENCE_RESULT_SLOT_COUNT];
} HeliosInferenceResultsArray;

#ifdef __cplusplus
static_assert(alignof(HeliosInferenceDetection) == 64);
static_assert(sizeof(HeliosInferenceDetection) == 384);
static_assert(alignof(HeliosInferenceResultsBlock) == 64);
static_assert(offsetof(HeliosInferenceResultsBlock, detections) == 128);
static_assert(sizeof(HeliosInferenceResultsBlock) == 24704);
#endif

#undef HELIOS_INFERENCE_ALIGN_64

#ifdef __cplusplus
}
#endif

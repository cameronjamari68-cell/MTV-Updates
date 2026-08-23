#pragma once

#include <stdint.h>

#include "HeliosVision.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HELIOS_METER_DESIGN_WIDTH 1920u
#define HELIOS_METER_DESIGN_HEIGHT 1080u

enum HeliosMeterPathAlgorithm {
    HELIOS_METER_PATH_STRAIGHT = 0,
    HELIOS_METER_PATH_QUADRATIC_BEZIER = 1,
};

enum HeliosMeterVisualTarget {
    HELIOS_METER_VISUAL_TARGET_FRAME = 1,
    HELIOS_METER_VISUAL_TARGET_FUSER = 2,
    HELIOS_METER_VISUAL_TARGET_BOTH = 3,
};

typedef struct HeliosMeterPathSettings {
    uint32_t algorithm;
    uint32_t segments;
    double curvature;
} HeliosMeterPathSettings;

typedef struct HeliosMeterVisualSettings {
    uint32_t enabled;
    uint32_t show_bbox;
    uint32_t show_path;
    uint32_t show_distance;
    uint32_t show_speed;
    uint32_t show_time_to_release;
    uint32_t show_elapsed_time;
    uint32_t target;
    HeliosVisionRgba8 bbox_color;
    HeliosVisionRgba8 path_color;
    HeliosVisionRgba8 metrics_color;
    uint32_t bbox_thickness;
    uint32_t path_thickness;
    uint32_t text_scale;
    uint32_t text_gap;
} HeliosMeterVisualSettings;

typedef struct HeliosMeterState {
    uint64_t meter_id;
    uint64_t sample_count;
    uint64_t frame_sequence;
    uint64_t timestamp_ns;
    HeliosVisionPointI32 point;
    HeliosVisionPointI32 release_point;
    HeliosVisionRectI32 roi;
    HeliosVisionPointF64 point_design;
    HeliosVisionPointF64 release_point_design;
    HeliosVisionPointF64 delta_design;
    HeliosVisionPointF64 control_point_design;
    uint32_t path_algorithm;
    double straight_distance;
    double distance;
    double previous_distance;
    double distance_delta;
    double speed;
    double delta_time_seconds;
    double elapsed_time_seconds;
    double time_to_release_seconds;
} HeliosMeterState;

typedef struct HeliosMeter HeliosMeter;

HELIOSVISION_API int32_t helios_meter_create(HeliosMeter** out_meter);
HELIOSVISION_API void helios_meter_destroy(HeliosMeter* meter);
HELIOSVISION_API int32_t helios_meter_update(
    HeliosMeter* meter,
    const HeliosVisionPointI32* point,
    const HeliosVisionPointI32* release_point,
    const HeliosMeterState** out_state);
HELIOSVISION_API const HeliosMeterState* helios_meter_state(const HeliosMeter* meter);
HELIOSVISION_API int32_t helios_meter_set_path_settings(
    HeliosMeter* meter,
    const HeliosMeterPathSettings* settings);
HELIOSVISION_API const HeliosMeterPathSettings* helios_meter_path_settings(
    const HeliosMeter* meter);
HELIOSVISION_API int32_t helios_meter_set_visual_settings(
    HeliosMeter* meter,
    const HeliosMeterVisualSettings* settings);
HELIOSVISION_API const HeliosMeterVisualSettings* helios_meter_visual_settings(
    const HeliosMeter* meter);
HELIOSVISION_API void helios_meter_reset(HeliosMeter* meter);

#ifdef __cplusplus
}
#endif

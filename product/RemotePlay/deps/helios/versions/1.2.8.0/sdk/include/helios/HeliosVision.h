#pragma once

#include <stdint.h>

#ifdef _WIN32
    #ifdef HELIOSVISION_EXPORTS
        #define HELIOSVISION_API __declspec(dllexport)
    #else
        #define HELIOSVISION_API __declspec(dllimport)
    #endif
#else
    #define HELIOSVISION_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define HELIOS_VISION_MAX_COLOR_RANGES 32u
#define HELIOS_VISION_SHAPE_GRID_SIZE 32u

enum HeliosVisionStatus {
    HELIOS_VISION_STATUS_OK = 0,
    HELIOS_VISION_STATUS_INVALID_ARGUMENT = 1,
    HELIOS_VISION_STATUS_UNSUPPORTED_FORMAT = 2,
    HELIOS_VISION_STATUS_OUT_OF_MEMORY = 3,
    HELIOS_VISION_STATUS_RESOLUTION_CHANGED = 4,
    HELIOS_VISION_STATUS_INTERNAL_ERROR = 5,
    HELIOS_VISION_STATUS_NO_CURRENT_FRAME = 6,
};

enum HeliosVisionPixelFormat {
    HELIOS_VISION_PIXEL_FORMAT_UNKNOWN = 0,
    HELIOS_VISION_PIXEL_FORMAT_BGR8 = 1,
};

enum HeliosVisionConnectivity {
    HELIOS_VISION_CONNECTIVITY_4 = 4,
    HELIOS_VISION_CONNECTIVITY_8 = 8,
};

enum HeliosVisionGrouping {
    HELIOS_VISION_GROUP_CONNECTED = 0,
    HELIOS_VISION_GROUP_ALL_IN_ROI = 1,
};

enum HeliosVisionNormMethod {
    HELIOS_VISION_NORM_L1 = 0,
    HELIOS_VISION_NORM_L2_SQUARED = 1,
};

enum HeliosVisionTemplateMethod {
    HELIOS_VISION_TEMPLATE_SAD = 0,
    HELIOS_VISION_TEMPLATE_SSD = 1,
    HELIOS_VISION_TEMPLATE_NCC = 2,
};

enum HeliosVisionResultFlags {
    HELIOS_VISION_RESULT_FLAG_NONE = 0,
    HELIOS_VISION_RESULT_FLAG_TRUNCATED = 1u << 0,
};

typedef struct HeliosVisionRectI32 {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
} HeliosVisionRectI32;

typedef struct HeliosVisionPointI32 {
    int32_t x;
    int32_t y;
} HeliosVisionPointI32;

typedef struct HeliosVisionPointF64 {
    double x;
    double y;
} HeliosVisionPointF64;

typedef struct HeliosVisionRgba8 {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} HeliosVisionRgba8;

typedef struct HeliosVisionBgr8 {
    uint8_t b;
    uint8_t g;
    uint8_t r;
    uint8_t _reserved0;
} HeliosVisionBgr8;

typedef struct HeliosVisionRangeU32 {
    uint32_t minimum;
    uint32_t maximum;
} HeliosVisionRangeU32;

typedef struct HeliosVisionFrameView {
    uint32_t struct_size;
    const uint8_t* data;
    uint64_t data_size;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t pixel_format;
    uint64_t frame_sequence;
    uint64_t timestamp_ns;
    uint64_t _reserved[4];
} HeliosVisionFrameView;

typedef struct HeliosVisionBgrRange {
    HeliosVisionBgr8 low;
    HeliosVisionBgr8 high;
    uint32_t _reserved[3];
} HeliosVisionBgrRange;

typedef struct HeliosVisionShapeView {
    uint32_t struct_size;
    const uint8_t* data;
    uint64_t data_size;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t tolerance_cells;
    float minimum_score;
    uint32_t _reserved[4];
} HeliosVisionShapeView;

typedef struct HeliosVisionContourFinderConfig {
    uint32_t struct_size;
    HeliosVisionRectI32 roi;
    uint32_t reference_width;
    uint32_t reference_height;
    HeliosVisionRangeU32 width;
    HeliosVisionRangeU32 height;
    const HeliosVisionBgrRange* colors;
    uint32_t color_count;
    uint32_t connectivity;
    uint32_t grouping;
    uint32_t max_results;
    HeliosVisionShapeView shape;
    uint32_t _reserved[8];
} HeliosVisionContourFinderConfig;

typedef struct HeliosVisionContour {
    HeliosVisionRectI32 bounds;
    float centroid_x;
    float centroid_y;
    double area;
    float fill_ratio;
    float color_score;
    float shape_score;
    float confidence;
    uint32_t core_pixel_count;
    uint32_t halo_pixel_count;
    uint32_t color_mask;
    uint32_t result_index;
    HeliosVisionBgr8 observed_low;
    HeliosVisionBgr8 observed_high;
    uint64_t generation;
    uint64_t _reserved[2];
} HeliosVisionContour;

typedef struct HeliosVisionColorStats {
    uint32_t color_index;
    uint32_t pixel_count;
    HeliosVisionBgr8 raw_low;
    HeliosVisionBgr8 raw_high;
    HeliosVisionBgr8 percentile05;
    HeliosVisionBgr8 median;
    HeliosVisionBgr8 percentile95;
    float mean_b;
    float mean_g;
    float mean_r;
    uint32_t core_pixel_count;
    uint32_t halo_pixel_count;
    uint32_t _reserved[4];
} HeliosVisionColorStats;

typedef struct HeliosVisionContourResults {
    const HeliosVisionContour* data;
    uint32_t count;
    uint32_t flags;
    uint64_t generation;
} HeliosVisionContourResults;

typedef struct HeliosVisionNormResult {
    uint64_t raw_value;
    uint64_t sample_count;
    double mean_error;
    double similarity;
} HeliosVisionNormResult;

typedef struct HeliosVisionTemplateResult {
    HeliosVisionRectI32 bounds;
    uint64_t raw_value;
    double score;
    uint32_t found;
    uint32_t _reserved[3];
} HeliosVisionTemplateResult;

typedef struct HeliosVisionContourFinder HeliosVisionContourFinder;
typedef struct HeliosVisionNormMatcher HeliosVisionNormMatcher;
typedef struct HeliosVisionTemplateMatcher HeliosVisionTemplateMatcher;

HELIOSVISION_API const char* helios_vision_status_message(int32_t status);

HELIOSVISION_API int32_t helios_vision_contour_finder_create(
    const HeliosVisionContourFinderConfig* config,
    HeliosVisionContourFinder** out_finder);
HELIOSVISION_API void helios_vision_contour_finder_destroy(HeliosVisionContourFinder* finder);
HELIOSVISION_API int32_t helios_vision_contour_finder_set_colors(
    HeliosVisionContourFinder* finder,
    const HeliosVisionBgrRange* colors,
    uint32_t color_count);
HELIOSVISION_API int32_t helios_vision_contour_finder_find(
    HeliosVisionContourFinder* finder,
    HeliosVisionContourResults* out_results);
HELIOSVISION_API int32_t helios_vision_contour_finder_color_stats(
    const HeliosVisionContourFinder* finder,
    uint32_t result_index,
    uint32_t color_index,
    HeliosVisionColorStats* out_stats);

HELIOSVISION_API int32_t helios_vision_norm_matcher_create(
    const HeliosVisionFrameView* reference,
    const HeliosVisionRectI32* reference_roi,
    uint32_t method,
    HeliosVisionNormMatcher** out_matcher);
HELIOSVISION_API void helios_vision_norm_matcher_destroy(HeliosVisionNormMatcher* matcher);
HELIOSVISION_API int32_t helios_vision_norm_matcher_match(
    const HeliosVisionNormMatcher* matcher,
    const HeliosVisionRectI32* roi,
    HeliosVisionNormResult* out_result);

HELIOSVISION_API int32_t helios_vision_template_matcher_create(
    const HeliosVisionFrameView* template_frame,
    const HeliosVisionRectI32* template_roi,
    uint32_t method,
    HeliosVisionTemplateMatcher** out_matcher);
HELIOSVISION_API void helios_vision_template_matcher_destroy(HeliosVisionTemplateMatcher* matcher);
HELIOSVISION_API int32_t helios_vision_template_matcher_find(
    const HeliosVisionTemplateMatcher* matcher,
    const HeliosVisionRectI32* search_roi,
    HeliosVisionTemplateResult* out_result);

#ifdef __cplusplus
}
#endif

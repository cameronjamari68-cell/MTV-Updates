#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    HELIOS_INFERENCE_OCR_MAX_REGIONS = 16,
    HELIOS_INFERENCE_OCR_MAX_TEXT_BYTES = 256,
};

enum {
    HELIOS_INFERENCE_OCR_REGION_PIXELS = 1,
    HELIOS_INFERENCE_OCR_REGION_NORMALIZED = 2,
};

enum {
    HELIOS_INFERENCE_OCR_STATUS_EMPTY = 0,
    HELIOS_INFERENCE_OCR_STATUS_READY = 1,
    HELIOS_INFERENCE_OCR_STATUS_NO_TEXT = 2,
    HELIOS_INFERENCE_OCR_STATUS_ERROR = 3,
    HELIOS_INFERENCE_OCR_STATUS_DISABLED = 4,
};

enum {
    HELIOS_INFERENCE_OCR_FLAG_TRUNCATED = 1u << 0,
};

typedef struct HeliosInferenceOcrResult {
    volatile uint64_t write_sequence;
    uint64_t frame_sequence;
    uint64_t timestamp_ns;
    uint32_t region_id;
    uint32_t status;
    float confidence;
    uint32_t text_bytes;
    uint32_t frame_width;
    uint32_t frame_height;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    uint32_t flags;
    uint32_t _reserved[8];
    char text[HELIOS_INFERENCE_OCR_MAX_TEXT_BYTES];
} HeliosInferenceOcrResult;

typedef struct HeliosInferenceOcrResultsBlock {
    uint32_t struct_size;
    uint32_t region_capacity;
    uint32_t _reserved[13];
    HeliosInferenceOcrResult regions[HELIOS_INFERENCE_OCR_MAX_REGIONS];
} HeliosInferenceOcrResultsBlock;

#ifdef __cplusplus
static_assert(sizeof(HeliosInferenceOcrResult) == 360);
static_assert(offsetof(HeliosInferenceOcrResult, text) == 100);
static_assert(offsetof(HeliosInferenceOcrResultsBlock, regions) == 64);
static_assert(sizeof(HeliosInferenceOcrResultsBlock) == 5824);
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(HeliosInferenceOcrResult) == 360, "OCR result size mismatch");
_Static_assert(offsetof(HeliosInferenceOcrResult, text) == 100, "OCR text offset mismatch");
_Static_assert(offsetof(HeliosInferenceOcrResultsBlock, regions) == 64, "OCR region offset mismatch");
_Static_assert(sizeof(HeliosInferenceOcrResultsBlock) == 5824, "OCR results size mismatch");
#endif

#ifdef __cplusplus
}
#endif

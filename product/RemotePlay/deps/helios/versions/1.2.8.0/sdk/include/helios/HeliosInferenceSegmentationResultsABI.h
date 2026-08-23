#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_MSC_VER)
#define HELIOS_INFERENCE_SEGMENTATION_ALIGN_64 __declspec(align(64))
#elif defined(__GNUC__)
#define HELIOS_INFERENCE_SEGMENTATION_ALIGN_64 __attribute__((aligned(64)))
#else
#error "The Helios inference segmentation ABI requires an explicit 64-byte alignment declaration."
#endif

enum {
    HELIOS_INFERENCE_SEGMENTATION_MAX_MASKS = 64,
    HELIOS_INFERENCE_SEGMENTATION_MAX_MASK_WIDTH = 512,
    HELIOS_INFERENCE_SEGMENTATION_MAX_MASK_HEIGHT = 512,
    HELIOS_INFERENCE_SEGMENTATION_MAX_ROW_BYTES =
        HELIOS_INFERENCE_SEGMENTATION_MAX_MASK_WIDTH / 8,
    HELIOS_INFERENCE_SEGMENTATION_MASK_PLANE_CAPACITY =
        HELIOS_INFERENCE_SEGMENTATION_MAX_ROW_BYTES *
        HELIOS_INFERENCE_SEGMENTATION_MAX_MASK_HEIGHT,
    HELIOS_INFERENCE_SEGMENTATION_SLOT_COUNT = 16,
};

enum {
    HELIOS_INFERENCE_SEGMENTATION_FORMAT_NONE = 0,
    HELIOS_INFERENCE_SEGMENTATION_FORMAT_BITMASK_LSB = 1,
};

typedef struct HELIOS_INFERENCE_SEGMENTATION_ALIGN_64 HeliosInferenceSegmentationResultsBlock {
    volatile uint64_t write_sequence;
    uint64_t frame_sequence;
    uint32_t format;
    uint32_t mask_count;
    uint32_t mask_width;
    uint32_t mask_height;
    uint32_t mask_row_bytes;
    uint32_t mask_plane_stride;
    uint32_t frame_width;
    uint32_t frame_height;
    float mask_origin_x;
    float mask_origin_y;
    float mask_scale_x;
    float mask_scale_y;
    uint32_t _reserved[16];
    uint8_t masks[
        HELIOS_INFERENCE_SEGMENTATION_MAX_MASKS *
        HELIOS_INFERENCE_SEGMENTATION_MASK_PLANE_CAPACITY];
} HeliosInferenceSegmentationResultsBlock;

typedef struct HeliosInferenceSegmentationResultsArray {
    HeliosInferenceSegmentationResultsBlock
        slots[HELIOS_INFERENCE_SEGMENTATION_SLOT_COUNT];
} HeliosInferenceSegmentationResultsArray;

#ifdef __cplusplus
static_assert(alignof(HeliosInferenceSegmentationResultsBlock) == 64);
static_assert(offsetof(HeliosInferenceSegmentationResultsBlock, masks) == 128);
static_assert(sizeof(HeliosInferenceSegmentationResultsBlock) == 2097280);
#endif

#undef HELIOS_INFERENCE_SEGMENTATION_ALIGN_64

#ifdef __cplusplus
}
#endif

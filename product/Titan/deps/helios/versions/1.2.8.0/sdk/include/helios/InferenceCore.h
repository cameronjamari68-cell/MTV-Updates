/**
 * InferenceCore - GPU Inference Runtime
 */

#ifndef INFCORE_H
#define INFCORE_H

#include <stddef.h>
#include <stdint.h>

#include "HeliosInferenceOcrABI.h"

#ifdef _WIN32
    #ifdef INFCORE_EXPORTS
        #define INFCORE_API __declspec(dllexport)
    #else
        #define INFCORE_API __declspec(dllimport)
    #endif
#else
    #define INFCORE_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define INFCORE_OK                     0
#define INFCORE_ERROR_FILE_NOT_FOUND   1
#define INFCORE_ERROR_INVALID_FORMAT   2
#define INFCORE_ERROR_NOT_LOGGED_IN    3
#define INFCORE_ERROR_SESSION_EXPIRED  4
#define INFCORE_ERROR_NETWORK          5
#define INFCORE_ERROR_ACCESS_DENIED    6
#define INFCORE_ERROR_DECRYPT_FAILED   7
#define INFCORE_ERROR_ONNX_LOAD        8
#define INFCORE_ERROR_DEBUGGER         9
#define INFCORE_ERROR_MEMORY           10
#define INFCORE_ERROR_INVALID_SALT     11
#define INFCORE_ERROR_NOT_ENCRYPTED    12
#define INFCORE_ERROR_ENCRYPT_FAILED   13
#define INFCORE_ERROR_ENGINE_BUILD     14
#define INFCORE_ERROR_RATE_LIMITED     15
#define INFCORE_ERROR_INVALID_ARGUMENT 16
#define INFCORE_ERROR_INFERENCE        17

#define INFCORE_MODEL_DETECTION    0
#define INFCORE_MODEL_POSE         1
#define INFCORE_MODEL_SEGMENTATION 2

#define INFCORE_POSE_ANCHOR_NONE    0
#define INFCORE_POSE_ANCHOR_HEAD    1
#define INFCORE_POSE_ANCHOR_CHEST   2
#define INFCORE_POSE_ANCHOR_ABDOMEN 3

#define INFCORE_ROI_DISABLED    0
#define INFCORE_ROI_NORMALIZED  1
#define INFCORE_ROI_PIXELS      2
#define INFCORE_ROI_MODEL_INPUT 3
#define INFCORE_ROI_RECOMMENDED 4

#define INFCORE_SORT_DISTANCE   0
#define INFCORE_SORT_CONFIDENCE 1

typedef struct InferenceEngine* InferenceEngineHandle;

typedef struct InfcoreAnchorPredictionConfig {
    uint32_t struct_size;
    int32_t x_enabled;
    int32_t y_enabled;
    float response_delay_ms;
    float lead_x;
    float lead_y;
    uint32_t _reserved[8];
} InfcoreAnchorPredictionConfig;

#include "InferenceCoreApi.h"

#define INFCORE_DECLARE(return_type, name, signature) \
    INFCORE_API return_type name signature;
INFCORE_ABI_FUNCTIONS(INFCORE_DECLARE)
#undef INFCORE_DECLARE

#ifdef __cplusplus
}

static_assert(sizeof(InfcoreAnchorPredictionConfig) == 56,
    "InfcoreAnchorPredictionConfig layout mismatch.");
#endif

#endif /* INFCORE_H */

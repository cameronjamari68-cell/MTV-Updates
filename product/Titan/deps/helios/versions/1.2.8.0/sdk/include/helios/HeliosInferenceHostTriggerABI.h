#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef _MSC_VER
#define HELIOS_INFERENCE_HOST_TRIGGER_CALL __cdecl
#else
#define HELIOS_INFERENCE_HOST_TRIGGER_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void (HELIOS_INFERENCE_HOST_TRIGGER_CALL *HeliosInferenceFrameTriggerCallback)(void* user_data, uint64_t sequence);

typedef bool (HELIOS_INFERENCE_HOST_TRIGGER_CALL *helios_register_inference_frame_trigger_fn)(
    HeliosInferenceFrameTriggerCallback callback,
    void* user_data);

typedef void (HELIOS_INFERENCE_HOST_TRIGGER_CALL *helios_unregister_inference_frame_trigger_fn)(
    HeliosInferenceFrameTriggerCallback callback,
    void* user_data);

#ifdef __cplusplus
}
#endif

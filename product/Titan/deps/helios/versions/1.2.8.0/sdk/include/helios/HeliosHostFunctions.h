#pragma once

#include <stdint.h>

#ifdef _WIN32
#define HELIOS_INFERENCE_HOST_CALL __cdecl
#else
#define HELIOS_INFERENCE_HOST_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct HeliosVisionFrameView;

typedef const struct HeliosVisionFrameView* (
    HELIOS_INFERENCE_HOST_CALL *helios_host_get_current_frame_fn)(void);
typedef uint64_t (HELIOS_INFERENCE_HOST_CALL *helios_host_get_current_frame_sequence_fn)(void);
typedef uint64_t (HELIOS_INFERENCE_HOST_CALL *helios_host_get_current_frame_timestamp_ns_fn)(void);
typedef uint32_t (HELIOS_INFERENCE_HOST_CALL *helios_host_get_process_id_fn)(void);
typedef const char* (HELIOS_INFERENCE_HOST_CALL *helios_host_get_video_ring_buffer_name_fn)(void);
typedef const char* (HELIOS_INFERENCE_HOST_CALL *helios_host_get_script_type_fn)(void);
typedef const char* (HELIOS_INFERENCE_HOST_CALL *helios_host_get_script_hash_fn)(void);
typedef const char* (HELIOS_INFERENCE_HOST_CALL *helios_host_get_execution_grant_fn)(void);
typedef const char* (HELIOS_INFERENCE_HOST_CALL *helios_host_get_root_path_fn)(void);

#ifdef __cplusplus
}
#endif

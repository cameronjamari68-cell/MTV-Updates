#pragma once

#include <stdint.h>

#ifndef _WIN32
#error "The Helios CV C++ script ABI requires Windows."
#endif

#define HELIOS_CV_SCRIPT_CALL __cdecl

#ifdef __cplusplus
extern "C" {
#endif

enum HeliosCVPixelFormat {
    HELIOS_CV_PIXEL_FORMAT_BGR8 = 1,
};

enum HeliosCVProcessStatus {
    HELIOS_CV_PROCESS_OK = 0,
    HELIOS_CV_PROCESS_INVALID_FRAME = 1,
    HELIOS_CV_PROCESS_EXCEPTION = 2,
};

typedef struct HeliosCVFrame {
    uint32_t struct_size;
    uint32_t pixel_format;
    uint8_t* data;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t channels;
    uint64_t sequence;
    uint64_t timestamp_ns;
} HeliosCVFrame;

typedef void* (HELIOS_CV_SCRIPT_CALL *helios_cv_script_create_fn)(
    uint32_t width,
    uint32_t height);

typedef int32_t (HELIOS_CV_SCRIPT_CALL *helios_cv_script_process_fn)(
    void* instance,
    const HeliosCVFrame* frame);

typedef void (HELIOS_CV_SCRIPT_CALL *helios_cv_script_destroy_fn)(
    void* instance);

#ifdef __cplusplus
}

static_assert(sizeof(HeliosCVFrame) == 48, "HeliosCVFrame layout mismatch.");
#endif

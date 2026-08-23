#pragma once

#define INFCORE_ABI_FUNCTIONS(X) \
    X(int, infcore_init, (void)) \
    X(void, infcore_shutdown, (void)) \
    X(int, infcore_get_last_error, (void)) \
    X(int, infcore_get_debug_log, (char*, int)) \
    X(int, infcore_has_tensorrt, (void)) \
    X(int, infcore_list_compute_adapters, (char*, int)) \
    X(int, infcore_set_compute_adapter, (const char*)) \
    X(int, infcore_get_compute_adapter, (char*, int)) \
    X(int, infcore_list_models, (char*, int)) \
    X(InferenceEngineHandle, infcore_create_engine, (const char*)) \
    X(int, infcore_start_inference, (InferenceEngineHandle, const char*, uint32_t)) \
    X(void, infcore_stop_inference, (InferenceEngineHandle)) \
    X(void, infcore_pause_engine, (InferenceEngineHandle)) \
    X(void, infcore_resume_engine, (InferenceEngineHandle)) \
    X(int, infcore_is_paused, (InferenceEngineHandle)) \
    X(void, infcore_destroy_engine, (InferenceEngineHandle)) \
    X(int, infcore_get_results_name, (InferenceEngineHandle, char*, int)) \
    X(int, infcore_get_segmentation_results_name, (InferenceEngineHandle, char*, int)) \
    X(int, infcore_get_complete_event_name, (InferenceEngineHandle, char*, int)) \
    X(int, infcore_get_ocr_results_name, (InferenceEngineHandle, char*, int)) \
    X(int, infcore_get_ocr_complete_event_name, (InferenceEngineHandle, char*, int)) \
    X(int, infcore_get_loaded_model_description, (InferenceEngineHandle, char*, int)) \
    X(int, infcore_get_runtime_error, (InferenceEngineHandle, char*, int)) \
    X(void, infcore_set_confidence, (InferenceEngineHandle, float)) \
    X(void, infcore_set_nms_threshold, (InferenceEngineHandle, float)) \
    X(int, infcore_set_segmentation_mask_threshold, (InferenceEngineHandle, float)) \
    X(void, infcore_set_draw_segmentation_masks, (InferenceEngineHandle, int)) \
    X(void, infcore_set_draw_segmentation_masks_for_classes, (InferenceEngineHandle, int, const int*, int)) \
    X(void, infcore_set_segmentation_mask_opacity, (InferenceEngineHandle, uint8_t)) \
    X(void, infcore_set_color_segmentation_mask, (InferenceEngineHandle, uint8_t, uint8_t, uint8_t)) \
    X(void, infcore_set_color_segmentation_mask_for_classes, (InferenceEngineHandle, uint8_t, uint8_t, uint8_t, const int*, int)) \
    X(void, infcore_clear_roi, (InferenceEngineHandle)) \
    X(void, infcore_set_roi, (InferenceEngineHandle, float, float, float, float, float)) \
    X(void, infcore_set_roi_pixels, (InferenceEngineHandle, float, float, int, int)) \
    X(void, infcore_set_roi_model_size, (InferenceEngineHandle, float, float)) \
    X(int, infcore_set_roi_recommended_size, (InferenceEngineHandle, float, float)) \
    X(int, infcore_set_use_recommended_roi, (InferenceEngineHandle, int)) \
    X(void, infcore_set_draw_detections, (InferenceEngineHandle, int)) \
    X(void, infcore_set_draw_detections_for_classes, (InferenceEngineHandle, int, const int*, int)) \
    X(void, infcore_set_bbox_thickness, (InferenceEngineHandle, int)) \
    X(void, infcore_set_draw_origin_cross, (InferenceEngineHandle, int)) \
    X(void, infcore_set_draw_anchor_point, (InferenceEngineHandle, int)) \
    X(void, infcore_set_target_delay_ms, (InferenceEngineHandle, int)) \
    X(void, infcore_set_draw_roi, (InferenceEngineHandle, int)) \
    X(void, infcore_set_draw_benchmarks, (InferenceEngineHandle, int)) \
    X(void, infcore_set_draw_confidence, (InferenceEngineHandle, int)) \
    X(void, infcore_set_polar_origin, (InferenceEngineHandle, float, float)) \
    X(void, infcore_set_polar_origin_ring, (InferenceEngineHandle, float, const uint32_t*, int, uint32_t)) \
    X(void, infcore_set_limit_detections_to_polar_origin_ring, (InferenceEngineHandle, int)) \
    X(void, infcore_set_polar_origin_ring_focus, (InferenceEngineHandle, int)) \
    X(void, infcore_set_anchor_point, (InferenceEngineHandle, float, float, float, float)) \
    X(void, infcore_set_pose_anchor_for_classes, (InferenceEngineHandle, int, const int*, int)) \
    X(int, infcore_set_anchor_prediction, (InferenceEngineHandle, const InfcoreAnchorPredictionConfig*)) \
    X(int, infcore_record_anchor_prediction_motion, (InferenceEngineHandle, uint64_t, float, float)) \
    X(int, infcore_set_ocr_enabled, (InferenceEngineHandle, int)) \
    X(int, infcore_set_ocr_skip_when_detections, (InferenceEngineHandle, int)) \
    X(void, infcore_set_draw_ocr_region, (InferenceEngineHandle, int)) \
    X(int, infcore_clear_ocr_regions, (InferenceEngineHandle)) \
    X(int, infcore_set_ocr_region_pixels, (InferenceEngineHandle, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)) \
    X(int, infcore_set_ocr_region_normalized, (InferenceEngineHandle, uint32_t, float, float, float, float)) \
    X(int, infcore_set_ocr_region_enabled, (InferenceEngineHandle, uint32_t, int)) \
    X(int, infcore_remove_ocr_region, (InferenceEngineHandle, uint32_t)) \
    X(int, infcore_get_ocr_result, (InferenceEngineHandle, uint32_t, HeliosInferenceOcrResult*)) \
    X(int, infcore_set_color_ocr_region, (InferenceEngineHandle, uint8_t, uint8_t, uint8_t)) \
    X(int, infcore_set_color_ocr_region_id, (InferenceEngineHandle, uint32_t, uint8_t, uint8_t, uint8_t)) \
    X(void, infcore_set_color_bbox, (InferenceEngineHandle, uint8_t, uint8_t, uint8_t)) \
    X(void, infcore_set_color_origin, (InferenceEngineHandle, uint8_t, uint8_t, uint8_t)) \
    X(void, infcore_set_color_anchor, (InferenceEngineHandle, uint8_t, uint8_t, uint8_t)) \
    X(void, infcore_set_color_roi, (InferenceEngineHandle, uint8_t, uint8_t, uint8_t)) \
    X(void, infcore_set_class_priority, (InferenceEngineHandle, const int*, int, const int*, int)) \
    X(int, infcore_set_class_priority_recommended, (InferenceEngineHandle)) \
    X(int, infcore_set_use_recommended_class_priority, (InferenceEngineHandle, int)) \
    X(void, infcore_set_anchor_point_for_classes, (InferenceEngineHandle, float, float, float, float, const int*, int)) \
    X(void, infcore_set_anchor_button_profiles_for_classes, (InferenceEngineHandle, const int*, const float*, int, const int*, int)) \
    X(void, infcore_set_color_bbox_for_classes, (InferenceEngineHandle, uint8_t, uint8_t, uint8_t, const int*, int)) \
    X(void, infcore_set_color_anchor_for_classes, (InferenceEngineHandle, uint8_t, uint8_t, uint8_t, const int*, int)) \
    X(void, infcore_set_confidence_for_classes, (InferenceEngineHandle, float, const int*, int)) \
    X(void, infcore_clear_class_overrides, (InferenceEngineHandle, const int*, int)) \
    X(void, infcore_set_ignore_region, (InferenceEngineHandle, float, float, float, float)) \
    X(void, infcore_set_draw_ignore_region, (InferenceEngineHandle, int)) \
    X(void, infcore_set_color_ignore_region, (InferenceEngineHandle, uint8_t, uint8_t, uint8_t)) \
    X(void, infcore_set_sort_method, (InferenceEngineHandle, int)) \
    X(void, infcore_set_draw_origin_line, (InferenceEngineHandle, int)) \
    X(void, infcore_set_color_origin_line, (InferenceEngineHandle, uint8_t, uint8_t, uint8_t)) \
    X(void, infcore_set_zoom_enabled, (InferenceEngineHandle, int)) \
    X(void, infcore_set_zoom_button_enabled, (InferenceEngineHandle, int)) \
    X(void, infcore_set_zoom_step, (InferenceEngineHandle, float)) \
    X(void, infcore_set_zoom_max, (InferenceEngineHandle, float)) \
    X(void, infcore_set_zoom_hold_ms, (InferenceEngineHandle, int)) \
    X(void, infcore_set_draw_zoomed_roi, (InferenceEngineHandle, int)) \
    X(void, infcore_set_color_zoomed_roi, (InferenceEngineHandle, uint8_t, uint8_t, uint8_t)) \
    X(void, infcore_set_zoom_buttons, (InferenceEngineHandle, uint32_t)) \
    X(void, infcore_set_search_buttons, (InferenceEngineHandle, uint32_t)) \
    X(void, infcore_set_search_button_conditions, (InferenceEngineHandle, const int*, const int*, int)) \
    X(void, infcore_set_search_clauses, (InferenceEngineHandle, const uint32_t*, const uint32_t*, int)) \
    X(void, infcore_trigger_frame, (InferenceEngineHandle, uint64_t)) \
    X(void, infcore_set_draw_keypoints, (InferenceEngineHandle, int)) \
    X(void, infcore_set_draw_keypoints_for_classes, (InferenceEngineHandle, int, const int*, int)) \
    X(void, infcore_set_draw_skeleton, (InferenceEngineHandle, int)) \
    X(void, infcore_set_draw_skeleton_for_classes, (InferenceEngineHandle, int, const int*, int)) \
    X(void, infcore_set_keypoint_conf_threshold, (InferenceEngineHandle, float)) \
    X(void, infcore_set_keypoint_mask, (InferenceEngineHandle, uint32_t)) \
    X(void, infcore_set_color_keypoints, (InferenceEngineHandle, uint8_t, uint8_t, uint8_t)) \
    X(void, infcore_set_color_skeleton, (InferenceEngineHandle, uint8_t, uint8_t, uint8_t)) \
    X(void, infcore_set_color_keypoints_for_classes, (InferenceEngineHandle, uint8_t, uint8_t, uint8_t, const int*, int)) \
    X(void, infcore_set_color_skeleton_for_classes, (InferenceEngineHandle, uint8_t, uint8_t, uint8_t, const int*, int)) \
    X(void, infcore_set_keypoint_radius, (InferenceEngineHandle, int)) \
    X(int, infcore_load_model, (InferenceEngineHandle, const char*)) \
    X(int, infcore_unload_model, (InferenceEngineHandle))

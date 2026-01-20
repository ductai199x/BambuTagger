#pragma once

#include "bambu_tagger.h"

// Scene handler declarations
void scene_main_menu_on_enter(void* context);
bool scene_main_menu_on_event(void* context, SceneManagerEvent event);
void scene_main_menu_on_exit(void* context);

void scene_select_filament_on_enter(void* context);
bool scene_select_filament_on_event(void* context, SceneManagerEvent event);
void scene_select_filament_on_exit(void* context);

void scene_select_color_on_enter(void* context);
bool scene_select_color_on_event(void* context, SceneManagerEvent event);
void scene_select_color_on_exit(void* context);

void scene_select_weight_on_enter(void* context);
bool scene_select_weight_on_event(void* context, SceneManagerEvent event);
void scene_select_weight_on_exit(void* context);

void scene_confirm_on_enter(void* context);
bool scene_confirm_on_event(void* context, SceneManagerEvent event);
void scene_confirm_on_exit(void* context);

void scene_scan_tag_on_enter(void* context);
bool scene_scan_tag_on_event(void* context, SceneManagerEvent event);
void scene_scan_tag_on_exit(void* context);

void scene_write_tag_on_enter(void* context);
bool scene_write_tag_on_event(void* context, SceneManagerEvent event);
void scene_write_tag_on_exit(void* context);

void scene_result_on_enter(void* context);
bool scene_result_on_event(void* context, SceneManagerEvent event);
void scene_result_on_exit(void* context);

void scene_read_tag_scan_on_enter(void* context);
bool scene_read_tag_scan_on_event(void* context, SceneManagerEvent event);
void scene_read_tag_scan_on_exit(void* context);

void scene_read_tag_result_on_enter(void* context);
bool scene_read_tag_result_on_event(void* context, SceneManagerEvent event);
void scene_read_tag_result_on_exit(void* context);

void scene_saved_tags_on_enter(void* context);
bool scene_saved_tags_on_event(void* context, SceneManagerEvent event);
void scene_saved_tags_on_exit(void* context);

void scene_saved_tag_view_on_enter(void* context);
bool scene_saved_tag_view_on_event(void* context, SceneManagerEvent event);
void scene_saved_tag_view_on_exit(void* context);

// Helper function for extracting strings from block data
void extract_string(const uint8_t* data, size_t offset, size_t max_len, char* out);

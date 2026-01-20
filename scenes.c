#include "scenes.h"
#include "nfc_operations.h"
#include "tag_storage.h"

// ============================================
// Scene handler arrays
// ============================================
const SceneManagerHandlers scene_handlers = {
    .on_enter_handlers =
        (void (*const[])(void*)){
            scene_main_menu_on_enter,
            scene_select_filament_on_enter,
            scene_select_color_on_enter,
            scene_select_weight_on_enter,
            scene_confirm_on_enter,
            scene_scan_tag_on_enter,
            scene_write_tag_on_enter,
            scene_result_on_enter,
            scene_read_tag_scan_on_enter,
            scene_read_tag_result_on_enter,
            scene_saved_tags_on_enter,
            scene_saved_tag_view_on_enter,
        },
    .on_event_handlers =
        (bool (*const[])(void*, SceneManagerEvent)){
            scene_main_menu_on_event,
            scene_select_filament_on_event,
            scene_select_color_on_event,
            scene_select_weight_on_event,
            scene_confirm_on_event,
            scene_scan_tag_on_event,
            scene_write_tag_on_event,
            scene_result_on_event,
            scene_read_tag_scan_on_event,
            scene_read_tag_result_on_event,
            scene_saved_tags_on_event,
            scene_saved_tag_view_on_event,
        },
    .on_exit_handlers =
        (void (*const[])(void*)){
            scene_main_menu_on_exit,
            scene_select_filament_on_exit,
            scene_select_color_on_exit,
            scene_select_weight_on_exit,
            scene_confirm_on_exit,
            scene_scan_tag_on_exit,
            scene_write_tag_on_exit,
            scene_result_on_exit,
            scene_read_tag_scan_on_exit,
            scene_read_tag_result_on_exit,
            scene_saved_tags_on_exit,
            scene_saved_tag_view_on_exit,
        },
    .scene_num = SceneCount,
};

// Helper to extract null-terminated string from block data
void extract_string(const uint8_t* data, size_t offset, size_t max_len, char* out) {
    size_t i;
    for(i = 0; i < max_len && data[offset + i] != 0; i++) {
        out[i] = (char)data[offset + i];
    }
    out[i] = '\0';
}

// ============================================
// Scene: Main Menu
// ============================================
static void main_menu_callback(void* context, uint32_t index) {
    App* app = context;
    if(index == 0) {
        view_dispatcher_send_custom_event(app->view_dispatcher, EventMainMenuRead);
    } else if(index == 1) {
        view_dispatcher_send_custom_event(app->view_dispatcher, EventMainMenuProgram);
    } else if(index == 2) {
        view_dispatcher_send_custom_event(app->view_dispatcher, EventMainMenuSaved);
    }
}

void scene_main_menu_on_enter(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Bambu Tagger");
    submenu_add_item(app->submenu, "Read Tag", 0, main_menu_callback, app);
    submenu_add_item(app->submenu, "Program Tag", 1, main_menu_callback, app);
    submenu_add_item(app->submenu, "Saved Tags", 2, main_menu_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewSubmenu);
}

bool scene_main_menu_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == EventMainMenuRead) {
            // Start reading
            scene_manager_next_scene(app->scene_manager, SceneReadTagScan);
            consumed = true;
        } else if(event.event == EventMainMenuProgram) {
            // Initialize defaults
            app->tag_data.filament_index = 0;
            app->tag_data.color_index = 0;
            app->tag_data.weight_grams = 1000;
            app->use_saved_tag = false;
            scene_manager_next_scene(app->scene_manager, SceneSelectFilament);
            consumed = true;
        } else if(event.event == EventMainMenuSaved) {
            // Show saved tags
            scene_manager_next_scene(app->scene_manager, SceneSavedTags);
            consumed = true;
        }
    }
    return consumed;
}

void scene_main_menu_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}

// ============================================
// Scene: Select Filament
// ============================================
static void filament_menu_callback(void* context, uint32_t index) {
    App* app = context;
    app->tag_data.filament_index = index;
    view_dispatcher_send_custom_event(app->view_dispatcher, EventFilamentSelected);
}

void scene_select_filament_on_enter(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Select Filament");

    for(size_t i = 0; i < BAMBU_FILAMENT_COUNT; i++) {
        submenu_add_item(
            app->submenu, BAMBU_FILAMENTS[i].display_name, i, filament_menu_callback, app);
    }

    submenu_set_selected_item(app->submenu, app->tag_data.filament_index);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewSubmenu);
}

bool scene_select_filament_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == EventFilamentSelected) {
            scene_manager_next_scene(app->scene_manager, SceneSelectColor);
            consumed = true;
        }
    }
    return consumed;
}

void scene_select_filament_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}

// ============================================
// Scene: Select Color
// ============================================
static void color_menu_callback(void* context, uint32_t index) {
    App* app = context;
    app->tag_data.color_index = index;
    view_dispatcher_send_custom_event(app->view_dispatcher, EventColorSelected);
}

void scene_select_color_on_enter(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Select Color");

    for(size_t i = 0; i < COLOR_PRESET_COUNT; i++) {
        submenu_add_item(app->submenu, COLOR_PRESETS[i].name, i, color_menu_callback, app);
    }

    submenu_set_selected_item(app->submenu, app->tag_data.color_index);
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewSubmenu);
}

bool scene_select_color_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == EventColorSelected) {
            scene_manager_next_scene(app->scene_manager, SceneSelectWeight);
            consumed = true;
        }
    }
    return consumed;
}

void scene_select_color_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}

// ============================================
// Scene: Select Weight
// ============================================
static void weight_changed_callback(VariableItem* item) {
    App* app = variable_item_get_context(item);
    uint8_t index = variable_item_get_current_value_index(item);

    app->tag_data.weight_grams = WEIGHT_PRESETS[index];

    char weight_str[16];
    snprintf(weight_str, sizeof(weight_str), "%d g", app->tag_data.weight_grams);
    variable_item_set_current_value_text(item, weight_str);
}

static void weight_enter_callback(void* context, uint32_t index) {
    UNUSED(index);
    App* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, EventWeightSelected);
}

void scene_select_weight_on_enter(void* context) {
    App* app = context;
    variable_item_list_reset(app->variable_item_list);

    VariableItem* item = variable_item_list_add(
        app->variable_item_list, "Spool Weight", WEIGHT_PRESET_COUNT, weight_changed_callback, app);

    // Find current weight index
    uint8_t weight_index = 2; // Default to 1000g
    for(size_t i = 0; i < WEIGHT_PRESET_COUNT; i++) {
        if(WEIGHT_PRESETS[i] == app->tag_data.weight_grams) {
            weight_index = i;
            break;
        }
    }

    variable_item_set_current_value_index(item, weight_index);

    char weight_str[16];
    snprintf(weight_str, sizeof(weight_str), "%d g", app->tag_data.weight_grams);
    variable_item_set_current_value_text(item, weight_str);

    variable_item_list_set_enter_callback(app->variable_item_list, weight_enter_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, ViewVariableItemList);
}

bool scene_select_weight_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == EventWeightSelected) {
            scene_manager_next_scene(app->scene_manager, SceneConfirm);
            consumed = true;
        }
    }
    return consumed;
}

void scene_select_weight_on_exit(void* context) {
    App* app = context;
    variable_item_list_reset(app->variable_item_list);
}

// ============================================
// Scene: Confirm
// ============================================
static void confirm_button_callback(GuiButtonType result, InputType type, void* context) {
    App* app = context;
    if(type == InputTypeShort && result == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, EventConfirmed);
    }
}

void scene_confirm_on_enter(void* context) {
    App* app = context;
    widget_reset(app->widget);

    const FilamentInfo* filament = &BAMBU_FILAMENTS[app->tag_data.filament_index];
    const ColorPreset* color = &COLOR_PRESETS[app->tag_data.color_index];

    FuriString* text = furi_string_alloc();
    furi_string_printf(
        text,
        "Filament: %s\n"
        "Type: %s\n"
        "Color: %s\n"
        "Weight: %d g",
        filament->display_name,
        filament->filament_type,
        color->name,
        app->tag_data.weight_grams);

    // Leave room for button at bottom
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 52, furi_string_get_cstr(text));
    furi_string_free(text);

    // Add scan button
    widget_add_button_element(
        app->widget, GuiButtonTypeRight, "Scan", confirm_button_callback, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
}

bool scene_confirm_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == EventConfirmed) {
            // Set flags for writing to blank tag
            app->use_saved_tag = false;
            app->write_to_blank = true;  // Use default key for blank tags
            scene_manager_next_scene(app->scene_manager, SceneScanTag);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        // Allow back navigation
        consumed = false;
    }
    return consumed;
}

void scene_confirm_on_exit(void* context) {
    App* app = context;
    widget_reset(app->widget);
}

// ============================================
// Scene: Scan Tag
// ============================================
void scene_scan_tag_on_enter(void* context) {
    App* app = context;

    app->card_detected = false;
    app->uid_read = false;
    app->detection_in_progress = false;
    app->detected_tag_type = TagTypeUnknown;

    widget_reset(app->widget);
    widget_add_text_scroll_element(
        app->widget, 0, 0, 128, 64, "Place tag on\nFlipper's back\n\nScanning...");
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);

    // Start scanner
    app->scanner = nfc_scanner_alloc(app->nfc);
    nfc_scanner_start(app->scanner, scanner_callback, app);
}

bool scene_scan_tag_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeTick) {
        // Check if card detected
        if(app->card_detected && !app->uid_read) {
            // Stop scanner and start UID read
            nfc_scanner_stop(app->scanner);
            nfc_scanner_free(app->scanner);
            app->scanner = NULL;

            widget_reset(app->widget);
            widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, "Reading UID...");

            // Start ISO14443-3A poller to read UID
            app->poller = nfc_poller_alloc(app->nfc, NfcProtocolIso14443_3a);
            nfc_poller_start(app->poller, uid_poller_callback, app);
        }

        if(app->uid_read && !app->detection_in_progress && app->detected_tag_type == TagTypeUnknown) {
            // UID read successful, start tag type detection
            if(app->poller) {
                nfc_poller_stop(app->poller);
                nfc_poller_free(app->poller);
                app->poller = NULL;
            }

            // Calculate keys from UID
            calculate_all_keys(app->tag_data.uid, app->tag_data.uid_len, &app->derived_keys);

            FURI_LOG_I(
                TAG,
                "UID: %02X %02X %02X %02X",
                app->tag_data.uid[0],
                app->tag_data.uid[1],
                app->tag_data.uid[2],
                app->tag_data.uid[3]);
            FURI_LOG_I(
                TAG,
                "Key[0]: %02X %02X %02X %02X %02X %02X",
                app->derived_keys.keys[0][0],
                app->derived_keys.keys[0][1],
                app->derived_keys.keys[0][2],
                app->derived_keys.keys[0][3],
                app->derived_keys.keys[0][4],
                app->derived_keys.keys[0][5]);

            // Start tag type detection
            widget_reset(app->widget);
            widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, "Detecting tag type...");

            app->detection_in_progress = true;
            app->poller = nfc_poller_alloc(app->nfc, NfcProtocolMfClassic);
            nfc_poller_start(app->poller, detect_tag_type_callback, app);
        }

        // Check detection result
        if(app->uid_read && !app->detection_in_progress && app->detected_tag_type != TagTypeUnknown) {
            if(app->poller) {
                nfc_poller_stop(app->poller);
                nfc_poller_free(app->poller);
                app->poller = NULL;
            }

            if(app->detected_tag_type == TagTypeBambu) {
                // Show error - cannot reprogram Bambu tags
                app->detected_tag_type = TagTypeUnknown;  // Prevent retriggering
                app->detection_in_progress = true;  // Block re-entry
                widget_reset(app->widget);
                widget_add_text_scroll_element(
                    app->widget,
                    0,
                    0,
                    128,
                    64,
                    "Bambu Tag Detected!\n\n"
                    "This tag has read-only\n"
                    "access bits and cannot\n"
                    "be reprogrammed.\n\n"
                    "Use a blank MIFARE\n"
                    "Classic 1K tag.");
                notification_message(app->notifications, &sequence_error);
                consumed = true;
            } else {
                // Blank tag - proceed to write
                scene_manager_next_scene(app->scene_manager, SceneWriteTag);
                consumed = true;
            }
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        // Clean up scanner if running
        if(app->scanner) {
            nfc_scanner_stop(app->scanner);
            nfc_scanner_free(app->scanner);
            app->scanner = NULL;
        }
        if(app->poller) {
            nfc_poller_stop(app->poller);
            nfc_poller_free(app->poller);
            app->poller = NULL;
        }
        consumed = false;
    }
    return consumed;
}

void scene_scan_tag_on_exit(void* context) {
    App* app = context;
    widget_reset(app->widget);
}

// ============================================
// Scene: Write Tag
// ============================================
void scene_write_tag_on_enter(void* context) {
    App* app = context;

    app->write_success = false;
    app->write_in_progress = true;

    widget_reset(app->widget);
    widget_add_text_scroll_element(
        app->widget, 0, 0, 128, 64, "Writing tag...\n\nKeep tag on\nFlipper's back");
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);

    // Start Mifare Classic poller for writing
    app->poller = nfc_poller_alloc(app->nfc, NfcProtocolMfClassic);
    nfc_poller_start(app->poller, write_poller_callback, app);
}

bool scene_write_tag_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeTick) {
        if(!app->write_in_progress) {
            // Write completed
            if(app->poller) {
                nfc_poller_stop(app->poller);
                nfc_poller_free(app->poller);
                app->poller = NULL;
            }

            scene_manager_next_scene(app->scene_manager, SceneResult);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        // Don't allow back during write
        consumed = true;
    }
    return consumed;
}

void scene_write_tag_on_exit(void* context) {
    App* app = context;
    widget_reset(app->widget);

    if(app->poller) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
        app->poller = NULL;
    }
}

// ============================================
// Scene: Result
// ============================================
void scene_result_on_enter(void* context) {
    App* app = context;

    popup_reset(app->popup);

    if(app->write_success) {
        popup_set_header(app->popup, "Success!", 64, 20, AlignCenter, AlignBottom);
        popup_set_text(app->popup, "Tag programmed\nsuccessfully!", 64, 40, AlignCenter, AlignBottom);
        notification_message(app->notifications, &sequence_success);
    } else {
        popup_set_header(app->popup, "Write Failed", 64, 20, AlignCenter, AlignBottom);
        popup_set_text(
            app->popup,
            "Auth error or\nnot Mifare Classic",
            64,
            40,
            AlignCenter,
            AlignBottom);
        notification_message(app->notifications, &sequence_error);
    }

    popup_set_timeout(app->popup, 3000);
    popup_set_context(app->popup, app);
    popup_enable_timeout(app->popup);

    view_dispatcher_switch_to_view(app->view_dispatcher, ViewPopup);
}

bool scene_result_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        // Go back to main menu
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, SceneMainMenu);
        consumed = true;
    }
    return consumed;
}

void scene_result_on_exit(void* context) {
    App* app = context;
    popup_reset(app->popup);
}

// ============================================
// Scene: Read Tag Scan
// ============================================
void scene_read_tag_scan_on_enter(void* context) {
    App* app = context;

    app->card_detected = false;
    app->uid_read = false;
    app->read_success = false;
    app->read_in_progress = false;
    memset(&app->read_data, 0, sizeof(ReadTagData));  // Clear all read data for multi-pass
    g_current_read_sector = 0;  // Start with sector 0

    widget_reset(app->widget);
    widget_add_text_scroll_element(
        app->widget, 0, 0, 128, 64, "Place tag on\nFlipper's back\n\nScanning...");
    view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);

    // Start scanner
    app->scanner = nfc_scanner_alloc(app->nfc);
    nfc_scanner_start(app->scanner, scanner_callback, app);
}

bool scene_read_tag_scan_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeTick) {
        // Debug: log state
        FURI_LOG_D(
            TAG,
            "Tick: detected=%d uid_read=%d in_progress=%d",
            app->card_detected,
            app->uid_read,
            app->read_in_progress);

        // Check if card detected and we haven't started UID read yet
        // Also check that poller is NULL to prevent starting multiple pollers
        if(app->card_detected && !app->uid_read && !app->read_in_progress && app->poller == NULL) {
            FURI_LOG_I(TAG, "Card detected, starting UID read");
            // Stop scanner and start UID read
            if(app->scanner) {
                nfc_scanner_stop(app->scanner);
                nfc_scanner_free(app->scanner);
                app->scanner = NULL;
            }

            widget_reset(app->widget);
            widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, "Reading UID...");

            // Start ISO14443-3A poller to read UID
            app->poller = nfc_poller_alloc(app->nfc, NfcProtocolIso14443_3a);
            nfc_poller_start(app->poller, uid_poller_callback, app);
        }

        if(app->uid_read && !app->read_in_progress && app->poller == NULL) {
            // Check what data we have
            bool has_sector0 = (app->read_data.block1[0] != 0 || app->read_data.block2[0] != 0);
            bool has_sector1 = (app->read_data.block4[0] != 0 || app->read_data.block5[0] != 0);

            // First time after UID read - calculate keys
            if(!has_sector0 && !has_sector1 && !app->read_success) {
                calculate_all_keys(app->tag_data.uid, app->tag_data.uid_len, &app->derived_keys);
                FURI_LOG_I(TAG, "Keys calculated for UID: %02X%02X%02X%02X",
                    app->tag_data.uid[0], app->tag_data.uid[1],
                    app->tag_data.uid[2], app->tag_data.uid[3]);
            }

            if(!has_sector0) {
                // Pass 1: Read sector 0
                g_current_read_sector = 0;
                widget_reset(app->widget);
                widget_add_text_scroll_element(
                    app->widget, 0, 0, 128, 64, "Reading sector 0...\n\nKeep tag on\nFlipper's back");

                FURI_LOG_I(TAG, "Starting Pass 1: Sector 0");
                app->read_in_progress = true;
                app->poller = nfc_poller_alloc(app->nfc, NfcProtocolMfClassic);
                nfc_poller_start(app->poller, read_poller_callback, app);
            } else if(!has_sector1) {
                // Pass 2: Read sector 1
                g_current_read_sector = 1;
                widget_reset(app->widget);
                widget_add_text_scroll_element(
                    app->widget, 0, 0, 128, 64, "Reading sector 1...\n\nKeep tag on\nFlipper's back");

                FURI_LOG_I(TAG, "Starting Pass 2: Sector 1");
                app->read_in_progress = true;
                app->poller = nfc_poller_alloc(app->nfc, NfcProtocolMfClassic);
                nfc_poller_start(app->poller, read_poller_callback, app);
            } else {
                // Both sectors read, done!
                app->read_data.valid = true;
                app->read_success = true;
                FURI_LOG_I(TAG, "Both sectors read successfully!");
                scene_manager_next_scene(app->scene_manager, SceneReadTagResult);
                consumed = true;
            }
        }

        // Handle poller completion
        if(!app->read_in_progress && app->poller != NULL) {
            nfc_poller_stop(app->poller);
            nfc_poller_free(app->poller);
            app->poller = NULL;
            FURI_LOG_I(TAG, "Poller stopped, checking progress...");
            // Next tick will check if we need another pass
        }

        // If we have at least sector 0 and failed to get sector 1, still show results
        if(!app->read_in_progress && app->poller == NULL) {
            bool has_sector0 = (app->read_data.block1[0] != 0 || app->read_data.block2[0] != 0);
            if(has_sector0 && app->read_success) {
                app->read_data.valid = true;
                scene_manager_next_scene(app->scene_manager, SceneReadTagResult);
                consumed = true;
            }
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        // Clean up
        if(app->scanner) {
            nfc_scanner_stop(app->scanner);
            nfc_scanner_free(app->scanner);
            app->scanner = NULL;
        }
        if(app->poller) {
            nfc_poller_stop(app->poller);
            nfc_poller_free(app->poller);
            app->poller = NULL;
        }
        consumed = false;
    }
    return consumed;
}

void scene_read_tag_scan_on_exit(void* context) {
    App* app = context;
    widget_reset(app->widget);

    if(app->scanner) {
        nfc_scanner_stop(app->scanner);
        nfc_scanner_free(app->scanner);
        app->scanner = NULL;
    }
    if(app->poller) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
        app->poller = NULL;
    }
}

// ============================================
// Scene: Read Tag Result
// ============================================
static void read_result_button_callback(GuiButtonType result, InputType type, void* context) {
    App* app = context;
    if(type == InputTypeShort) {
        if(result == GuiButtonTypeRight) {
            view_dispatcher_send_custom_event(app->view_dispatcher, EventSaveTag);
        } else if(result == GuiButtonTypeLeft) {
            view_dispatcher_send_custom_event(app->view_dispatcher, EventBack);
        }
    }
}

void scene_read_tag_result_on_enter(void* context) {
    App* app = context;
    widget_reset(app->widget);

    FuriString* text = furi_string_alloc();

    if(app->read_data.valid) {
        // Extract material ID from block 1 (bytes 8-15)
        char material_id[9];
        extract_string(app->read_data.block1, 8, 8, material_id);

        // Extract filament type from block 2
        char filament_type[17];
        extract_string(app->read_data.block2, 0, 16, filament_type);

        // Extract detailed type from block 4
        char detailed_type[17];
        extract_string(app->read_data.block4, 0, 16, detailed_type);

        // Extract color from block 5 (bytes 0-3: RGBA)
        uint8_t r = app->read_data.block5[0];
        uint8_t g = app->read_data.block5[1];
        uint8_t b = app->read_data.block5[2];

        // Extract weight from block 5 (bytes 4-5: little endian)
        uint16_t weight = app->read_data.block5[4] | (app->read_data.block5[5] << 8);

        // Format UID
        char uid_str[32];
        snprintf(
            uid_str,
            sizeof(uid_str),
            "%02X:%02X:%02X:%02X",
            app->tag_data.uid[0],
            app->tag_data.uid[1],
            app->tag_data.uid[2],
            app->tag_data.uid[3]);

        furi_string_printf(
            text,
            "UID: %s\n"
            "ID: %s\n"
            "Type: %s\n"
            "Detail: %s\n"
            "Color: #%02X%02X%02X\n"
            "Weight: %d g",
            uid_str,
            material_id[0] ? material_id : "(empty)",
            filament_type[0] ? filament_type : "(empty)",
            detailed_type[0] ? detailed_type : "(empty)",
            r, g, b,
            weight);

        notification_message(app->notifications, &sequence_success);
    } else {
        furi_string_printf(
            text,
            "Read Failed!\n\n"
            "Could not authenticate\n"
            "or read tag blocks.\n\n"
            "Is this a Bambu tag?");

        notification_message(app->notifications, &sequence_error);
    }

    // Leave room for buttons at bottom (height 52 instead of 64)
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 52, furi_string_get_cstr(text));
    furi_string_free(text);

    // Add button elements
    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "Back", read_result_button_callback, app);
    if(app->read_data.valid) {
        widget_add_button_element(
            app->widget, GuiButtonTypeRight, "Save", read_result_button_callback, app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
}

bool scene_read_tag_result_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == EventSaveTag) {
            if(save_tag_to_file(app)) {
                notification_message(app->notifications, &sequence_success);
                // Show saved message briefly then go back
                popup_reset(app->popup);
                popup_set_header(app->popup, "Saved!", 64, 20, AlignCenter, AlignBottom);
                popup_set_text(app->popup, "Tag saved to SD card", 64, 40, AlignCenter, AlignBottom);
                popup_set_timeout(app->popup, 1500);
                popup_enable_timeout(app->popup);
                view_dispatcher_switch_to_view(app->view_dispatcher, ViewPopup);
            } else {
                notification_message(app->notifications, &sequence_error);
            }
            consumed = true;
        } else if(event.event == EventBack) {
            scene_manager_search_and_switch_to_previous_scene(app->scene_manager, SceneMainMenu);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        // Go back to main menu
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, SceneMainMenu);
        consumed = true;
    }
    return consumed;
}

void scene_read_tag_result_on_exit(void* context) {
    App* app = context;
    widget_reset(app->widget);
}

// ============================================
// Scene: Saved Tags List
// ============================================
static void saved_tags_callback(void* context, uint32_t index) {
    App* app = context;
    if(index < app->saved_tags_count) {
        // Store selected tag path
        furi_string_printf(
            app->saved_tag_path,
            "%s/%s",
            BAMBU_TAGGER_FOLDER,
            app->saved_tags[index]);
        view_dispatcher_send_custom_event(app->view_dispatcher, EventSavedTagSelected);
    }
}

void scene_saved_tags_on_enter(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Saved Tags");

    // Load list of saved tags
    load_saved_tags_list(app);

    if(app->saved_tags_count == 0) {
        submenu_add_item(app->submenu, "(No saved tags)", 0, NULL, app);
    } else {
        for(uint8_t i = 0; i < app->saved_tags_count; i++) {
            // Remove extension for display
            char display_name[64];
            strncpy(display_name, app->saved_tags[i], 63);
            display_name[63] = '\0';
            char* ext = strstr(display_name, BAMBU_TAGGER_EXTENSION);
            if(ext) *ext = '\0';

            submenu_add_item(app->submenu, display_name, i, saved_tags_callback, app);
        }
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, ViewSubmenu);
}

bool scene_saved_tags_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == EventSavedTagSelected) {
            scene_manager_next_scene(app->scene_manager, SceneSavedTagView);
            consumed = true;
        }
    }
    return consumed;
}

void scene_saved_tags_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}

// ============================================
// Scene: Saved Tag View
// ============================================
static void saved_tag_view_button_callback(GuiButtonType result, InputType type, void* context) {
    App* app = context;
    if(type == InputTypeShort) {
        if(result == GuiButtonTypeRight) {
            view_dispatcher_send_custom_event(app->view_dispatcher, EventProgramSavedTag);
        } else if(result == GuiButtonTypeCenter) {
            view_dispatcher_send_custom_event(app->view_dispatcher, EventDeleteTag);
        } else if(result == GuiButtonTypeLeft) {
            view_dispatcher_send_custom_event(app->view_dispatcher, EventBack);
        }
    }
}

void scene_saved_tag_view_on_enter(void* context) {
    App* app = context;
    widget_reset(app->widget);

    FuriString* text = furi_string_alloc();

    // Load tag data
    if(load_tag_from_file(app, furi_string_get_cstr(app->saved_tag_path))) {
        // Extract info for display
        char material_id[9];
        extract_string(app->read_data.block1, 8, 8, material_id);
        char filament_type[17];
        extract_string(app->read_data.block2, 0, 16, filament_type);
        char detailed_type[17];
        extract_string(app->read_data.block4, 0, 16, detailed_type);
        uint8_t r = app->read_data.block5[0];
        uint8_t g = app->read_data.block5[1];
        uint8_t b = app->read_data.block5[2];
        uint16_t weight = app->read_data.block5[4] | (app->read_data.block5[5] << 8);

        char uid_str[32];
        snprintf(
            uid_str,
            sizeof(uid_str),
            "%02X:%02X:%02X:%02X",
            app->tag_data.uid[0],
            app->tag_data.uid[1],
            app->tag_data.uid[2],
            app->tag_data.uid[3]);

        furi_string_printf(
            text,
            "UID: %s\n"
            "Type: %s\n"
            "Detail: %s\n"
            "Color: #%02X%02X%02X\n"
            "Weight: %d g",
            uid_str,
            filament_type[0] ? filament_type : "(empty)",
            detailed_type[0] ? detailed_type : "(empty)",
            r, g, b,
            weight);
    } else {
        furi_string_printf(text, "Failed to load tag!");
    }

    // Leave room for buttons at bottom
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 52, furi_string_get_cstr(text));
    furi_string_free(text);

    // Add button elements
    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "Back", saved_tag_view_button_callback, app);
    widget_add_button_element(
        app->widget, GuiButtonTypeCenter, "Delete", saved_tag_view_button_callback, app);
    if(app->read_data.valid) {
        widget_add_button_element(
            app->widget, GuiButtonTypeRight, "Clone", saved_tag_view_button_callback, app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, ViewWidget);
}

bool scene_saved_tag_view_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == EventProgramSavedTag) {
            // Set flags for cloning to blank tag
            app->use_saved_tag = true;
            app->write_to_blank = true;  // Use default key for authentication
            scene_manager_next_scene(app->scene_manager, SceneScanTag);
            consumed = true;
        } else if(event.event == EventDeleteTag) {
            // Extract filename from path
            const char* path = furi_string_get_cstr(app->saved_tag_path);
            const char* filename = strrchr(path, '/');
            if(filename) {
                filename++; // Skip the '/'
                delete_saved_tag(app, filename);
            }
            // Go back to saved tags list
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        } else if(event.event == EventBack) {
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }
    return consumed;
}

void scene_saved_tag_view_on_exit(void* context) {
    App* app = context;
    widget_reset(app->widget);
}

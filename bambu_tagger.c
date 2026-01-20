/**
 * @file bambu_tagger.c
 * @brief Main entry point and app lifecycle for Bambu Tagger
 * @author Tai Nguyen <taiducnguyen.drexel@gmail.com>
 */

#include "bambu_tagger.h"
#include "scenes.h"

// ============================================
// View Dispatcher callbacks
// ============================================
static bool app_navigation_callback(void* context) {
    App* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static bool app_custom_event_callback(void* context, uint32_t event) {
    App* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static void app_tick_event_callback(void* context) {
    App* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

// ============================================
// Application allocation/free
// ============================================
static App* app_alloc(void) {
    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));

    // Initialize GUI
    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    // Allocate view dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, app_navigation_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // Allocate scene manager
    app->scene_manager = scene_manager_alloc(&scene_handlers, app);

    // Allocate views
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, ViewSubmenu, submenu_get_view(app->submenu));

    app->variable_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        ViewVariableItemList,
        variable_item_list_get_view(app->variable_item_list));

    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, ViewWidget, widget_get_view(app->widget));

    app->popup = popup_alloc();
    view_dispatcher_add_view(app->view_dispatcher, ViewPopup, popup_get_view(app->popup));

    // Allocate NFC
    app->nfc = nfc_alloc();

    // Allocate MfClassic data structure for read/write operations
    app->mf_data = mf_classic_alloc();

    // Allocate storage
    app->storage = furi_record_open(RECORD_STORAGE);
    app->saved_tag_path = furi_string_alloc();

    // Initialize tag data defaults
    app->tag_data.filament_index = 0;
    app->tag_data.color_index = 0;
    app->tag_data.weight_grams = 1000;
    app->use_saved_tag = false;

    return app;
}

static void app_free(App* app) {
    // Free views
    view_dispatcher_remove_view(app->view_dispatcher, ViewSubmenu);
    submenu_free(app->submenu);

    view_dispatcher_remove_view(app->view_dispatcher, ViewVariableItemList);
    variable_item_list_free(app->variable_item_list);

    view_dispatcher_remove_view(app->view_dispatcher, ViewWidget);
    widget_free(app->widget);

    view_dispatcher_remove_view(app->view_dispatcher, ViewPopup);
    popup_free(app->popup);

    // Free scene manager and view dispatcher
    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    // Free MfClassic data
    if(app->mf_data) {
        mf_classic_free(app->mf_data);
    }

    // Free NFC
    nfc_free(app->nfc);

    // Free storage
    furi_string_free(app->saved_tag_path);
    furi_record_close(RECORD_STORAGE);

    // Close records
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);

    free(app);
}

// ============================================
// Main entry point
// ============================================
int32_t bambu_tagger_app(void* p) {
    UNUSED(p);

    App* app = app_alloc();

    // Set tick event for polling (100ms interval)
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher,
        app_tick_event_callback,
        100);

    // Start with main menu
    scene_manager_next_scene(app->scene_manager, SceneMainMenu);

    // Run event loop
    view_dispatcher_run(app->view_dispatcher);

    app_free(app);

    return 0;
}

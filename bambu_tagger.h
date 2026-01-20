#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/popup.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <dialogs/dialogs.h>
#include <nfc/nfc.h>
#include <nfc/nfc_scanner.h>
#include <nfc/nfc_poller.h>
#include <nfc/protocols/mf_classic/mf_classic_poller.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>

#include "bambu_crypto.h"
#include "bambu_tag_data.h"

#define TAG "BambuTagger"
#define BAMBU_TAGGER_FOLDER EXT_PATH("apps_data/bambu_tagger")
#define BAMBU_TAGGER_EXTENSION ".btag"
#define MAX_SAVED_TAGS 32

// ============================================
// Scene definitions
// ============================================
typedef enum {
    SceneMainMenu,
    SceneSelectFilament,
    SceneSelectColor,
    SceneSelectWeight,
    SceneConfirm,
    SceneScanTag,
    SceneWriteTag,
    SceneResult,
    SceneReadTagScan,
    SceneReadTagResult,
    SceneSavedTags,
    SceneSavedTagView,
    SceneCount
} AppScene;

// ============================================
// View definitions
// ============================================
typedef enum {
    ViewSubmenu,
    ViewVariableItemList,
    ViewWidget,
    ViewPopup,
} AppView;

// ============================================
// Custom events
// ============================================
typedef enum {
    EventMainMenuProgram,
    EventMainMenuRead,
    EventMainMenuSaved,
    EventFilamentSelected,
    EventColorSelected,
    EventWeightSelected,
    EventConfirmed,
    EventTagDetected,
    EventWriteSuccess,
    EventWriteFailed,
    EventReadSuccess,
    EventReadFailed,
    EventSaveTag,
    EventSavedTagSelected,
    EventDeleteTag,
    EventProgramSavedTag,
    EventBack,
} AppEvent;

// ============================================
// Tag type detection
// ============================================
typedef enum {
    TagTypeUnknown,
    TagTypeBambu,
    TagTypeBlank,
} TagType;

// ============================================
// Read tag result data
// ============================================
typedef struct {
    uint8_t block1[16];   // Material variant + Material ID
    uint8_t block2[16];   // Filament type
    uint8_t block4[16];   // Detailed type
    uint8_t block5[16];   // Color RGBA + Weight
    bool valid;
} ReadTagData;

// ============================================
// Application context
// ============================================
typedef struct {
    // GUI components
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    Submenu* submenu;
    VariableItemList* variable_item_list;
    Widget* widget;
    Popup* popup;
    NotificationApp* notifications;

    // NFC components
    Nfc* nfc;
    NfcScanner* scanner;
    NfcPoller* poller;

    // Tag programming data
    TagProgramData tag_data;
    BambuKeys derived_keys;

    // Read tag data
    ReadTagData read_data;

    // MfClassic data for read/write operations
    MfClassicData* mf_data;

    // State flags
    bool card_detected;
    bool uid_read;
    bool write_success;
    bool write_in_progress;
    bool read_success;
    bool read_in_progress;

    // Saved tags
    Storage* storage;
    FuriString* saved_tag_path;  // Currently selected saved tag path
    char saved_tags[MAX_SAVED_TAGS][64];  // List of saved tag filenames
    uint8_t saved_tags_count;
    bool use_saved_tag;  // Flag to use loaded tag data for programming
    bool write_to_blank;  // Flag to use default key for blank tags
    TagType detected_tag_type;  // Result of tag type detection
    bool detection_in_progress;  // Flag for detection phase

    // Message queue for async events
    FuriMessageQueue* event_queue;
} App;

// Scene handler declarations (defined in scenes.c)
extern const SceneManagerHandlers scene_handlers;

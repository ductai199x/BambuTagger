# Development Guide: Lessons Learned

This document captures hard-learned lessons from building the Bambu Tagger Flipper Zero application. If you're new to Flipper Zero development, NFC programming, or embedded systems, read this carefully.

## Table of Contents
1. [NFC Resource Management](#1-nfc-resource-management)
2. [Multi-Sector MIFARE Classic Operations](#2-multi-sector-mifare-classic-operations)
3. [Writing to MIFARE Classic Tags](#3-writing-to-mifare-classic-tags)
4. [Tag Type Detection](#4-tag-type-detection)
5. [Scene and View Management](#5-scene-and-view-management)
6. [Widget Input Handling](#6-widget-input-handling)
7. [Code Organization](#7-code-organization)
8. [Debugging Tips](#8-debugging-tips)

---

## 1. NFC Resource Management

The Flipper Zero NFC subsystem uses scanner and poller objects that **must be properly managed** to avoid crashes.

### The Problem
NFC scanner and poller objects hold hardware resources. If you don't free them properly, or if you try to allocate multiple instances, the Flipper will crash.

### Rules

1. **Always free resources in `on_exit` handlers**
2. **Set pointers to NULL after freeing**
3. **Check for NULL before allocating**
4. **Set pointers to NULL in `on_enter` before allocating**

### ❌ DON'T

```c
void scene_scan_on_enter(void* context) {
    App* app = context;
    // BAD: Not resetting state - could have stale pointer
    app->scanner = nfc_scanner_alloc(app->nfc);
    nfc_scanner_start(app->scanner, callback, app);
}

void scene_scan_on_exit(void* context) {
    App* app = context;
    // BAD: Not cleaning up resources
    widget_reset(app->widget);
}
```

### ✅ DO

```c
void scene_scan_on_enter(void* context) {
    App* app = context;

    // GOOD: Reset all state first
    app->card_detected = false;
    app->scanner = NULL;
    app->poller = NULL;

    // Now safe to allocate
    app->scanner = nfc_scanner_alloc(app->nfc);
    nfc_scanner_start(app->scanner, callback, app);
}

void scene_scan_on_exit(void* context) {
    App* app = context;
    widget_reset(app->widget);

    // GOOD: Clean up all NFC resources
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
```

### In Event Handlers

```c
bool scene_scan_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type == SceneManagerEventTypeTick) {
        // GOOD: Check poller is NULL before allocating new one
        if(app->card_detected && !app->uid_read && app->poller == NULL) {
            // Safe to stop scanner and start poller
            if(app->scanner) {
                nfc_scanner_stop(app->scanner);
                nfc_scanner_free(app->scanner);
                app->scanner = NULL;
            }
            app->poller = nfc_poller_alloc(app->nfc, NfcProtocolIso14443_3a);
            nfc_poller_start(app->poller, callback, app);
        }
    }
    return false;
}
```

---

## 2. Multi-Sector MIFARE Classic Operations

MIFARE Classic cards have sectors, each with its own authentication. **You cannot authenticate to multiple sectors in a single poller session.**

### The Problem
After authenticating to sector 0 and performing operations, attempting to authenticate to sector 1 in the same session will fail with auth errors (error code 2 or 5).

### ❌ DON'T

```c
// BAD: Trying to access multiple sectors in one callback
NfcCommand write_callback(NfcGenericEvent event, void* context) {
    // Auth and write sector 0
    mf_classic_poller_auth(poller, 0, &key, MfClassicKeyTypeA, &auth_ctx, false);
    mf_classic_poller_write_block(poller, 1, &block_data);
    mf_classic_poller_write_block(poller, 2, &block_data);

    // BAD: This will fail! Can't re-auth in same session
    mf_classic_poller_auth(poller, 4, &key, MfClassicKeyTypeA, &auth_ctx, false);
    mf_classic_poller_write_block(poller, 4, &block_data);

    return NfcCommandStop;
}
```

### ✅ DO

Use multiple poller passes, one per sector:

```c
// Global to track current sector
uint8_t g_current_write_sector = 0;

// Callback only handles ONE sector per invocation
NfcCommand write_callback(NfcGenericEvent event, void* context) {
    App* app = context;

    if(mf_event->type == MfClassicPollerEventTypeCardDetected) {
        uint8_t sector = g_current_write_sector;
        uint8_t first_block = sector * 4;

        // Auth to current sector only
        mf_classic_poller_auth(poller, first_block, &key, MfClassicKeyTypeA, &auth_ctx, false);

        if(sector == 0) {
            mf_classic_poller_write_block(poller, 1, &block_data);
            mf_classic_poller_write_block(poller, 2, &block_data);
        } else {
            mf_classic_poller_write_block(poller, 4, &block_data);
            mf_classic_poller_write_block(poller, 5, &block_data);
        }

        app->write_in_progress = false;
        return NfcCommandStop;  // Stop this pass
    }
    return NfcCommandContinue;
}

// Scene handler manages multiple passes
bool scene_write_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type == SceneManagerEventTypeTick) {
        if(!app->write_in_progress && app->poller == NULL) {
            if(g_current_write_sector == 0) {
                // First pass done, start second pass
                g_current_write_sector = 1;
                app->write_in_progress = true;
                app->poller = nfc_poller_alloc(app->nfc, NfcProtocolMfClassic);
                nfc_poller_start(app->poller, write_callback, app);
            } else {
                // Both sectors done
                app->write_success = true;
                scene_manager_next_scene(app->scene_manager, SceneResult);
            }
        }

        // Clean up finished poller
        if(!app->write_in_progress && app->poller != NULL) {
            nfc_poller_stop(app->poller);
            nfc_poller_free(app->poller);
            app->poller = NULL;
        }
    }
    return false;
}
```

---

## 3. Writing to MIFARE Classic Tags

When writing to MIFARE Classic tags, you must consider **both data blocks AND sector trailers**.

### The Problem
If you only write data blocks but not the sector trailer (which contains keys), the tag will still have its old keys. This means:
- Writing with default key `FFFFFFFFFFFF` works
- But reading later with derived keys fails (because keys weren't updated)

### Sector Trailer Format (Block 3, 7, 11, etc.)
```
Bytes 0-5:   Key A
Bytes 6-8:   Access bits
Byte 9:      User byte
Bytes 10-15: Key B
```

### ❌ DON'T

```c
// BAD: Only writing data blocks
mf_classic_poller_write_block(poller, 1, &data_block);
mf_classic_poller_write_block(poller, 2, &data_block);
// Tag still has old keys! Can't read back with new keys.
```

### ✅ DO

```c
// Write data blocks
mf_classic_poller_write_block(poller, 1, &data_block);
mf_classic_poller_write_block(poller, 2, &data_block);

// GOOD: Also write sector trailer with new keys
MfClassicBlock trailer;
memset(trailer.data, 0, 16);
memcpy(trailer.data, new_key, 6);           // Key A (bytes 0-5)
trailer.data[6] = 0xFF;                      // Access bits
trailer.data[7] = 0x07;
trailer.data[8] = 0x80;
trailer.data[9] = 0x69;                      // User byte
memcpy(&trailer.data[10], new_key, 6);      // Key B (bytes 10-15)

mf_classic_poller_write_block(poller, 3, &trailer);  // Sector 0 trailer
```

### Access Bits

- `FF 07 80` = Default, all blocks readable/writable with Key A or B
- Different values = Restrictive (read-only, key-specific access, etc.)

Original Bambu tags have restrictive access bits. Our programmed tags use default access bits.

---

## 4. Tag Type Detection

You cannot determine tag type just by checking if a key works. You must also check access bits.

### The Problem
After we program a blank tag with Bambu-derived keys, subsequent detection would see "Bambu key works" and incorrectly identify it as an original (read-only) Bambu tag.

### ❌ DON'T

```c
// BAD: Only checking if key works
if(mf_classic_poller_auth(poller, 0, &bambu_key, ...) == MfClassicErrorNone) {
    // This could be OUR programmed tag!
    app->detected_tag_type = TagTypeBambu;  // Wrong!
}
```

### ✅ DO

```c
if(mf_classic_poller_auth(poller, 0, &bambu_key, ...) == MfClassicErrorNone) {
    // Key works - but is it original Bambu or our programmed tag?
    // Read sector trailer and check access bits
    MfClassicBlock trailer;
    mf_classic_poller_read_block(poller, 3, &trailer);

    // Check if access bits are writable (our tags) or restrictive (original Bambu)
    bool is_writable = (trailer.data[6] == 0xFF &&
                        trailer.data[7] == 0x07 &&
                        trailer.data[8] == 0x80);

    if(is_writable) {
        // Our programmed tag - can reprogram
        app->detected_tag_type = TagTypeBlank;
        app->write_to_blank = false;  // Use Bambu keys
    } else {
        // Original Bambu tag - read-only
        app->detected_tag_type = TagTypeBambu;
    }
} else {
    // Bambu key failed - truly blank tag
    app->detected_tag_type = TagTypeBlank;
    app->write_to_blank = true;  // Use default keys
}
```

---

## 5. Scene and View Management

Flipper Zero uses a scene manager pattern. Understanding the lifecycle is crucial.

### Scene Lifecycle
1. `on_enter` - Called when entering scene (allocate resources, reset state)
2. `on_event` - Called for events (tick, custom, back button)
3. `on_exit` - Called when leaving scene (free resources)

### Rules

1. **Reset ALL state in `on_enter`** - Don't assume clean state
2. **Handle `SceneManagerEventTypeBack`** - Clean up before allowing back navigation
3. **Clean up in `on_exit`** - This is your last chance before scene dies

### ❌ DON'T

```c
void scene_on_enter(void* context) {
    App* app = context;
    // BAD: Not resetting state - previous run's state could cause issues
    app->poller = nfc_poller_alloc(app->nfc, NfcProtocolMfClassic);
}
```

### ✅ DO

```c
void scene_on_enter(void* context) {
    App* app = context;

    // GOOD: Reset all relevant state
    app->success = false;
    app->in_progress = false;
    app->poller = NULL;  // Explicit NULL before alloc

    // Now allocate
    app->poller = nfc_poller_alloc(app->nfc, NfcProtocolMfClassic);
}
```

---

## 6. Widget Input Handling

The Widget view supports text scrolling and button elements. **Don't override the input callback if you need scrolling.**

### The Problem
Setting `view_set_input_callback` on a widget overrides ALL input handling, including the up/down scrolling for `widget_add_text_scroll_element`.

### ❌ DON'T

```c
void scene_on_enter(void* context) {
    App* app = context;

    widget_add_text_scroll_element(app->widget, 0, 0, 128, 52, long_text);
    widget_add_button_element(app->widget, GuiButtonTypeRight, "OK", button_cb, app);

    // BAD: This breaks scrolling!
    view_set_input_callback(widget_get_view(app->widget), my_input_callback);
}
```

### ✅ DO

```c
void scene_on_enter(void* context) {
    App* app = context;

    // Text scroll with room for button (height 52 instead of 64)
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 52, long_text);

    // GOOD: Use button element callback - doesn't interfere with scrolling
    widget_add_button_element(app->widget, GuiButtonTypeRight, "OK", button_cb, app);

    // DON'T set view_set_input_callback - let widget handle scrolling
}

// Button callback handles the button press
static void button_cb(GuiButtonType result, InputType type, void* context) {
    App* app = context;
    if(type == InputTypeShort && result == GuiButtonTypeRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, EventConfirmed);
    }
}
```

---

## 7. Code Organization

Large monolithic files become unmanageable. Split into logical modules.

### Recommended Structure

```
├── bambu_tagger.c      # Main entry point, app lifecycle (~150 lines)
├── bambu_tagger.h      # Shared types, structs, enums
├── scenes.c/h          # All scene handlers
├── nfc_operations.c/h  # NFC callbacks (scanner, poller)
├── tag_storage.c/h     # File save/load operations
├── bambu_crypto.c/h    # Crypto/key derivation
└── bambu_tag_data.h    # Data structures and helpers
```

### Header File Pattern

```c
// bambu_tagger.h - Shared across all modules
#pragma once

#include <furi.h>
// ... other includes

// Forward declarations
typedef struct App App;

// Enums
typedef enum { SceneMain, SceneRead, SceneWrite, SceneCount } AppScene;

// Main app struct
struct App {
    Gui* gui;
    Nfc* nfc;
    // ...
};
```

### Module Pattern

```c
// nfc_operations.h
#pragma once
#include "bambu_tagger.h"

extern uint8_t g_current_read_sector;  // Global for multi-pass tracking

void scanner_callback(NfcScannerEvent event, void* context);
NfcCommand read_poller_callback(NfcGenericEvent event, void* context);
```

```c
// nfc_operations.c
#include "nfc_operations.h"

uint8_t g_current_read_sector = 0;

void scanner_callback(NfcScannerEvent event, void* context) {
    // Implementation
}
```

---

## 8. Debugging Tips

### Enable Logging

```c
#define TAG "MyApp"

FURI_LOG_I(TAG, "Info: value=%d", value);
FURI_LOG_D(TAG, "Debug: state=%d", state);
FURI_LOG_W(TAG, "Warning: unexpected condition");
FURI_LOG_E(TAG, "Error: operation failed with code %d", err);
```

### View Logs

```bash
# Connect to Flipper CLI
ufbt cli

# Or with uv
uv run ufbt cli
```

Logs appear in real-time as you use the app.

### Common Error Codes (MfClassicError)

- `0` = Success
- `1` = Not present
- `2` = Auth failed (wrong key)
- `3` = Partial read
- `4` = Protocol error
- `5` = Timeout

### Debug State Machines

Log state transitions:

```c
bool scene_on_event(void* context, SceneManagerEvent event) {
    App* app = context;

    if(event.type == SceneManagerEventTypeTick) {
        FURI_LOG_D(TAG, "Tick: detected=%d, read=%d, progress=%d, poller=%p",
            app->card_detected,
            app->uid_read,
            app->in_progress,
            app->poller);
    }
    // ...
}
```

---

## Summary Checklist

Before submitting code, verify:

- [ ] All `on_exit` handlers free scanner/poller resources
- [ ] All `on_enter` handlers reset state and set pointers to NULL
- [ ] All `on_event` handlers check `poller == NULL` before allocating
- [ ] Multi-sector operations use multiple poller passes
- [ ] Sector trailers are written when updating keys
- [ ] Tag detection checks access bits, not just key validity
- [ ] Widget scrolling isn't broken by input callbacks
- [ ] Error codes are logged for debugging
- [ ] No unused variables (compile with `-Werror`)

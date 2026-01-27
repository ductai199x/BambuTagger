/**
 * @file nfc_operations.c
 * @brief NFC scanner and poller callbacks for tag read/write
 * @author Tai Nguyen <taiducnguyen.drexel@gmail.com>
 */

#include "nfc_operations.h"

// Track which sector to read/write (0 or 1) - managed by scene handler
uint8_t g_current_read_sector = 0;
uint8_t g_current_write_sector = 0;

void scanner_callback(NfcScannerEvent event, void* context) {
    App* app = context;
    if(event.type == NfcScannerEventTypeDetected) {
        app->card_detected = true;
    }
}

NfcCommand uid_poller_callback(NfcGenericEvent event, void* context) {
    App* app = context;

    if(event.protocol == NfcProtocolIso14443_3a) {
        const Iso14443_3aPollerEvent* iso_event = event.event_data;
        FURI_LOG_D(TAG, "ISO14443-3A event type: %d", iso_event->type);

        if(iso_event->type == Iso14443_3aPollerEventTypeReady) {
            // Get the data from the poller instance
            const Iso14443_3aData* data = nfc_poller_get_data(app->poller);
            if(data) {
                app->tag_data.uid_len = data->uid_len;
                memcpy(app->tag_data.uid, data->uid, app->tag_data.uid_len);
                app->uid_read = true;
                FURI_LOG_I(TAG, "UID read successfully, len=%d", data->uid_len);
            }
            return NfcCommandStop;
        }
    }
    return NfcCommandContinue;
}

NfcCommand detect_tag_type_callback(NfcGenericEvent event, void* context) {
    App* app = context;

    if(event.protocol == NfcProtocolMfClassic) {
        const MfClassicPollerEvent* mf_event = event.event_data;
        MfClassicPoller* poller = event.instance;

        if(mf_event->type == MfClassicPollerEventTypeRequestMode) {
            MfClassicPollerEventDataRequestMode* mode_data = &mf_event->data->poller_mode;
            mf_classic_reset(app->mf_data);
            app->mf_data->type = MfClassicType1k;
            mode_data->mode = MfClassicPollerModeRead;
            mode_data->data = app->mf_data;
            return NfcCommandContinue;
        }

        if(mf_event->type == MfClassicPollerEventTypeCardDetected) {
            // Try to authenticate sector 0 with Bambu-derived key
            MfClassicKey key;
            MfClassicAuthContext auth_ctx;
            memcpy(key.data, app->derived_keys.keys[0], MF_CLASSIC_KEY_SIZE);

            MfClassicError err = mf_classic_poller_auth(poller, 0, &key, MfClassicKeyTypeA, &auth_ctx, false);

            if(err == MfClassicErrorNone) {
                // Bambu keys worked - now check if access bits are read-only
                FURI_LOG_I(TAG, "Detection: Bambu key auth succeeded, checking access bits...");

                // Read sector trailer (block 3) to check access bits
                MfClassicBlock trailer;
                err = mf_classic_poller_read_block(poller, 3, &trailer);

                if(err == MfClassicErrorNone) {
                    // Access bits are in bytes 6-8 of the trailer
                    // Default (writable): FF 07 80
                    // We check if it matches our writable pattern
                    bool is_writable = (trailer.data[6] == 0xFF &&
                                       trailer.data[7] == 0x07 &&
                                       trailer.data[8] == 0x80);

                    FURI_LOG_I(TAG, "Access bits: %02X %02X %02X - %s",
                        trailer.data[6], trailer.data[7], trailer.data[8],
                        is_writable ? "writable" : "read-only");

                    if(is_writable) {
                        // Tag we programmed - can be reprogrammed
                        app->detected_tag_type = TagTypeBlank;
                        app->write_to_blank = false;  // Use Bambu keys for auth
                    } else {
                        // Original Bambu tag with restrictive access bits
                        app->detected_tag_type = TagTypeBambu;
                    }
                } else {
                    // Couldn't read trailer - assume original Bambu tag
                    FURI_LOG_W(TAG, "Couldn't read sector trailer, assuming Bambu tag");
                    app->detected_tag_type = TagTypeBambu;
                }
            } else {
                // Bambu keys failed - assume blank tag with default keys
                FURI_LOG_I(TAG, "Detection: Bambu key auth failed - assuming blank tag");
                app->detected_tag_type = TagTypeBlank;
                app->write_to_blank = true;  // Use default keys for auth
            }
            app->detection_in_progress = false;
            return NfcCommandStop;
        }
    }
    return NfcCommandContinue;
}

NfcCommand write_poller_callback(NfcGenericEvent event, void* context) {
    App* app = context;

    if(event.protocol == NfcProtocolMfClassic) {
        const MfClassicPollerEvent* mf_event = event.event_data;
        MfClassicPoller* poller = event.instance;

        if(mf_event->type == MfClassicPollerEventTypeRequestMode) {
            MfClassicPollerEventDataRequestMode* mode_data = &mf_event->data->poller_mode;
            mf_classic_reset(app->mf_data);
            app->mf_data->type = MfClassicType1k;
            mode_data->mode = MfClassicPollerModeRead;  // Use read mode, we'll write manually
            mode_data->data = app->mf_data;
            FURI_LOG_I(TAG, "Write: RequestMode, sector=%d, write_to_blank=%d",
                g_current_write_sector, app->write_to_blank);
            return NfcCommandContinue;
        }

        if(mf_event->type == MfClassicPollerEventTypeCardDetected) {
            FURI_LOG_I(TAG, "Write: Card detected, writing sector %d", g_current_write_sector);

            MfClassicKey auth_key;
            MfClassicAuthContext auth_ctx;
            MfClassicBlock block_data;
            MfClassicError err;

            uint8_t sector = g_current_write_sector;
            uint8_t first_block = sector * 4;  // Sector 0 -> block 0, Sector 1 -> block 4

            // Determine which key to use for authentication
            if(app->write_to_blank) {
                memset(auth_key.data, 0xFF, MF_CLASSIC_KEY_SIZE);
                FURI_LOG_I(TAG, "Using default key FFFFFFFFFFFF");
            } else {
                memcpy(auth_key.data, app->derived_keys.keys[sector], MF_CLASSIC_KEY_SIZE);
            }

            // Authenticate to sector
            FURI_LOG_I(TAG, "Authenticating sector %d (block %d)...", sector, first_block);
            err = mf_classic_poller_auth(poller, first_block, &auth_key, MfClassicKeyTypeA, &auth_ctx, false);

            if(err != MfClassicErrorNone) {
                FURI_LOG_E(TAG, "Sector %d auth failed: %d", sector, err);
                app->write_in_progress = false;
                return NfcCommandStop;
            }

            FURI_LOG_I(TAG, "Sector %d auth OK", sector);

            // Prepare and write blocks for this sector
            if(sector == 0) {
                // Write blocks 1 and 2
                if(app->use_saved_tag) {
                    memcpy(block_data.data, app->read_data.block1, 16);
                } else {
                    const FilamentInfo* filament = &BAMBU_FILAMENTS[app->tag_data.filament_index];
                    prepare_block1(block_data.data, filament);
                }
                FURI_LOG_I(TAG, "Writing block 1...");
                err = mf_classic_poller_write_block(poller, 1, &block_data);
                if(err != MfClassicErrorNone) {
                    FURI_LOG_E(TAG, "Block 1 write failed: %d", err);
                    app->write_in_progress = false;
                    return NfcCommandStop;
                }
                FURI_LOG_I(TAG, "Block 1 write OK");

                if(app->use_saved_tag) {
                    memcpy(block_data.data, app->read_data.block2, 16);
                } else {
                    const FilamentInfo* filament = &BAMBU_FILAMENTS[app->tag_data.filament_index];
                    prepare_block2(block_data.data, filament);
                }
                FURI_LOG_I(TAG, "Writing block 2...");
                err = mf_classic_poller_write_block(poller, 2, &block_data);
                if(err != MfClassicErrorNone) {
                    FURI_LOG_E(TAG, "Block 2 write failed: %d", err);
                    app->write_in_progress = false;
                    return NfcCommandStop;
                }
                FURI_LOG_I(TAG, "Block 2 write OK");

                // Write sector 0 trailer (block 3) with Bambu-derived keys
                memset(block_data.data, 0, 16);
                memcpy(block_data.data, app->derived_keys.keys[0], 6);  // Key A
                block_data.data[6] = 0xFF;  // Access bits
                block_data.data[7] = 0x07;
                block_data.data[8] = 0x80;
                block_data.data[9] = 0x69;
                memcpy(&block_data.data[10], app->derived_keys.keys[0], 6);  // Key B
                FURI_LOG_I(TAG, "Writing sector 0 trailer (block 3)...");
                err = mf_classic_poller_write_block(poller, 3, &block_data);
                if(err != MfClassicErrorNone) {
                    FURI_LOG_E(TAG, "Block 3 write failed: %d", err);
                    app->write_in_progress = false;
                    return NfcCommandStop;
                }
                FURI_LOG_I(TAG, "Block 3 (sector trailer) write OK");
            } else {
                // Write blocks 4 and 5
                if(app->use_saved_tag) {
                    memcpy(block_data.data, app->read_data.block4, 16);
                } else {
                    const FilamentInfo* filament = &BAMBU_FILAMENTS[app->tag_data.filament_index];
                    prepare_block4(block_data.data, filament);
                }
                FURI_LOG_I(TAG, "Writing block 4...");
                err = mf_classic_poller_write_block(poller, 4, &block_data);
                if(err != MfClassicErrorNone) {
                    FURI_LOG_E(TAG, "Block 4 write failed: %d", err);
                    app->write_in_progress = false;
                    return NfcCommandStop;
                }
                FURI_LOG_I(TAG, "Block 4 write OK");

                if(app->use_saved_tag) {
                    memcpy(block_data.data, app->read_data.block5, 16);
                } else {
                    const ColorPreset* color = &COLOR_PRESETS[app->tag_data.color_index];
                    prepare_block5(block_data.data, color, app->tag_data.weight_grams);
                }
                FURI_LOG_I(TAG, "Writing block 5...");
                err = mf_classic_poller_write_block(poller, 5, &block_data);
                if(err != MfClassicErrorNone) {
                    FURI_LOG_E(TAG, "Block 5 write failed: %d", err);
                    app->write_in_progress = false;
                    return NfcCommandStop;
                }
                FURI_LOG_I(TAG, "Block 5 write OK");

                // Write block 6 (manufacturer)
                if(app->use_saved_tag) {
                    memcpy(block_data.data, app->read_data.block6, 16);
                } else {
                    const ManufacturerPreset* manufacturer = &MANUFACTURER_PRESETS[app->tag_data.manufacturer_index];
                    prepare_block6(block_data.data, manufacturer);
                }
                FURI_LOG_I(TAG, "Writing block 6...");
                err = mf_classic_poller_write_block(poller, 6, &block_data);
                if(err != MfClassicErrorNone) {
                    FURI_LOG_E(TAG, "Block 6 write failed: %d", err);
                    app->write_in_progress = false;
                    return NfcCommandStop;
                }
                FURI_LOG_I(TAG, "Block 6 write OK");

                // Write sector 1 trailer (block 7) with Bambu-derived keys
                memset(block_data.data, 0, 16);
                memcpy(block_data.data, app->derived_keys.keys[1], 6);  // Key A
                block_data.data[6] = 0xFF;  // Access bits
                block_data.data[7] = 0x07;
                block_data.data[8] = 0x80;
                block_data.data[9] = 0x69;
                memcpy(&block_data.data[10], app->derived_keys.keys[1], 6);  // Key B
                FURI_LOG_I(TAG, "Writing sector 1 trailer (block 7)...");
                err = mf_classic_poller_write_block(poller, 7, &block_data);
                if(err != MfClassicErrorNone) {
                    FURI_LOG_E(TAG, "Block 7 write failed: %d", err);
                    app->write_in_progress = false;
                    return NfcCommandStop;
                }
                FURI_LOG_I(TAG, "Block 7 (sector trailer) write OK");
            }

            FURI_LOG_I(TAG, "Sector %d write complete", sector);
            app->write_in_progress = false;
            return NfcCommandStop;
        }

        if(mf_event->type == MfClassicPollerEventTypeCardLost) {
            FURI_LOG_W(TAG, "Card lost during write");
            app->write_in_progress = false;
            return NfcCommandStop;
        }
    }
    return NfcCommandContinue;
}

NfcCommand read_poller_callback(NfcGenericEvent event, void* context) {
    App* app = context;

    if(event.protocol == NfcProtocolMfClassic) {
        const MfClassicPollerEvent* mf_event = event.event_data;
        MfClassicPoller* poller = event.instance;

        if(mf_event->type == MfClassicPollerEventTypeRequestMode) {
            MfClassicPollerEventDataRequestMode* mode_data = &mf_event->data->poller_mode;

            mf_classic_reset(app->mf_data);
            app->mf_data->type = MfClassicType1k;
            mf_classic_set_uid(app->mf_data, app->tag_data.uid, app->tag_data.uid_len);

            mode_data->mode = MfClassicPollerModeRead;
            mode_data->data = app->mf_data;

            FURI_LOG_I(TAG, "RequestMode: will read sector %d", g_current_read_sector);
            return NfcCommandContinue;
        }

        if(mf_event->type == MfClassicPollerEventTypeCardDetected) {
            MfClassicKey key;
            MfClassicAuthContext auth_ctx;
            MfClassicBlock block_data;
            MfClassicError err;

            uint8_t sector = g_current_read_sector;
            uint8_t first_block = sector * 4;  // Sector 0 -> block 0, Sector 1 -> block 4

            memcpy(key.data, app->derived_keys.keys[sector], MF_CLASSIC_KEY_SIZE);
            FURI_LOG_I(TAG, "Reading sector %d (block %d)...", sector, first_block);

            err = mf_classic_poller_auth(poller, first_block, &key, MfClassicKeyTypeA, &auth_ctx, false);

            if(err == MfClassicErrorNone) {
                FURI_LOG_I(TAG, "Sector %d Auth OK", sector);

                if(sector == 0) {
                    // Read blocks 1 and 2
                    if(mf_classic_poller_read_block(poller, 1, &block_data) == MfClassicErrorNone) {
                        memcpy(app->read_data.block1, block_data.data, 16);
                        FURI_LOG_I(TAG, "Block 1: %02X %02X %02X %02X...",
                            block_data.data[0], block_data.data[1],
                            block_data.data[2], block_data.data[3]);
                    }
                    if(mf_classic_poller_read_block(poller, 2, &block_data) == MfClassicErrorNone) {
                        memcpy(app->read_data.block2, block_data.data, 16);
                        FURI_LOG_I(TAG, "Block 2: %02X %02X %02X %02X...",
                            block_data.data[0], block_data.data[1],
                            block_data.data[2], block_data.data[3]);
                    }
                } else {
                    // Read blocks 4, 5, and 6
                    if(mf_classic_poller_read_block(poller, 4, &block_data) == MfClassicErrorNone) {
                        memcpy(app->read_data.block4, block_data.data, 16);
                        FURI_LOG_I(TAG, "Block 4: %02X %02X %02X %02X...",
                            block_data.data[0], block_data.data[1],
                            block_data.data[2], block_data.data[3]);
                    }
                    if(mf_classic_poller_read_block(poller, 5, &block_data) == MfClassicErrorNone) {
                        memcpy(app->read_data.block5, block_data.data, 16);
                        FURI_LOG_I(TAG, "Block 5: %02X %02X %02X %02X...",
                            block_data.data[0], block_data.data[1],
                            block_data.data[2], block_data.data[3]);
                    }
                    // Read block 6 (manufacturer) - gracefully handle failure
                    // block6 is already zeroed by memset in scene on_enter
                    if(mf_classic_poller_read_block(poller, 6, &block_data) == MfClassicErrorNone) {
                        memcpy(app->read_data.block6, block_data.data, 16);
                        FURI_LOG_I(TAG, "Block 6: %02X %02X %02X %02X...",
                            block_data.data[0], block_data.data[1],
                            block_data.data[2], block_data.data[3]);
                    } else {
                        // Block 6 read failed - continue with zeroed data (shows "Generic")
                        FURI_LOG_I(TAG, "Block 6 read failed - defaulting to Generic");
                    }
                }
            } else {
                FURI_LOG_E(TAG, "Sector %d Auth Failed: %d", sector, err);
            }

            // Signal that this pass is done
            app->read_in_progress = false;
            return NfcCommandStop;
        }

        if(mf_event->type == MfClassicPollerEventTypeCardLost) {
            FURI_LOG_W(TAG, "Card lost");
            app->read_in_progress = false;
            return NfcCommandStop;
        }
    }
    return NfcCommandContinue;
}

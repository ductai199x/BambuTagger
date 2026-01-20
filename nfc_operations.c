/**
 * @file nfc_operations.c
 * @brief NFC scanner and poller callbacks for tag read/write
 * @author Tai Nguyen <taiducnguyen.drexel@gmail.com>
 */

#include "nfc_operations.h"

// Track which sector to read (0 or 1) - managed by scene handler
uint8_t g_current_read_sector = 0;

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
                // Bambu keys worked - it's a Bambu tag
                FURI_LOG_I(TAG, "Detection: Bambu key auth succeeded - Bambu tag");
                app->detected_tag_type = TagTypeBambu;
            } else {
                // Bambu keys failed - assume blank tag
                FURI_LOG_I(TAG, "Detection: Bambu key auth failed - assuming blank tag");
                app->detected_tag_type = TagTypeBlank;
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

        if(mf_event->type == MfClassicPollerEventTypeRequestMode) {
            MfClassicPollerEventDataRequestMode* mode_data = &mf_event->data->poller_mode;

            // Reset the data structure and set card type
            mf_classic_reset(app->mf_data);
            app->mf_data->type = MfClassicType1k;
            mf_classic_set_uid(app->mf_data, app->tag_data.uid, app->tag_data.uid_len);

            // Set sector keys for authentication
            if(app->write_to_blank) {
                // Blank tags use default key FFFFFFFFFFFF
                uint64_t default_key = 0xFFFFFFFFFFFF;
                for(int sector = 0; sector < BAMBU_NUM_SECTORS; sector++) {
                    mf_classic_set_key_found(app->mf_data, sector, MfClassicKeyTypeA, default_key);
                    mf_classic_set_key_found(app->mf_data, sector, MfClassicKeyTypeB, default_key);
                }
            } else {
                // Use Bambu-derived keys
                for(int sector = 0; sector < BAMBU_NUM_SECTORS; sector++) {
                    uint64_t key = key_bytes_to_uint64(app->derived_keys.keys[sector]);
                    mf_classic_set_key_found(app->mf_data, sector, MfClassicKeyTypeA, key);
                    mf_classic_set_key_found(app->mf_data, sector, MfClassicKeyTypeB, key);
                }
            }

            // Prepare block data
            MfClassicBlock block;

            if(app->use_saved_tag) {
                // Use loaded data from saved tag
                memcpy(block.data, app->read_data.block1, 16);
                mf_classic_set_block_read(app->mf_data, 1, &block);

                memcpy(block.data, app->read_data.block2, 16);
                mf_classic_set_block_read(app->mf_data, 2, &block);

                memcpy(block.data, app->read_data.block4, 16);
                mf_classic_set_block_read(app->mf_data, 4, &block);

                memcpy(block.data, app->read_data.block5, 16);
                mf_classic_set_block_read(app->mf_data, 5, &block);
            } else {
                // Use freshly configured data
                const FilamentInfo* filament = &BAMBU_FILAMENTS[app->tag_data.filament_index];
                const ColorPreset* color = &COLOR_PRESETS[app->tag_data.color_index];

                // Block 1: Material variant + Material ID
                prepare_block1(block.data, filament);
                mf_classic_set_block_read(app->mf_data, 1, &block);

                // Block 2: Filament type
                prepare_block2(block.data, filament);
                mf_classic_set_block_read(app->mf_data, 2, &block);

                // Block 4: Detailed filament type
                prepare_block4(block.data, filament);
                mf_classic_set_block_read(app->mf_data, 4, &block);

                // Block 5: Color + Weight
                prepare_block5(block.data, color, app->tag_data.weight_grams);
                mf_classic_set_block_read(app->mf_data, 5, &block);
            }

            mode_data->mode = MfClassicPollerModeWrite;
            mode_data->data = app->mf_data;

            return NfcCommandContinue;
        }

        // Handle sector trailer request for writing
        if(mf_event->type == MfClassicPollerEventTypeRequestSectorTrailer) {
            MfClassicPollerEventDataSectorTrailerRequest* sec_tr =
                &mf_event->data->sec_tr_data;
            uint8_t sector = sec_tr->sector_num;

            FURI_LOG_I(TAG, "Write: Sector trailer request for sector %d", sector);

            // Only handle sectors 0 and 1 (where Bambu data lives)
            if(sector < 2) {
                // Set up sector trailer with our key
                memset(sec_tr->sector_trailer.data, 0, 16);
                // Key A (bytes 0-5)
                memcpy(sec_tr->sector_trailer.data, app->derived_keys.keys[sector], 6);
                // Access bits (bytes 6-9) - use default
                sec_tr->sector_trailer.data[6] = 0xFF;
                sec_tr->sector_trailer.data[7] = 0x07;
                sec_tr->sector_trailer.data[8] = 0x80;
                sec_tr->sector_trailer.data[9] = 0x69;
                // Key B (bytes 10-15)
                memcpy(&sec_tr->sector_trailer.data[10], app->derived_keys.keys[sector], 6);
                sec_tr->sector_trailer_provided = true;
            } else {
                sec_tr->sector_trailer_provided = false;
            }

            return NfcCommandContinue;
        }

        // Handle write block request
        if(mf_event->type == MfClassicPollerEventTypeRequestWriteBlock) {
            MfClassicPollerEventDataWriteBlockRequest* write_req =
                &mf_event->data->write_block_data;
            uint8_t block = write_req->block_num;

            FURI_LOG_I(TAG, "Write: Block write request for block %d", block);

            // Only provide data for blocks we actually want to write
            if(block == 1 || block == 2 || block == 4 || block == 5) {
                memcpy(
                    write_req->write_block.data, app->mf_data->block[block].data, MF_CLASSIC_BLOCK_SIZE);
                write_req->write_block_provided = true;
            } else {
                write_req->write_block_provided = false;
            }

            return NfcCommandContinue;
        }

        if(mf_event->type == MfClassicPollerEventTypeSuccess) {
            app->write_success = true;
            app->write_in_progress = false;
            return NfcCommandStop;
        }

        if(mf_event->type == MfClassicPollerEventTypeFail) {
            app->write_success = false;
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
                    // Read blocks 4 and 5
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

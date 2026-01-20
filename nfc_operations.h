/**
 * @file nfc_operations.h
 * @brief NFC scanner and poller callback declarations
 * @author Tai Nguyen <taiducnguyen.drexel@gmail.com>
 */

#pragma once

#include "bambu_tagger.h"

// Global variable for multi-pass read tracking
extern uint8_t g_current_read_sector;

// NFC scanner callback
void scanner_callback(NfcScannerEvent event, void* context);

// UID poller callback (ISO14443-3A)
NfcCommand uid_poller_callback(NfcGenericEvent event, void* context);

// Tag type detection callback
NfcCommand detect_tag_type_callback(NfcGenericEvent event, void* context);

// Write poller callback
NfcCommand write_poller_callback(NfcGenericEvent event, void* context);

// Read poller callback
NfcCommand read_poller_callback(NfcGenericEvent event, void* context);

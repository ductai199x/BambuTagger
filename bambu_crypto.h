/**
 * @file bambu_crypto.h
 * @brief Bambu Lab RFID key derivation declarations
 * @author Tai Nguyen <taiducnguyen.drexel@gmail.com>
 */

#pragma once
#include <stdint.h>
#include <stddef.h>

// Bambu Lab uses Mifare Classic 1K with 16 sectors
#define BAMBU_NUM_SECTORS 16
#define BAMBU_KEY_LENGTH 6

// Structure to hold all derived keys
typedef struct {
    uint8_t keys[BAMBU_NUM_SECTORS][BAMBU_KEY_LENGTH];
} BambuKeys;

// Calculate all 16 sector keys from tag UID
void calculate_all_keys(const uint8_t* uid, size_t uid_len, BambuKeys* keys_out);

// Helper to get key as uint64_t for Flipper's MfClassic API
uint64_t key_bytes_to_uint64(const uint8_t* key);

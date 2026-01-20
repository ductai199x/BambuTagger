/**
 * @file bambu_crypto.c
 * @brief Bambu Lab RFID key derivation algorithm
 * @author Tai Nguyen <taiducnguyen.drexel@gmail.com>
 */

#include "bambu_crypto.h"
#include <mbedtls/md.h>
#include <string.h>

// Master Key from SpoolEase (reverse-engineered from Bambu Lab)
static const uint8_t BAMBU_MASTER_KEY[] = {
    0x9a, 0x75, 0x9c, 0xf2, 0xc4, 0xf7, 0xca, 0xff,
    0x22, 0x2c, 0xb9, 0x76, 0x9b, 0x41, 0xbc, 0x96
};

// Context string for HKDF-Expand (includes null terminator)
static const uint8_t BAMBU_CONTEXT[] = "RFID-A";
#define BAMBU_CONTEXT_LEN 7  // "RFID-A" + \0

// SHA256 output length
#define SHA256_LEN 32

void calculate_all_keys(const uint8_t* uid, size_t uid_len, BambuKeys* keys_out) {
    mbedtls_md_context_t ctx;
    uint8_t prk[SHA256_LEN];  // Pseudo-random key from extract phase

    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);

    // ============================================
    // HKDF-Extract: PRK = HMAC-SHA256(master_key, uid)
    // ============================================
    mbedtls_md_hmac_starts(&ctx, BAMBU_MASTER_KEY, sizeof(BAMBU_MASTER_KEY));
    mbedtls_md_hmac_update(&ctx, uid, uid_len);
    mbedtls_md_hmac_finish(&ctx, prk);

    // ============================================
    // HKDF-Expand: Generate 96 bytes (16 keys × 6 bytes)
    // ============================================
    // We need 96 bytes total. Each SHA256 block gives 32 bytes.
    // So we need ceil(96/32) = 3 blocks (gives us 96 bytes exactly usable).
    // Actually 3 blocks give 96 bytes, but we only need first 96 bytes.

    uint8_t okm[128];  // Output keying material (3 blocks = 96 bytes needed)
    uint8_t t_prev[SHA256_LEN];  // T(i-1)
    uint8_t t_curr[SHA256_LEN];  // T(i)
    size_t okm_offset = 0;

    // Number of HMAC blocks needed: ceil(96 / 32) = 3
    // But the algorithm uses counter starting at 1
    const int num_blocks = 3;

    for(int i = 1; i <= num_blocks; i++) {
        mbedtls_md_hmac_reset(&ctx);
        mbedtls_md_hmac_starts(&ctx, prk, SHA256_LEN);

        // T(i) = HMAC(PRK, T(i-1) || context || i)
        if(i > 1) {
            mbedtls_md_hmac_update(&ctx, t_prev, SHA256_LEN);
        }
        mbedtls_md_hmac_update(&ctx, BAMBU_CONTEXT, BAMBU_CONTEXT_LEN);

        uint8_t counter = (uint8_t)i;
        mbedtls_md_hmac_update(&ctx, &counter, 1);

        mbedtls_md_hmac_finish(&ctx, t_curr);

        // Copy to output buffer
        memcpy(&okm[okm_offset], t_curr, SHA256_LEN);
        okm_offset += SHA256_LEN;

        // Save for next iteration
        memcpy(t_prev, t_curr, SHA256_LEN);
    }

    // Extract 16 keys (6 bytes each) from OKM
    for(int sector = 0; sector < BAMBU_NUM_SECTORS; sector++) {
        memcpy(keys_out->keys[sector], &okm[sector * BAMBU_KEY_LENGTH], BAMBU_KEY_LENGTH);
    }

    mbedtls_md_free(&ctx);
}

uint64_t key_bytes_to_uint64(const uint8_t* key) {
    uint64_t result = 0;
    for(int i = 0; i < BAMBU_KEY_LENGTH; i++) {
        result = (result << 8) | key[i];
    }
    return result;
}

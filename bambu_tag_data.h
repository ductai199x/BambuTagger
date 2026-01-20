/**
 * @file bambu_tag_data.h
 * @brief Filament definitions and tag block data helpers
 * @author Tai Nguyen <taiducnguyen.drexel@gmail.com>
 */

#pragma once
#include <stdint.h>
#include <stddef.h>

// ============================================
// Bambu Lab Tag Block Layout
// ============================================
// Block 0: UID, BCC, manufacturer data (read-only)
// Block 1: Material variant (0-7) + Material ID (8-15)
// Block 2: Filament type string (e.g., "PLA")
// Block 3: Sector 0 trailer
// Block 4: Detailed filament type (e.g., "PLA Basic")
// Block 5: Color RGBA (0-3) + Weight (4-5) + Reserved
// Block 6: Additional data
// Block 7: Sector 1 trailer

// ============================================
// Material Types (base categories)
// ============================================
typedef enum {
    MATERIAL_PLA = 0,
    MATERIAL_PETG,
    MATERIAL_ABS,
    MATERIAL_ASA,
    MATERIAL_TPU,
    MATERIAL_PA,
    MATERIAL_PC,
    MATERIAL_PET_CF,
    MATERIAL_SUPPORT,
    MATERIAL_COUNT
} MaterialType;

// ============================================
// Filament definitions
// ============================================
typedef struct {
    const char* material_id;      // e.g., "GFA00"
    const char* display_name;     // e.g., "PLA Basic"
    const char* material_variant; // e.g., "A00-G1" (optional, can be empty)
    const char* filament_type;    // e.g., "PLA" for block 2
    MaterialType category;
} FilamentInfo;

// Common Bambu Lab filaments (subset for UI)
static const FilamentInfo BAMBU_FILAMENTS[] = {
    // PLA variants
    {"GFA00", "PLA Basic", "", "PLA", MATERIAL_PLA},
    {"GFA01", "PLA Matte", "", "PLA", MATERIAL_PLA},
    {"GFA02", "PLA Metal", "", "PLA", MATERIAL_PLA},
    {"GFA05", "PLA Silk", "", "PLA", MATERIAL_PLA},
    {"GFA08", "PLA Sparkle", "", "PLA", MATERIAL_PLA},
    {"GFA09", "PLA Tough", "", "PLA", MATERIAL_PLA},
    {"GFA50", "PLA-CF", "", "PLA-CF", MATERIAL_PLA},
    {"GFL99", "Generic PLA", "", "PLA", MATERIAL_PLA},

    // PETG variants
    {"GFG00", "PETG Basic", "", "PETG", MATERIAL_PETG},
    {"GFG01", "PETG Translucent", "", "PETG", MATERIAL_PETG},
    {"GFG02", "PETG HF", "", "PETG", MATERIAL_PETG},
    {"GFG50", "PETG-CF", "", "PETG-CF", MATERIAL_PETG},
    {"GFG99", "Generic PETG", "", "PETG", MATERIAL_PETG},

    // ABS variants
    {"GFB00", "ABS", "", "ABS", MATERIAL_ABS},
    {"GFB50", "ABS-GF", "", "ABS-GF", MATERIAL_ABS},
    {"GFB99", "Generic ABS", "", "ABS", MATERIAL_ABS},

    // ASA variants
    {"GFB01", "ASA", "", "ASA", MATERIAL_ASA},
    {"GFB02", "ASA-Aero", "", "ASA-AERO", MATERIAL_ASA},
    {"GFB98", "Generic ASA", "", "ASA", MATERIAL_ASA},

    // TPU variants
    {"GFU00", "TPU 95A HF", "", "TPU", MATERIAL_TPU},
    {"GFU01", "TPU 95A", "", "TPU", MATERIAL_TPU},
    {"GFU02", "TPU for AMS", "", "TPU-AMS", MATERIAL_TPU},
    {"GFU99", "Generic TPU", "", "TPU", MATERIAL_TPU},

    // PA (Nylon) variants
    {"GFN03", "PA-CF", "", "PA-CF", MATERIAL_PA},
    {"GFN05", "PA6-CF", "", "PA6-CF", MATERIAL_PA},
    {"GFN99", "Generic PA", "", "PA", MATERIAL_PA},

    // PC variants
    {"GFC00", "PC", "", "PC", MATERIAL_PC},
    {"GFC99", "Generic PC", "", "PC", MATERIAL_PC},

    // PET-CF
    {"GFT01", "PET-CF", "", "PET-CF", MATERIAL_PET_CF},

    // Support materials
    {"GFS00", "Support W", "", "PLA-S", MATERIAL_SUPPORT},
    {"GFS01", "Support G", "", "PA-S", MATERIAL_SUPPORT},
    {"GFS02", "Support PLA", "", "PLA-S", MATERIAL_SUPPORT},
    {"GFS04", "PVA", "", "PVA", MATERIAL_SUPPORT},
};

#define BAMBU_FILAMENT_COUNT (sizeof(BAMBU_FILAMENTS) / sizeof(BAMBU_FILAMENTS[0]))

// ============================================
// Color Presets
// ============================================
typedef struct {
    const char* name;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;  // Always 0xFF for solid colors
} ColorPreset;

static const ColorPreset COLOR_PRESETS[] = {
    // Basic colors
    {"Black", 0x00, 0x00, 0x00, 0xFF},
    {"White", 0xFF, 0xFF, 0xFF, 0xFF},
    {"Gray", 0x80, 0x80, 0x80, 0xFF},

    // Primary colors
    {"Red", 0xFF, 0x00, 0x00, 0xFF},
    {"Green", 0x00, 0xFF, 0x00, 0xFF},
    {"Blue", 0x00, 0x00, 0xFF, 0xFF},

    // Secondary colors
    {"Yellow", 0xFF, 0xFF, 0x00, 0xFF},
    {"Cyan", 0x00, 0xFF, 0xFF, 0xFF},
    {"Magenta", 0xFF, 0x00, 0xFF, 0xFF},

    // Common filament colors
    {"Orange", 0xFF, 0xA5, 0x00, 0xFF},
    {"Purple", 0x80, 0x00, 0x80, 0xFF},
    {"Pink", 0xFF, 0xC0, 0xCB, 0xFF},
    {"Brown", 0x8B, 0x45, 0x13, 0xFF},
    {"Beige", 0xF5, 0xF5, 0xDC, 0xFF},
    {"Navy", 0x00, 0x00, 0x80, 0xFF},
    {"Teal", 0x00, 0x80, 0x80, 0xFF},
    {"Olive", 0x80, 0x80, 0x00, 0xFF},
    {"Maroon", 0x80, 0x00, 0x00, 0xFF},

    // Metallics / Special
    {"Silver", 0xC0, 0xC0, 0xC0, 0xFF},
    {"Gold", 0xFF, 0xD7, 0x00, 0xFF},

    // Natural / Translucent
    {"Natural", 0xFD, 0xF5, 0xE6, 0xFF},
    {"Clear", 0xFF, 0xFF, 0xFF, 0x80},
};

#define COLOR_PRESET_COUNT (sizeof(COLOR_PRESETS) / sizeof(COLOR_PRESETS[0]))

// ============================================
// Weight Presets (grams)
// ============================================
static const uint16_t WEIGHT_PRESETS[] = {
    250,   // Small spool
    500,   // Medium spool
    1000,  // Standard spool (1kg)
    2000,  // Large spool (2kg)
    3000,  // Bulk spool (3kg)
};

#define WEIGHT_PRESET_COUNT (sizeof(WEIGHT_PRESETS) / sizeof(WEIGHT_PRESETS[0]))

// ============================================
// Tag Data Structure (for programming)
// ============================================
typedef struct {
    // User selections
    uint8_t filament_index;  // Index into BAMBU_FILAMENTS
    uint8_t color_index;     // Index into COLOR_PRESETS
    uint16_t weight_grams;   // Spool weight in grams

    // Derived from UID after scanning
    uint8_t uid[10];
    uint8_t uid_len;
} TagProgramData;

// ============================================
// Block data helpers
// ============================================

// Prepare Block 1 data: material_variant (0-7) + material_id (8-15)
static inline void prepare_block1(uint8_t* block, const FilamentInfo* filament) {
    memset(block, 0, 16);
    // Material variant (bytes 0-7) - leave empty/zero if not specified
    if(filament->material_variant[0] != '\0') {
        size_t len = strlen(filament->material_variant);
        if(len > 8) len = 8;
        memcpy(block, filament->material_variant, len);
    }
    // Material ID (bytes 8-15)
    size_t id_len = strlen(filament->material_id);
    if(id_len > 8) id_len = 8;
    memcpy(&block[8], filament->material_id, id_len);
}

// Prepare Block 2 data: filament type string
static inline void prepare_block2(uint8_t* block, const FilamentInfo* filament) {
    memset(block, 0, 16);
    size_t len = strlen(filament->filament_type);
    if(len > 16) len = 16;
    memcpy(block, filament->filament_type, len);
}

// Prepare Block 4 data: detailed filament type
static inline void prepare_block4(uint8_t* block, const FilamentInfo* filament) {
    memset(block, 0, 16);
    size_t len = strlen(filament->display_name);
    if(len > 16) len = 16;
    memcpy(block, filament->display_name, len);
}

// Prepare Block 5 data: RGBA (0-3) + weight (4-5 little endian) + reserved
static inline void prepare_block5(uint8_t* block, const ColorPreset* color, uint16_t weight_grams) {
    memset(block, 0, 16);
    // RGBA color (bytes 0-3)
    block[0] = color->r;
    block[1] = color->g;
    block[2] = color->b;
    block[3] = color->a;
    // Weight in grams (bytes 4-5, little endian)
    block[4] = (uint8_t)(weight_grams & 0xFF);
    block[5] = (uint8_t)((weight_grams >> 8) & 0xFF);
}

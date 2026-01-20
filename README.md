# Bambu Tagger

![Build Status](https://github.com/ductai199x/BambuTagger/actions/workflows/build.yml/badge.svg)

A Flipper Zero application for reading and programming Bambu Lab filament RFID tags.

## Features

- **Read Tag** - Read filament data from Bambu Lab spool tags (material type, color, weight)
- **Program Tag** - Create new filament tags on blank MIFARE Classic 1K cards
- **Save/Load Tags** - Save read tags to SD card and clone them to new tags
- **Bambu Tag Detection** - Automatically detects original Bambu tags (which cannot be reprogrammed due to read-only access bits)

## Requirements

### Hardware
- Flipper Zero
- Blank MIFARE Classic 1K tags (CUID/Chinese magic cards recommended for full compatibility)

### Important Notes
- **Original Bambu tags cannot be reprogrammed** - They have read-only access bits set by Bambu Lab
- This app is for programming **blank tags** to work with Bambu Lab printers
- The app derives encryption keys from the tag's UID using Bambu's key derivation algorithm

## Supported Filaments

The app includes presets for common Bambu Lab filament types:

| Category | Types |
|----------|-------|
| PLA | Basic, Matte, Metal, Silk, Sparkle, Tough, PLA-CF, Generic |
| PETG | Basic, Translucent, HF, PETG-CF, Generic |
| ABS | Standard, ABS-GF, Generic |
| ASA | Standard, ASA-Aero, Generic |
| TPU | 95A HF, 95A, TPU for AMS, Generic |
| PA (Nylon) | PA-CF, PA6-CF, Generic |
| PC | Standard, Generic |
| PET-CF | Standard |
| Support | Support W, Support G, Support PLA, PVA |

## Usage

### Reading a Tag
1. Select **Read Tag** from the main menu
2. Place the Bambu spool tag on the back of your Flipper
3. View the tag data (material ID, type, color, weight)
4. Optionally save the tag data to SD card

### Programming a New Tag
1. Select **Program Tag** from the main menu
2. Choose filament type from the list
3. Select color preset
4. Set spool weight (250g - 3000g)
5. Confirm settings and place a **blank** MIFARE Classic 1K tag on Flipper
6. Wait for programming to complete

### Cloning a Saved Tag
1. Select **Saved Tags** from the main menu
2. Choose a previously saved tag
3. Press **Clone** to write the data to a new blank tag

## Building

### Prerequisites
Install [ufbt](https://github.com/flipperdevices/flipperzero-ufbt) (Flipper Zero build tool):

```bash
# Using pip
pip install ufbt

# Or using uv
uv tool install ufbt
```

### Build Commands
```bash
# Build the app
ufbt

# Build and deploy to connected Flipper
ufbt launch
```

### Project Structure
```
├── bambu_tagger.c      # Main entry point, app lifecycle
├── bambu_tagger.h      # Shared types and structures
├── scenes.c/h          # UI scene handlers
├── nfc_operations.c/h  # NFC scanner/poller callbacks
├── tag_storage.c/h     # Save/load tag files
├── bambu_crypto.c/h    # Key derivation algorithm
├── bambu_tag_data.h    # Filament definitions and block helpers
└── application.fam     # App manifest
```

## Tag Data Format

Bambu Lab tags use MIFARE Classic 1K with the following block layout:

| Block | Content |
|-------|---------|
| 0 | UID, BCC, manufacturer data (read-only) |
| 1 | Material variant + Material ID |
| 2 | Filament type string (e.g., "PLA") |
| 3 | Sector 0 trailer (keys + access bits) |
| 4 | Detailed filament type (e.g., "PLA Basic") |
| 5 | Color RGBA + Weight (little-endian) |
| 6 | Additional data |
| 7 | Sector 1 trailer (keys + access bits) |

Saved tags are stored in `/ext/apps_data/bambu_tagger/` with the `.btag` extension.

## Acknowledgments

- Key derivation algorithm based on [SpoolEase](https://github.com/yanshay/SpoolEase) by yanshay
- Built with the [Flipper Zero](https://flipperzero.one/) SDK

## License

This project is for educational and personal use. Use responsibly and respect intellectual property rights.

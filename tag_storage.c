/**
 * @file tag_storage.c
 * @brief Tag file save/load operations for SD card storage
 * @author Tai Nguyen <taiducnguyen.drexel@gmail.com>
 */

#include "tag_storage.h"

bool ensure_storage_dir(Storage* storage) {
    if(!storage_dir_exists(storage, BAMBU_TAGGER_FOLDER)) {
        return storage_simply_mkdir(storage, BAMBU_TAGGER_FOLDER);
    }
    return true;
}

bool save_tag_to_file(App* app) {
    if(!ensure_storage_dir(app->storage)) {
        FURI_LOG_E(TAG, "Failed to create storage directory");
        return false;
    }

    // Generate filename from UID
    FuriString* path = furi_string_alloc();
    furi_string_printf(
        path,
        "%s/%02X%02X%02X%02X%s",
        BAMBU_TAGGER_FOLDER,
        app->tag_data.uid[0],
        app->tag_data.uid[1],
        app->tag_data.uid[2],
        app->tag_data.uid[3],
        BAMBU_TAGGER_EXTENSION);

    File* file = storage_file_alloc(app->storage);
    bool success = false;

    if(storage_file_open(file, furi_string_get_cstr(path), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FuriString* data = furi_string_alloc();

        // Write header
        furi_string_printf(data, "Filetype: Bambu Tag\nVersion: 1\n");

        // Write UID
        furi_string_cat_printf(
            data,
            "UID: %02X %02X %02X %02X\n",
            app->tag_data.uid[0],
            app->tag_data.uid[1],
            app->tag_data.uid[2],
            app->tag_data.uid[3]);
        furi_string_cat_printf(data, "UID_len: %d\n", app->tag_data.uid_len);

        // Write block data as hex
        furi_string_cat(data, "Block_1:");
        for(int i = 0; i < 16; i++) {
            furi_string_cat_printf(data, " %02X", app->read_data.block1[i]);
        }
        furi_string_cat(data, "\n");

        furi_string_cat(data, "Block_2:");
        for(int i = 0; i < 16; i++) {
            furi_string_cat_printf(data, " %02X", app->read_data.block2[i]);
        }
        furi_string_cat(data, "\n");

        furi_string_cat(data, "Block_4:");
        for(int i = 0; i < 16; i++) {
            furi_string_cat_printf(data, " %02X", app->read_data.block4[i]);
        }
        furi_string_cat(data, "\n");

        furi_string_cat(data, "Block_5:");
        for(int i = 0; i < 16; i++) {
            furi_string_cat_printf(data, " %02X", app->read_data.block5[i]);
        }
        furi_string_cat(data, "\n");

        if(storage_file_write(file, furi_string_get_cstr(data), furi_string_size(data)) ==
           furi_string_size(data)) {
            success = true;
            FURI_LOG_I(TAG, "Tag saved to %s", furi_string_get_cstr(path));
        }

        furi_string_free(data);
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_string_free(path);

    return success;
}

bool load_tag_from_file(App* app, const char* path) {
    File* file = storage_file_alloc(app->storage);
    bool success = false;

    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint64_t file_size = storage_file_size(file);
        if(file_size > 0 && file_size < 4096) {
            char* buffer = malloc(file_size + 1);
            if(storage_file_read(file, buffer, file_size) == file_size) {
                buffer[file_size] = '\0';

                // Parse UID
                char* uid_line = strstr(buffer, "UID:");
                if(uid_line) {
                    unsigned int u0, u1, u2, u3;
                    if(sscanf(uid_line, "UID: %02X %02X %02X %02X", &u0, &u1, &u2, &u3) == 4) {
                        app->tag_data.uid[0] = u0;
                        app->tag_data.uid[1] = u1;
                        app->tag_data.uid[2] = u2;
                        app->tag_data.uid[3] = u3;
                    }
                }

                // Parse UID length
                char* uid_len_line = strstr(buffer, "UID_len:");
                if(uid_len_line) {
                    int len;
                    if(sscanf(uid_len_line, "UID_len: %d", &len) == 1) {
                        app->tag_data.uid_len = len;
                    }
                }

                // Parse blocks
                char* block1_line = strstr(buffer, "Block_1:");
                if(block1_line) {
                    unsigned int b[16];
                    if(sscanf(
                           block1_line,
                           "Block_1: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                           &b[0], &b[1], &b[2], &b[3], &b[4], &b[5], &b[6], &b[7],
                           &b[8], &b[9], &b[10], &b[11], &b[12], &b[13], &b[14], &b[15]) == 16) {
                        for(int i = 0; i < 16; i++) app->read_data.block1[i] = b[i];
                    }
                }

                char* block2_line = strstr(buffer, "Block_2:");
                if(block2_line) {
                    unsigned int b[16];
                    if(sscanf(
                           block2_line,
                           "Block_2: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                           &b[0], &b[1], &b[2], &b[3], &b[4], &b[5], &b[6], &b[7],
                           &b[8], &b[9], &b[10], &b[11], &b[12], &b[13], &b[14], &b[15]) == 16) {
                        for(int i = 0; i < 16; i++) app->read_data.block2[i] = b[i];
                    }
                }

                char* block4_line = strstr(buffer, "Block_4:");
                if(block4_line) {
                    unsigned int b[16];
                    if(sscanf(
                           block4_line,
                           "Block_4: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                           &b[0], &b[1], &b[2], &b[3], &b[4], &b[5], &b[6], &b[7],
                           &b[8], &b[9], &b[10], &b[11], &b[12], &b[13], &b[14], &b[15]) == 16) {
                        for(int i = 0; i < 16; i++) app->read_data.block4[i] = b[i];
                    }
                }

                char* block5_line = strstr(buffer, "Block_5:");
                if(block5_line) {
                    unsigned int b[16];
                    if(sscanf(
                           block5_line,
                           "Block_5: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                           &b[0], &b[1], &b[2], &b[3], &b[4], &b[5], &b[6], &b[7],
                           &b[8], &b[9], &b[10], &b[11], &b[12], &b[13], &b[14], &b[15]) == 16) {
                        for(int i = 0; i < 16; i++) app->read_data.block5[i] = b[i];
                    }
                }

                app->read_data.valid = true;
                success = true;
                FURI_LOG_I(TAG, "Tag loaded from %s", path);
            }
            free(buffer);
        }
    }

    storage_file_close(file);
    storage_file_free(file);

    return success;
}

void load_saved_tags_list(App* app) {
    app->saved_tags_count = 0;

    File* dir = storage_file_alloc(app->storage);
    if(storage_dir_open(dir, BAMBU_TAGGER_FOLDER)) {
        FileInfo info;
        char name[64];

        while(storage_dir_read(dir, &info, name, sizeof(name)) && app->saved_tags_count < MAX_SAVED_TAGS) {
            if(!(info.flags & FSF_DIRECTORY)) {
                // Check extension
                size_t len = strlen(name);
                size_t ext_len = strlen(BAMBU_TAGGER_EXTENSION);
                if(len > ext_len && strcmp(name + len - ext_len, BAMBU_TAGGER_EXTENSION) == 0) {
                    strncpy(app->saved_tags[app->saved_tags_count], name, 63);
                    app->saved_tags[app->saved_tags_count][63] = '\0';
                    app->saved_tags_count++;
                }
            }
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);

    FURI_LOG_I(TAG, "Found %d saved tags", app->saved_tags_count);
}

bool delete_saved_tag(App* app, const char* filename) {
    FuriString* path = furi_string_alloc();
    furi_string_printf(path, "%s/%s", BAMBU_TAGGER_FOLDER, filename);

    bool success = storage_simply_remove(app->storage, furi_string_get_cstr(path));
    if(success) {
        FURI_LOG_I(TAG, "Deleted tag: %s", filename);
    } else {
        FURI_LOG_E(TAG, "Failed to delete tag: %s", filename);
    }

    furi_string_free(path);
    return success;
}

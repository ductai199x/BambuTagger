#pragma once

#include "bambu_tagger.h"

// Ensure storage directory exists
bool ensure_storage_dir(Storage* storage);

// Save current tag data to file
bool save_tag_to_file(App* app);

// Load tag data from file
bool load_tag_from_file(App* app, const char* path);

// Load list of saved tags
void load_saved_tags_list(App* app);

// Delete a saved tag file
bool delete_saved_tag(App* app, const char* filename);

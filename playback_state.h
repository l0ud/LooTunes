#pragma once

#include <cstdint>

struct PlaybackState {
    uint32_t current_dir_index;
    uint32_t current_track_index;
    uint32_t rand_key;
    uint32_t tracks_in_current_dir;
    bool has_file;

    enum class Mode {
        SENSOR, // light sensor state
        FORCED_ON,
        FORCED_OFF, // forced off state
        LAST_ELEMENT // marker for calculation
    } mode;

    bool load_from_file(const char* filename);
    bool save_to_file(const char* filename);
    void regenerate();
    void regenerate_key();
};

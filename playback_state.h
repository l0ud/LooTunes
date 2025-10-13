/*
 * Copyright (c) 2025 Przemysław Romaniak
 * 
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the root directory for details.
*/

#pragma once

#include <cstdint>

struct PlaybackState {
    uint32_t current_dir_index;
    uint32_t current_track_index;
    uint32_t rand_key;
    uint32_t tracks_in_current_dir;

    enum class Mode {
        Sensor, // light sensor state
        ForcedOn,
        ForcedOff, // forced off state
        LastElement // marker for calculation
    } mode;

    bool load_from_file(const char* filename);
    bool save_to_file(const char* filename);
    void regenerate(uint32_t seed);
    void regenerate_key(uint32_t seed);
};

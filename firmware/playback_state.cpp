/*
 * Copyright (c) 2025 Przemysław Romaniak
 * 
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the root directory for details.
*/

#include "playback_state.h"
#include "petitfat/source/pff.h"
#include "random.h"

bool PlaybackState::load_from_file(const char* filename) {
    // Open file
    FRESULT res = pf_open(filename);
    if (res != FR_OK) {
        return false;
    }

    // read all members from binary file
    UINT br;
    pf_read(&current_dir_index, sizeof(current_dir_index), &br);
    pf_read(&current_track_index, sizeof(current_track_index), &br);
    pf_read(&rand_key, sizeof(rand_key), &br);
    pf_read(&tracks_in_current_dir, sizeof(tracks_in_current_dir), &br);
    pf_read(&mode, sizeof(mode), &br);
    return true;
}

bool PlaybackState::save_to_file(const char* filename) {
    // Open file
    FRESULT res = pf_open(filename);
    if (res != FR_OK) {
        return false;
    }

    // write all members to binary file
    UINT br;
    pf_write(&current_dir_index, sizeof(current_dir_index), &br);
    pf_write(&current_track_index, sizeof(current_track_index), &br);
    pf_write(&rand_key, sizeof(rand_key), &br);
    pf_write(&tracks_in_current_dir, sizeof(tracks_in_current_dir), &br);
    pf_write(&mode, sizeof(mode), &br);
    pf_write(0, 0, &br); // finalize write operation

    return true;
}

void PlaybackState::regenerate(uint32_t seed) {
    current_dir_index = -1;
    current_track_index = -1;
    tracks_in_current_dir = 0;
    regenerate_key(seed);
}

void PlaybackState::regenerate_key(uint32_t seed) {
    if (seed) {
        rand_key = seed;
    }
    else {
        rand_key = RAND::next();
    }
}

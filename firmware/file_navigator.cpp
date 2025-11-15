/*
 * Copyright (c) 2025 Przemysław Romaniak
 * 
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the root directory for details.
*/

#include "file_navigator.h"
#include "petitfat/source/pff.h"
#include "playback_state.h"
#include "config.h"
#include "feistel.h"
#include "gpio.h"



namespace FileNavigator {


constexpr const char* StateFileName = "STATE.BIN";
constexpr const char* ConfigFileName = "CONFIG.INI";

namespace {
    FATFS fs;
    PlaybackState nv_state = {0};
    DIR main_dir = {0};
    DIR sub_dir = {0};
    uint32_t subdir_iter = -1;
    FILINFO current_file = {0};
    volatile bool save_state_requested = false;
}

uint32_t translate_track_number(uint32_t track) {
    if (CFG.random_mode) {
        return permute(track, nv_state.tracks_in_current_dir, nv_state.rand_key, 3);
    }
    else {
        return track;
    }
}

bool init() {
    FRESULT res = pf_mount(&fs);
    
    if (res != FR_OK) {
        return false;
    }

    // load config file
    CFG.load_from_file(ConfigFileName);

    // apply static usb modes, if selected
    if (CFG.usb_mode == Config::UsbMode::AlwaysOn) {
        GPIO::usb_power_on();
    } else if (CFG.usb_mode == Config::UsbMode::AlwaysOff) {
        GPIO::usb_power_off();
    }

    // load playback state
    if (CFG.save_state == Config::SaveState::Disabled || !nv_state.load_from_file(StateFileName)) {
        // failed to load state / state disabled, reset to defaults
        nv_state.regenerate(CFG.seed);
        CFG.save_state = Config::SaveState::Disabled; // disable saving state if loading failed
    }

    return true;
}

bool open_main_directory() {
    FRESULT res = pf_opendir(&main_dir, "/");
    return res == FR_OK;
}

bool restore_state() {
    bool state_restored = true;
    
    while(1) {
        if (CFG.saving_enabled(Config::SaveState::SaveDirectory) == false) {
            state_restored = false;
            break;
        }

        if (CFG.seed && nv_state.rand_key != CFG.seed) {
            // if seed defined by user and different, for convenience reset state
            // to avoid confusion
            state_restored = false;
            break;
        }

        // skip to correct subdirectory
        FRESULT res = pf_readdir_n_element(&main_dir, nv_state.current_dir_index, &current_file);
        if (res == FR_NO_FILE) {
            state_restored = false;
            break;
        }
        if (res != FR_OK) {
            return false;
        }

        if (!(current_file.fattrib & AM_DIR)) {
            return false;
        }

        res = pf_opendir(&sub_dir, current_file.fname);
        if (res != FR_OK) {
            return false;
        }

        // now jump to the file from state, if this is enabled in config
        uint32_t count = pf_countindir(&sub_dir);

        if (count != nv_state.tracks_in_current_dir || nv_state.tracks_in_current_dir == 0) {
            state_restored = false;
            break;
        }

        if (CFG.saving_enabled(Config::SaveState::SaveTrack)) {
            uint32_t next_track = translate_track_number(nv_state.current_track_index);

            res = pf_readdir_n_element(&sub_dir, next_track, &current_file);
            if (res == FR_NO_FILE) {
                state_restored = false;
                break;
            }

            if (res != FR_OK) {
                return false;
            }
            subdir_iter = next_track;
            break;
        }
        else {
            // get random key as track saving is not enabled
            nv_state.regenerate_key(CFG.seed);
            nv_state.current_track_index = -1;
            // go to first track
            if (!next_track()) {
                return false;
            }
        }
    }

    if (!state_restored) {
        nv_state.regenerate(CFG.seed);
        // rewind main dir
        pf_readdir(&main_dir, nullptr);
        // go to first dir
        if (!next_dir()) {
            return false;
        }
        // go to first track
        if (!next_track()) {
            return false;
        }
    }

    return true;
}

bool next_track_in_dir(bool &next_dir) {
    while(1) {
        if (++nv_state.current_track_index >= nv_state.tracks_in_current_dir) {
            // reached end of dir
            // decide what to do depending on config
            if (CFG.jump_next_dir) {
                next_dir = true; // signal to caller to jump to next dir
                return true;
            }

            nv_state.current_track_index = -1;
            continue;
        }

        FRESULT res = FR_OK;
        uint32_t next_track = translate_track_number(nv_state.current_track_index);

        if (subdir_iter + 1 == next_track) {
            // quick skip to next file
            subdir_iter++;
            res = pf_readdir(&sub_dir, &current_file);
        }
        else {
            // slower seek to next file
            res = pf_readdir_n_element(&sub_dir, next_track, &current_file);
            subdir_iter = next_track;
        }

        if (res != FR_OK) {
            return false;
        }

        if (!(current_file.fattrib & AM_DIR)) {
            // found next file
            break;
        }
    }

    return true;
}

bool next_track() {
    bool next_dir_requested = false;
    do {
        next_track_in_dir(next_dir_requested);
        if (next_dir_requested) {
            if (!next_dir()) {
                return false;
            }
        }
    }
    while(next_dir_requested);

    return true;
}

bool prev_track() {
    if (nv_state.current_track_index > 0) {
        nv_state.current_track_index--;
    }

    uint32_t track = translate_track_number(nv_state.current_track_index);

    FRESULT res = pf_readdir_n_element(&sub_dir, track, &current_file);
    subdir_iter = track;

    if (res != FR_OK) {
        return false;
    }

    return true;
}

bool next_dir() {
    while(1) {
        while(1) {
            FRESULT res = pf_readdir(&main_dir, &current_file);
            if (res != FR_OK) {
                return false;
            }

            if (current_file.fname[0] == 0) {
                // reached end of main dir
                // rewind to start
                if (pf_readdir(&main_dir, nullptr) != FR_OK) {
                    return false;
                }

                nv_state.current_dir_index = -1;
                continue;
            }

            nv_state.current_dir_index++;
            if (current_file.fattrib & AM_DIR) {
                // found next dir
                break;
            }
        }

        FRESULT res = pf_opendir(&sub_dir, current_file.fname);
        if (res != FR_OK) {
            return false;
        }

        nv_state.tracks_in_current_dir = pf_countindir(&sub_dir);
        if (nv_state.tracks_in_current_dir > 0) {
            break; // found directory with files
        }
    }

    subdir_iter = -1;
    nv_state.current_track_index = -1;
    nv_state.regenerate_key(CFG.seed);
    return true;
}

FILINFO* get_current_file() {
    return &current_file;
}

PlaybackState& get_state() {
    return nv_state;
}

void request_state_save() {
    save_state_requested = true;
}

bool is_state_save_requested() {
    return save_state_requested;
}

void handle_state_save() {
    // Simple state save - only used outside of playback
    nv_state.save_to_file(StateFileName);
    save_state_requested = false;
}

} // namespace FileNavigator

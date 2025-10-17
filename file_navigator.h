/*
 * Copyright (c) 2025 Przemysław Romaniak
 * 
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the root directory for details.
*/

#pragma once

#include <cstdint>
#include "petitfat/source/pff.h"
#include "playback_state.h"

namespace FileNavigator {

/**
 * @brief Initialize filesystem and load state
 * @return true if successful, false otherwise
 */
bool init();

/**
 * @brief Open the main directory
 * @return true if successful, false otherwise
 */
bool open_main_directory();

/**
 * @brief Restore playback state from saved state file
 * @return true if state restored or reset successfully, false on error
 */
bool restore_state();

/**
 * @brief Move to the next track in the current directory
 * @param[out] next_dir Set to true if end of directory reached and should jump to next dir
 * @return true if successful, false on error
 */
bool next_track_in_dir(bool &next_dir);

/**
 * @brief Move to the next track (handles directory boundaries)
 * @return true if successful, false on error
 */
bool next_track();

/**
 * @brief Move to the previous track
 * @return true if successful, false on error
 */
bool prev_track();

/**
 * @brief Move to the next directory
 * @return true if successful, false on error
 */
bool next_dir();

/**
 * @brief Get the current file info
 * @return Pointer to current FILINFO structure
 */
FILINFO* get_current_file();

/**
 * @brief Get the current playback state
 * @return Reference to current PlaybackState
 */
PlaybackState& get_state();

/**
 * @brief Request state to be saved to file
 */
void request_state_save();

/**
 * @brief Check if state save is requested
 * @return true if save is pending
 */
bool is_state_save_requested();

/**
 * @brief Perform the state save operation
 */
void handle_state_save();

} // namespace FileNavigator

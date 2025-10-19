/*
 * Copyright (c) 2025 Przemysław Romaniak
 * 
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the root directory for details.
*/

#pragma once

#include <cstdint>
#include "petitfat/source/pff.h"

namespace AudioPlayer {

enum class PlaybackCommand : uint8_t {
    KeepPlaying = 0,
    NextTrack = 1,
    PrevTrack = 2,
    NextDirectory = 3,
};

/**
 * @brief Initialize audio hardware (timers and DMA)
 */
void init();

/**
 * @brief Initialize timer for PWM audio output
 */
void init_timer();

/**
 * @brief Initialize DMA for double-buffered audio streaming
 */
void init_dma();

/**
 * @brief Mute audio output (stop DMA and set to silence)
 */
void mute();

/**
 * @brief Unmute audio output (resume DMA)
 */
void unmute();

/**
 * @brief Reset mute reference counter (for use after SD card re-initialization)
 */
void reset_mute();

/**
 * @brief Play a single audio file
 * @param file Pointer to FILINFO structure of file to play
 * @param[out] command Playback command requested during playback
 * @return true if file played successfully, false on error
 */
bool play_file(FILINFO *file, PlaybackCommand &command);

/**
 * @brief Set playback command to interrupt current playback
 * @param command Playback command
 */
void set_playback_command(PlaybackCommand command);

} // namespace AudioPlayer

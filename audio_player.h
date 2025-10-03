#pragma once

#include <cstdint>
#include "petitfat/source/pff.h"

namespace AudioPlayer {

// =============================================================================
// Types
// =============================================================================

enum class PlaybackCommand : uint8_t {
    KeepPlaying = 0,
    NextTrack = 1,
    PrevTrack = 2,
    NextDirectory = 3,
};

// =============================================================================
// Audio Playback Control
// =============================================================================

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
 * @brief Start audio playback (enable timer)
 */
void start_playback();

/**
 * @brief Stop audio playback (disable timer)
 */
void stop_playback();

/**
 * @brief Mute audio output (switch to silence buffer)
 */
void mute();

/**
 * @brief Unmute audio output (switch to PCM buffers)
 */
void unmute();

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

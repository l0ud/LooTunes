#include "controller.h"
#include "file_navigator.h"
#include "audio_player.h"
#include "gpio.h"
#include "config.h"
#include "light_sensor.h"
#include "playback_state.h"

extern "C" {
    #include "defines.h"

    uint8_t VolumeShift = 0;
}

// Global callback functions for external C interfaces
void ButtonPressCallback(BTN::ID id) {
    Controller::on_button_press(id);
}

void LightSensorCallback(uint16_t value) {
    Controller::on_light_sensor(value);
}

namespace Controller {

// =============================================================================
// Internal Types and Enums
// =============================================================================

using AudioPlayer::PlaybackCommand;

enum class PState {
    NOT_PLAYING,
    PLAYING,
};

// =============================================================================
// Forward Declarations
// =============================================================================

void change_main_state(PlaybackState::Mode new_state);
void change_playing_state(PState new_state);
void set_thresholds_for_state();

namespace {
    // Playback state
    PState p_state = PState::NOT_PLAYING;
}

// =============================================================================
// Initialization Functions
// =============================================================================

void init() {
    AudioPlayer::init();
}

bool init_sd() {
    if (!FileNavigator::init()) {
        return false;
    }

    PlaybackState& nv_state = FileNavigator::get_state();

    if (CFG.saving_enabled(Config::SaveState::SaveMode)) {
        change_main_state(nv_state.mode);
    }
    else {
        // do defaults
        if (CFG.light_mode != Config::LightMode::Disabled) {
            change_main_state(PlaybackState::Mode::SENSOR);
        }
        else {
            change_main_state(PlaybackState::Mode::FORCED_OFF);
        }
    }

    if (nv_state.mode == PlaybackState::Mode::SENSOR) {
        change_playing_state(PState::PLAYING);
        set_thresholds_for_state();
    }

    return true;
}

// =============================================================================
// State Management Functions
// =============================================================================

void set_thresholds_for_state()
{
    const bool arm_to_dim = (p_state == PState::PLAYING
            && CFG.light_mode == Config::LightMode::Normal)
            || (p_state == PState::NOT_PLAYING
            && CFG.light_mode == Config::LightMode::Reversed);
    // closer to 0 the sensor value is, the more light is present
    if (arm_to_dim) {
        // we're playing - arm light sensor for powering off
        LIGHT::set_thresholds(0, CFG.off_threshold);
    }
    else {
        // arm light sensor for dim condition
        LIGHT::set_thresholds(CFG.on_threshold, 0xfff);
    }
}

void on_button_press(BTN::ID id)
{
    PlaybackState& nv_state = FileNavigator::get_state();
    
    switch (id) {
        case BTN::ID::POWER:
        {
            PlaybackState::Mode new_state = static_cast<PlaybackState::Mode>((static_cast<int>(nv_state.mode) + 1) % static_cast<int>(PlaybackState::Mode::LAST_ELEMENT));

            if (new_state == PlaybackState::Mode::SENSOR && CFG.light_mode == Config::LightMode::Disabled) {
                // If light sensor is disabled, skip to FORCED_OFF state
                new_state = PlaybackState::Mode::FORCED_ON;
            }

            change_main_state(new_state);
            if (CFG.saving_enabled(Config::SaveState::SaveMode)) {
                FileNavigator::request_state_save();
            }
            break;
        }
        case BTN::ID::NEXT_DIR:
            AudioPlayer::set_playback_command(PlaybackCommand::NextDirectory);
            break;
        case BTN::ID::NEXT:
            AudioPlayer::set_playback_command(PlaybackCommand::NextTrack);
            break;
        case BTN::ID::PREV:
            AudioPlayer::set_playback_command(PlaybackCommand::PrevTrack);
            break;
    }
}

void on_light_sensor(uint16_t value)
{
    PlaybackState& nv_state = FileNavigator::get_state();
    
    if (nv_state.mode == PlaybackState::Mode::SENSOR) {
        if (value < CFG.on_threshold) {
            // turn on
            change_playing_state(CFG.light_mode == Config::LightMode::Normal ? PState::PLAYING : PState::NOT_PLAYING);
        } else if (value > CFG.off_threshold) {
            // turn off
            change_playing_state(CFG.light_mode == Config::LightMode::Normal ? PState::NOT_PLAYING : PState::PLAYING);
        }

        set_thresholds_for_state();
    }
}

void change_main_state(PlaybackState::Mode new_state)
{
    PlaybackState& nv_state = FileNavigator::get_state();
    
    if (new_state == PlaybackState::Mode::FORCED_OFF) {
        LIGHT::stop();
        change_playing_state(PState::NOT_PLAYING);
    } else if (new_state == PlaybackState::Mode::FORCED_ON) {
        LIGHT::stop();
        change_playing_state(PState::PLAYING);
    } else if (new_state == PlaybackState::Mode::SENSOR) {
        // Handle sensor state, if needed
        set_thresholds_for_state();
        LIGHT::start();
    }

    nv_state.mode = new_state;
}

void change_playing_state(PState new_state)
{
    if (p_state == new_state) {
        // No change needed
        return;
    }
    if (new_state == PState::NOT_PLAYING) {
        GPIO::led_off();

        if (CFG.usb_mode == Config::UsbMode::OnPlayback) {
            GPIO::usb_power_off();
        }
        AudioPlayer::stop_playback();
    } else if (new_state == PState::PLAYING) {
        GPIO::led_on();

        if (CFG.usb_mode == Config::UsbMode::OnPlayback) {
            GPIO::usb_power_on();
        }
        AudioPlayer::start_playback();
    }

    p_state = new_state;
}

// =============================================================================
// Main Playback Loop
// =============================================================================

bool main() {
    // list main directory
    if (!FileNavigator::open_main_directory()) {
        return false;
    }

    if (!FileNavigator::restore_state()) {
        return false;
    }

    FILINFO* current_file = FileNavigator::get_current_file();

    for (;;) {

        if (current_file->fname[0] == 0) {
            break;
        }

        PlaybackCommand command = PlaybackCommand::KeepPlaying;
        
        if (!AudioPlayer::play_file(current_file, command)) {
            // Error playing file, skip to next
            command = PlaybackCommand::NextTrack;
        }

        bool next_directory = false;

        if (command == PlaybackCommand::PrevTrack) {
            if (!FileNavigator::prev_track()) {
                return false;
            }
        }
        else if (command == PlaybackCommand::NextDirectory) {
            if (!FileNavigator::next_dir()) {
                return false;
            }

            next_directory = true;

            if (!FileNavigator::next_track()) { // start from first track
                return false;
            }
        }
        else {
            // next track
            if (!FileNavigator::next_track()) {
                return false;
            }
        }

        // save state
        if (CFG.saving_enabled(Config::SaveState::SaveTrack) || (CFG.saving_enabled(Config::SaveState::SaveDirectory) && next_directory)) {
            FileNavigator::handle_state_save();
        }
    }

    return true;
}

} // namespace Controller

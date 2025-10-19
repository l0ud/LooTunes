/*
 * Copyright (c) 2025 Przemysław Romaniak
 * 
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the root directory for details.
*/

#include "controller.h"
#include "file_navigator.h"
#include "audio_player.h"
#include "gpio.h"
#include "config.h"
#include "light_sensor.h"
#include "playback_state.h"

extern "C" {
    volatile uint8_t VolumeShift = 0;
}

// Global callback functions for external C interfaces
void ButtonPressCallback(BTN::ID id) {
    Controller::on_button_press(id);
}

void LightSensorCallback(uint16_t value) {
    Controller::on_light_sensor(value);
}

namespace Controller {

using AudioPlayer::PlaybackCommand;

enum class PState {
    Invalid,
    NotPlaying,
    FadeOut,
    FadeIn,
    Playing,
};

void change_main_state(PlaybackState::Mode new_state);
void change_playing_state(PState new_state, bool force = false);
void set_thresholds_for_state();

namespace {
    // Playback state
    PState p_state = PState::NotPlaying;
}

void init() {
    AudioPlayer::init();

    // initialize timer used for fade in and out
    __HAL_RCC_TIM16_CLK_ENABLE();
    // 1ms time base
    TIM16->PSC = (INPUT_FREQUENCY / 1000) - 1;
    // enable update interrupt
    TIM16->DIER |= TIM_DIER_UIE;
    NVIC_EnableIRQ(TIM16_IRQn);
    // don't start yet
    TIM16->CR1 = 0;
}

void fade_handler() {
    if (p_state == PState::FadeIn) {
        if (VolumeShift > 0) {
            VolumeShift = VolumeShift - 1;
        }
        else {
            // done
            TIM16->CR1 &= ~TIM_CR1_CEN; // stop timer
            Controller::change_playing_state(Controller::PState::Playing, true);
        }
    }
    else if (p_state == Controller::PState::FadeOut) {
        if (VolumeShift < 10) {
            VolumeShift = VolumeShift + 1;
        }
        else {
            // done
            TIM16->CR1 &= ~TIM_CR1_CEN; // stop timer
            Controller::change_playing_state(Controller::PState::NotPlaying, true);
        }
    }
}

void arm_fade_timer(uint8_t period_ms) {
    TIM16->CNT = 0;
    TIM16->ARR = period_ms;
    TIM16->CR1 |= TIM_CR1_CEN; // start timer
}

bool init_sd() {
    // re-initialize mute state after SD card init
    AudioPlayer::reset_mute();
    // reset playback state, needs to be loaded
    p_state = PState::Invalid;

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
            change_main_state(PlaybackState::Mode::Sensor);
        }
        else {
            change_main_state(PlaybackState::Mode::ForcedOff);
        }
    }

    return true;
}

void set_thresholds_for_state()
{
    const bool arm_to_dim = (p_state >= PState::FadeIn // playing or fade in
            && CFG.light_mode == Config::LightMode::Normal)
            || (p_state <= PState::FadeOut // not playing or fade out
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
            PlaybackState::Mode new_state = static_cast<PlaybackState::Mode>((static_cast<int>(nv_state.mode) + 1) % static_cast<int>(PlaybackState::Mode::LastElement));

            if (new_state == PlaybackState::Mode::Sensor && CFG.light_mode == Config::LightMode::Disabled) {
                // If light sensor is disabled, skip to ForcedOff state
                new_state = PlaybackState::Mode::ForcedOn;
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
    
    if (nv_state.mode == PlaybackState::Mode::Sensor) {
        if (value < CFG.on_threshold) {
            // turn on
            change_playing_state(CFG.light_mode == Config::LightMode::Normal ? PState::Playing : PState::NotPlaying);
        } else if (value > CFG.off_threshold) {
            // turn off
            change_playing_state(CFG.light_mode == Config::LightMode::Normal ? PState::NotPlaying : PState::Playing);
        }

        set_thresholds_for_state();
    }
}

void change_main_state(PlaybackState::Mode new_state)
{
    PlaybackState& nv_state = FileNavigator::get_state();

    if (new_state == PlaybackState::Mode::ForcedOff) {
        LIGHT::stop();
        change_playing_state(PState::NotPlaying, CFG.instant_mode_change != 0);
    } else if (new_state == PlaybackState::Mode::ForcedOn) {
        LIGHT::stop();
        change_playing_state(PState::Playing, CFG.instant_mode_change != 0);
    } else if (new_state == PlaybackState::Mode::Sensor) {
        // Handle sensor state, if needed

        // special case - initializing for first time after reset
        if (p_state == PState::Invalid) {
            // start in not playing state
            change_playing_state(PState::NotPlaying, true);
        }
        set_thresholds_for_state();
        LIGHT::start();
    }

    nv_state.mode = new_state;
}

void change_playing_state(PState new_state, bool force)
{
    if (p_state == new_state) {
        // No change needed
        return;
    }

    if (!force) { // make changes soft
        if (CFG.fade_out != 0 && new_state == PState::NotPlaying) {
            // fade out
            new_state = PState::FadeOut;
        }

        if (CFG.fade_in != 0 && new_state == PState::Playing) {
            // fade in
            new_state = PState::FadeIn;
        }
    }


    if (new_state == PState::NotPlaying) {
        GPIO::led_off();

        if (CFG.usb_mode == Config::UsbMode::OnPlayback) {
            GPIO::usb_power_off();
        }
        AudioPlayer::mute(); // this will create state-related mute lock
        VolumeShift = 10; // prepare shift for potential fade in
    }
    else if (new_state == PState::FadeIn || new_state == PState::Playing)  {
        GPIO::led_on();

        if (CFG.usb_mode == Config::UsbMode::OnPlayback) {
            GPIO::usb_power_on();
        }

        if (new_state == PState::FadeIn) {
            // Start fade-in timer
            arm_fade_timer(CFG.fade_in);
        }
        else {
            VolumeShift = 0; // instant on
        }

        if (p_state == PState::NotPlaying) {
            // avoid unmuting multiple times
            AudioPlayer::unmute(); // release state-related mute lock
        }

    }
    else if (new_state == PState::FadeOut) {
        // keep USB on during fade out, but not led
        GPIO::led_off();
        // Start fade-out timer
        arm_fade_timer(CFG.fade_out);
    }

    p_state = new_state;
}

// Main Playback Loop

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
        if (FileNavigator::is_state_save_requested()
            || CFG.saving_enabled(Config::SaveState::SaveTrack)
            || (CFG.saving_enabled(Config::SaveState::SaveDirectory) && next_directory)) {

            FileNavigator::handle_state_save();
        }
    }

    return true;
}

} // namespace Controller

void TIM16_IRQHandler() {
    if (TIM16->SR & TIM_SR_UIF) {
        TIM16->SR &= ~TIM_SR_UIF; // clear interrupt flag
        Controller::fade_handler();
    }
}
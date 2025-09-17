#include <cstddef>
#include <array>

#include "playback.h"
#include "libsbc/include/sbc.h"
#include "gpio.h"
#include "watchdog.h"
#include "config.h"
#include "light_sensor.h"
#include "feistel.h"
#include "petitfat/source/pff.h"
#include "playback_state.h"
#include "utility.h"

extern "C" {
    #include "defines.h"
    #include "py32f0xx.h"
    #include "py32f0xx_hal.h"
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
// Constants and Configuration
// =============================================================================

constexpr const char* StateFileName = "STATE.BIN";
constexpr const char* ConfigFileName = "CONFIG.INI";

// Audio buffer configuration
constexpr auto CHANNEL_HALF_BUFFER = SBC_MAX_SAMPLES; // 128
constexpr auto CHANNEL_FULL_BUFFER = CHANNEL_HALF_BUFFER * 2; // 256

// Timer configuration
constexpr uint32_t TIM_PERIOD_48KHZ = 250 - 1;   // 192 kHz PWM frequency
constexpr uint32_t TIM_PERIOD_441KHZ = 272 - 1;  // 176.4 kHz PWM frequency
constexpr uint32_t REPEAT_COUNT = 3;              // Update every 4 PWM cycles (RCR + 1 = 4)


// =============================================================================
// Internal Types and Enums
// =============================================================================

enum class PlaybackCommand : uint8_t {
    KeepPlaying = 0,
    NextTrack = 1,
    PrevTrack = 2,
    NextDirectory = 3,
};

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

void play_file(FILINFO *file);

bool next_track_in_dir(bool &next_dir);
bool next_track();
bool prev_track();
bool next_dir();

namespace {
    // Audio system state
    int16_t pcml[CHANNEL_FULL_BUFFER] = {0};
    int16_t pcmr[CHANNEL_FULL_BUFFER] = {0};
    uint8_t data[SBC_MAX_SAMPLES*sizeof(int16_t)] = {0};
    constexpr auto silence = make_filled_array<int16_t, CHANNEL_FULL_BUFFER, 136>();

    FATFS fs;

    // Synchronization flags
    volatile bool half_transfer = false;
    volatile bool transfer_complete = false;
    volatile bool save_state_requested = false;
    volatile PlaybackCommand playback_command = PlaybackCommand::KeepPlaying;

    // Playback state
    PState p_state = PState::NOT_PLAYING;
    PlaybackState nv_state = {0};
    DIR main_dir = {0};
    DIR sub_dir = {0};
    uint32_t subdir_iter = -1;
    FILINFO current_file = {0};
}

// =============================================================================
// Hardware Initialization Functions
// =============================================================================

void init_timer() {
    __HAL_RCC_TIM1_CLK_ENABLE();

    // Configure TIM1 as PWM output
    TIM1->PSC = 0;                     // No prescaler (48 MHz clock)
    TIM1->ARR = TIM_PERIOD_441KHZ;     // Set Auto-Reload Register for desired frequency
    TIM1->CCR2 = 0;                    // Set initial duty cycle to 0
    TIM1->CCR3 = 0;                    // Set initial duty cycle to 0
    TIM1->RCR = REPEAT_COUNT;          // Update every 4 PWM cycles

    TIM1->CCMR1 |= TIM_CCMR1_OC2M_2 | TIM_CCMR1_OC2M_1;  // PWM Mode 1 (110)
    TIM1->CCMR2 |= TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3M_1;  // PWM Mode 1 (110)

    TIM1->CCER |= TIM_CCER_CC2E | TIM_CCER_CC3E;  // Enable CH2 and CH3 output
    TIM1->DIER |= TIM_DIER_UDE;                   // Enable Update DMA Request
    TIM1->BDTR |= TIM_BDTR_MOE;                   // Main Output Enable
    TIM1->CR1 = TIM_CR1_ARPE;                     // Enable timer AUTO PRELOAD
}

void init_dma() {
    __HAL_RCC_DMA_CLK_ENABLE();

    // Configure DMA mapping
    SYSCFG->CFGR3 &= ~(SYSCFG_CFGR3_DMA1_MAP_Msk | SYSCFG_CFGR3_DMA2_MAP_Msk);
    SYSCFG->CFGR3 |= SYSCFG_CFGR3_DMA1_MAP_4 | SYSCFG_CFGR3_DMA2_MAP_4 | 
                        SYSCFG_CFGR3_DMA2_ACKLVL | SYSCFG_CFGR3_DMA1_ACKLVL;

    // Configure DMA Channel 1 (Left channel)
    DMA1_Channel1->CCR &= ~DMA_CCR_EN;
    DMA1_Channel1->CNDTR = CHANNEL_FULL_BUFFER;
    DMA1_Channel1->CPAR = (uint32_t)&TIM1->CCR2;
    DMA1_Channel1->CMAR = (uint32_t)pcml;
    
    DMA1_Channel1->CCR |= DMA_CCR_MINC |      // Memory increment mode
                            DMA_CCR_DIR |       // Memory-to-peripheral direction
                            DMA_CCR_MSIZE_0 |   // Memory size: 16-bit
                            DMA_CCR_PSIZE_0 |   // Peripheral size: 16-bit
                            DMA_CCR_CIRC |      // Circular mode
                            DMA_CCR_HTIE |      // Half transfer interrupt
                            DMA_CCR_TCIE;       // Transfer complete interrupt

    // Configure DMA Channel 2 (Right channel)
    DMA1_Channel2->CCR &= ~DMA_CCR_EN;
    DMA1_Channel2->CNDTR = CHANNEL_FULL_BUFFER;
    DMA1_Channel2->CPAR = (uint32_t)&TIM1->CCR3;
    DMA1_Channel2->CMAR = (uint32_t)pcmr;
    
    DMA1_Channel2->CCR |= DMA_CCR_MINC |      // Memory increment mode
                            DMA_CCR_DIR |       // Memory-to-peripheral direction
                            DMA_CCR_MSIZE_0 |   // Memory size: 16-bit
                            DMA_CCR_PSIZE_0 |   // Peripheral size: 16-bit
                            DMA_CCR_CIRC;       // Circular mode

    // Enable DMA channels and interrupt
    NVIC_EnableIRQ(DMA1_Channel1_IRQn);
    DMA1_Channel1->CCR |= DMA_CCR_EN;
    DMA1_Channel2->CCR |= DMA_CCR_EN;
}

void init() {
    init_timer();
    init_dma();
    init_sd();
}

// =============================================================================
// Filesystem and State Management Functions  
// =============================================================================

bool init_sd() {
    FRESULT res = pf_mount(&fs);
    WDT::feed();
    
    if (res != FR_OK) {
        // todo: might handle mount error
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
// Audio Control Functions
// =============================================================================

void start_playback() {
    TIM1->CR1 |= TIM_CR1_CEN;         // Enable the timer
}

void stop_playback() {
    TIM1->CR1 &= ~TIM_CR1_CEN;        // Disable the timer
}

void mute() {
    DMA1_Channel1->CMAR = (uint32_t)silence.data();
    DMA1_Channel2->CMAR = (uint32_t)silence.data();
}

void unmute() {
    DMA1_Channel1->CMAR = (uint32_t)pcml;
    DMA1_Channel2->CMAR = (uint32_t)pcmr;
}


uint32_t translate_track_number(uint32_t track) {
    if (CFG.random_mode) {
        return permute(track, nv_state.tracks_in_current_dir, nv_state.rand_key, 3);
    }
    else {
        return track;
    }
}

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

        // skip to correct subdirctory
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

// =============================================================================
// Track Navigation Functions
// =============================================================================

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

void on_button_press(BTN::ID id)
{
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
                save_state_requested = true;
            }
            break;
        }
        case BTN::ID::NEXT_DIR:
            playback_command = PlaybackCommand::NextDirectory;
            break;
        case BTN::ID::NEXT:
            playback_command = PlaybackCommand::NextTrack;
            break;
        case BTN::ID::PREV:
            playback_command = PlaybackCommand::PrevTrack;
            break;
    }
}

void on_light_sensor(uint16_t value)
{
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
        TIM1->CR1 &= ~TIM_CR1_CEN; // Disable timer
    } else if (new_state == PState::PLAYING) {
        GPIO::led_on();

        if (CFG.usb_mode == Config::UsbMode::OnPlayback) {
            GPIO::usb_power_on();
        }
        TIM1->CR1 |= TIM_CR1_CEN; // Enable timer
    }

    p_state = new_state;
}

void __attribute__ ((noinline)) handle_state_save() {
    // kinda ugly hack to avoid PetitFat forgetting about currently played file
    FATFS petit_state;
    pf_save_state(&petit_state);
    nv_state.save_to_file(StateFileName);
    pf_restore_state(&petit_state);
    save_state_requested = false;
}


// =============================================================================
// Playback loop
// =============================================================================

bool main() {
    // list main directory
    FRESULT res = pf_opendir(&main_dir, "/");
    if (res != FR_OK) {
        return false;
    }

    if (!restore_state()) {
        return false;
    }

    for (;;) {

        if (current_file.fname[0] == 0) {
            break;
        }

        playback_command = PlaybackCommand::KeepPlaying;
        play_file(&current_file);

        bool next_directory = false;

        if (playback_command == PlaybackCommand::PrevTrack) {
            if (!prev_track()) {
                return false;
            }
        }
        else if (playback_command == PlaybackCommand::NextDirectory) {
            if (!next_dir()) {
                return false;
            }

            next_directory = true;

            if (!next_track()) { // start from first track
                return false;
            }
        }
        else {
            // normal next track
            if (!next_track()) {
                return false;
            }
        }

        // save state
        if (CFG.saving_enabled(Config::SaveState::SaveTrack) || (CFG.saving_enabled(Config::SaveState::SaveDirectory) && next_directory)) {
            nv_state.save_to_file(StateFileName);
        }
    }

    return true;
}

void play_file(FILINFO *file)
{
    // Open file
    FRESULT res;
    res = pf_open_fileinfo(file);
    if (res != FR_OK) {
        // Handle open error
        return;
    }

    /* --- Setup decoding --- */
    struct sbc_frame frame = {0};
    sbc_t sbc = {0};

    // reading frame at the beginning to setup frequency
    if (freadwrap(data, SBC_PROBE_SIZE) < 1
            || sbc_probe(data, &frame) < 0) {
    }

    int srate_hz = sbc_get_freq_hz(frame.freq);

    sbc_reset(&sbc);

    int pos = 0;

    unmute(); // todo: this might need to be moved deeper

    do {
        WDT::feed();
        if (freadwrap(data + SBC_PROBE_SIZE,
                sbc_get_frame_size(&frame) - SBC_PROBE_SIZE) < 1) {
            break;
        }


        int npcm = frame.nblocks * frame.nsubbands;

        // disable interrupts during decode, too stack intensive
        __disable_irq();

        sbc_decode(&sbc, data, sizeof(data),
            &frame, &pcml[pos], &pcmr[pos]);

        __enable_irq();
        
        const bool left_part = pos < CHANNEL_HALF_BUFFER;

        pos += npcm;

        if (left_part && pos >= CHANNEL_HALF_BUFFER) {
            // wait for transfer complete
            while (!transfer_complete) {
                if (save_state_requested) {
                    handle_state_save();
                }
            };
            transfer_complete = false;

        }

        if (pos >= CHANNEL_FULL_BUFFER) {
            // wait for half transfer
            while (!half_transfer) {
                if (save_state_requested) {
                    handle_state_save();
                }
            };
            half_transfer = false;
            pos = 0;
        }
    }

    while(playback_command == PlaybackCommand::KeepPlaying && freadwrap(data, SBC_PROBE_SIZE) >= 1 && sbc_probe(data, &frame) == 0);
    mute();
}

} // namespace Controller

// =============================================================================
// DMA Interrupt Handler
// =============================================================================

void DMA1_Channel1_IRQHandler() {
    // Check for DMA1 Channel 1 Transfer Complete Interrupt
    if (DMA1->ISR & DMA_ISR_TCIF1) {
        Controller::transfer_complete = true;
        DMA1->IFCR |= DMA_IFCR_CTCIF1;  // Clear interrupt flag
    }
    
    // Check for DMA1 Channel 1 Half Transfer Interrupt
    if (DMA1->ISR & DMA_ISR_HTIF1) {
        Controller::half_transfer = true;
        DMA1->IFCR |= DMA_IFCR_CHTIF1;  // Clear interrupt flag
    }
}
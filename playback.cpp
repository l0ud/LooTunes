#include <cstddef>

#include "playback.h"
#include "libsbc/include/sbc.h"

#include "petitfat/source/pff.h"
#include "gpio.h"
#include "watchdog.h"
#include "config.h"
#include "light_sensor.h"

extern "C" {
    #include "defines.h"
    #include "py32f0xx.h"
    #include "py32f0xx_hal.h"
}


#define CHANNEL_HALF_BUFFER (SBC_MAX_SAMPLES) // 128
#define CHANNEL_FULL_BUFFER (CHANNEL_HALF_BUFFER * 2) // 256

namespace {

    FATFS fs;
    uint8_t data[SBC_MAX_SAMPLES*sizeof(int16_t)] = {0};
    int16_t pcml[CHANNEL_FULL_BUFFER] = {0};
    int16_t pcmr[CHANNEL_FULL_BUFFER] = {0};

    volatile bool half_transfer = false;
    volatile bool transfer_complete = false;

}
    enum class PlaybackCommand : uint8_t {
        KeepPlaying = 0,
        NextTrack = 1,
        PrevTrack = 2
    };
    volatile PlaybackCommand playback_command = PlaybackCommand::KeepPlaying;

// Timer settings
#define TIM_PERIOD_48KHZ (250 - 1)  // 192 kHz PWM frequency
#define TIM_PERIOD_441KHZ (272 - 1)  // 176.4 kHz PWM frequency
#define REPEAT_COUNT (3)      // Update every 4 PWM cycles (RCR + 1 = 4)


Controller::MState Controller::m_state = Controller::MState::FORCED_OFF;
Controller::PState Controller::p_state = Controller::PState::NOT_PLAYING;

bool Controller::init() {
    // init TIM1

    __HAL_RCC_TIM1_CLK_ENABLE();

    // Configure TIM1 as PWM output
    TIM1->PSC = 0;                   // No prescaler (48 MHz clock)
    TIM1->ARR = TIM_PERIOD_441KHZ;   // Set Auto-Reload Register for desired frequency
    TIM1->CCR2 = 0;                   // Set initial duty cycle to 0
    TIM1->CCR3 = 0;                   // Set initial duty cycle to 0
    
    TIM1->RCR = REPEAT_COUNT;         // Update every 4 PWM cycles

    TIM1->CCMR1 |= TIM_CCMR1_OC2M_2 | TIM_CCMR1_OC2M_1;  // PWM Mode 1 (110)
    TIM1->CCMR2 |= TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3M_1;  // PWM Mode 1 (110)

    TIM1->CCER |= TIM_CCER_CC2E | TIM_CCER_CC3E; // Enable CH2 and CH3 output
    TIM1->DIER |= TIM_DIER_UDE;       // Enable Update DMA Request
    TIM1->BDTR |= TIM_BDTR_MOE;       // Main Output Enable


    // turn on DMA
    __HAL_RCC_DMA_CLK_ENABLE();

    SYSCFG->CFGR3 &= ~(SYSCFG_CFGR3_DMA1_MAP_Msk | SYSCFG_CFGR3_DMA2_MAP_Msk);
    SYSCFG->CFGR3 |= SYSCFG_CFGR3_DMA1_MAP_4 | SYSCFG_CFGR3_DMA2_MAP_4 | SYSCFG_CFGR3_DMA2_ACKLVL | SYSCFG_CFGR3_DMA1_ACKLVL; // map dma channel 1 and 2 to TIM1_UP

    // map channel 2


    DMA1_Channel1->CCR &= ~DMA_CCR_EN;  // Disable DMA before configuration
    DMA1_Channel1->CNDTR = CHANNEL_FULL_BUFFER;
    DMA1_Channel1->CPAR = (uint32_t)&TIM1->CCR2;  // Peripheral address is CCR1
    DMA1_Channel1->CMAR = (uint32_t)pcml;    // Memory address to left channel
    
    // Configure the DMA Channel settings
    DMA1_Channel1->CCR |= DMA_CCR_MINC;  // Memory increment mode
    DMA1_Channel1->CCR |= DMA_CCR_DIR;   // Memory-to-peripheral direction

    //!!!!
    DMA1_Channel1->CCR |= DMA_CCR_MSIZE_0; // Memory size: 16-bit

    DMA1_Channel1->CCR |= DMA_CCR_PSIZE_0; // Peripheral size: 16-bit
    DMA1_Channel1->CCR |= DMA_CCR_CIRC;   // Circular mode to repeat the DMA transfer
    // enable half transfer and transfer complete interrupts
    DMA1_Channel1->CCR |= DMA_CCR_HTIE | DMA_CCR_TCIE;

    // Enable DMA interrupt
    NVIC_EnableIRQ(DMA1_Channel1_IRQn);


    // prepare DMA for right channel
    DMA1_Channel2->CCR &= ~DMA_CCR_EN;  // Disable DMA before configuration
    DMA1_Channel2->CNDTR = CHANNEL_FULL_BUFFER;
    DMA1_Channel2->CPAR = (uint32_t)&TIM1->CCR3;  // Peripheral address is CCR3
    DMA1_Channel2->CMAR = (uint32_t)pcmr;    // Memory address to right channel

    // Configure the DMA Channel settings
    DMA1_Channel2->CCR |= DMA_CCR_MINC;  // Memory increment mode
    DMA1_Channel2->CCR |= DMA_CCR_DIR;   // Memory-to-peripheral direction

    //!!!!
    DMA1_Channel2->CCR |= DMA_CCR_MSIZE_0; // Memory size: 16-bit

    DMA1_Channel2->CCR |= DMA_CCR_PSIZE_0; // Peripheral size: 16-bit
    DMA1_Channel2->CCR |= DMA_CCR_CIRC;   // Circular mode to repeat the DMA transfer
    // do not enable interrupts for right channel

    DMA1_Channel1->CCR |= DMA_CCR_EN;     // Enable DMA1 Channel
    DMA1_Channel2->CCR |= DMA_CCR_EN;     // Enable DMA2 Channel

    TIM1->CR1 = TIM_CR1_ARPE;         // Enable timer AUTO PRELOAD

    FRESULT res = pf_mount(&fs);
    WDT::feed();
    
    if (res != FR_OK) {
        // todo: might handle mount error
        return false;
    }

    // load config file
    CFG.load_from_file("config.ini");

    // apply static usb modes, if selected
    if (CFG.usb_mode == Config::UsbMode::AlwaysOn) {
        GPIO::usb_power_on();
    } else if (CFG.usb_mode == Config::UsbMode::AlwaysOff) {
        GPIO::usb_power_off();
    }

    if (CFG.light_mode != Config::LightMode::Disabled) {
        change_main_state(Controller::MState::SENSOR);
        change_playing_state(Controller::PState::PLAYING);
        set_thresholds_for_state();
    }

    return true;
}

void DMA1_Channel1_IRQHandler()
{
    // check interrupt source
    if (DMA1->ISR & DMA_ISR_TCIF1) {
        // clear interrupt flag
        transfer_complete = true;
        DMA1->IFCR |= DMA_IFCR_CTCIF1;
    }
    if (DMA1->ISR & DMA_ISR_HTIF1) {
        // clear interrupt flag
        half_transfer = true;
        DMA1->IFCR |= DMA_IFCR_CHTIF1;
    }
}

void Controller::main() {
    // list main directory
    DIR dj;
    FRESULT res = pf_opendir(&dj, "/");
    if (res != FR_OK) {
        // todo: might handle opendir error
        return;
    }


    for (;;) {
        FILINFO fno;
        if (playback_command == PlaybackCommand::PrevTrack) {
            pf_prevdir(&dj);
        }

        res = pf_readdir(&dj, &fno);
        if (res != FR_OK || fno.fname[0] == 0) {
            break;
        }

        WDT::feed();

        playback_command = PlaybackCommand::KeepPlaying;
        // Handle file
        play_file(fno.fname);

    }

}

inline __attribute__((always_inline)) UINT freadwrap(void* buff, UINT size) {
    UINT br;
    FRESULT res = pf_read_cached(buff, size, &br);
    if (res != FR_OK) {
        // Handle read error
        while(1) { };
    }

    return br;
}

void ButtonPressCallback(BTN::ID id) {
    Controller::on_button_press(id);
}

void LightSensorCallback(uint16_t value) {
    Controller::on_light_sensor(value);
}

void Controller::on_button_press(BTN::ID id)
{
    switch (id) {
        case BTN::ID::POWER:
        {
            MState new_state = static_cast<MState>((static_cast<int>(m_state) + 1) % static_cast<int>(MState::LAST_ELEMENT));

            if (new_state == MState::SENSOR && CFG.light_mode == Config::LightMode::Disabled) {
                // If light sensor is disabled, skip to FORCED_OFF state
                new_state = MState::FORCED_OFF;
            }

            change_main_state(new_state);
            break;
        }
        case BTN::ID::TODO:
            // decide what holding power button might do, if needed
            break;
        case BTN::ID::NEXT:
            playback_command = PlaybackCommand::NextTrack;
            break;
        case BTN::ID::PREV:
            playback_command = PlaybackCommand::PrevTrack;
            break;
    }
}

void Controller::on_light_sensor(uint16_t value)
{
    if (m_state == MState::SENSOR) {
        if (value < CFG.on_treshold) {
            // turn on
            change_playing_state(CFG.light_mode == Config::LightMode::Normal ? PState::PLAYING : PState::NOT_PLAYING);
        } else if (value > CFG.off_treshold) {
            // turn off
            change_playing_state(CFG.light_mode == Config::LightMode::Normal ? PState::NOT_PLAYING : PState::PLAYING);
        }

        set_thresholds_for_state();
    }
}

void Controller::set_thresholds_for_state()
{
    const bool arm_to_dim = (p_state == PState::PLAYING
            && CFG.light_mode == Config::LightMode::Normal)
            || (p_state == PState::NOT_PLAYING
            && CFG.light_mode == Config::LightMode::Reversed);
    // closer to 0 the sensor value is, the more light is present
    if (arm_to_dim) {
        // we're playing - arm light sensor for powering off
        LIGHT::set_thresholds(0, CFG.off_treshold);
    }
    else {
        // arm light sensor for dim condition
        LIGHT::set_thresholds(CFG.on_treshold, 0xfff);
    }
}

void Controller::change_main_state(MState new_state)
{
    if (m_state == new_state) {
        // No change needed
        return;
    }
    if (new_state == MState::FORCED_OFF) {
        LIGHT::stop();
        change_playing_state(PState::NOT_PLAYING);
    } else if (new_state == MState::FORCED_ON) {
        LIGHT::stop();
        change_playing_state(PState::PLAYING);
    } else if (new_state == MState::SENSOR) {
        // Handle sensor state, if needed
        set_thresholds_for_state();
        LIGHT::start();
    }

    m_state = new_state;
}

void Controller::change_playing_state(PState new_state)
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

void Controller::play_file(const char* filename)
{
    // Open file
    FRESULT res;
    res = pf_open(filename);
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
        
        const bool leftPart = pos < CHANNEL_HALF_BUFFER;

        pos += npcm;

        if (leftPart && pos >= CHANNEL_HALF_BUFFER) {
            // wait for transfer complete
            while (!transfer_complete) { };
            transfer_complete = false;

        }

        if (pos >= CHANNEL_FULL_BUFFER) {
            // wait for half transfer
            while (!half_transfer) { };
            half_transfer = false;
            pos = 0;
        }
    }

    while(playback_command == PlaybackCommand::KeepPlaying && freadwrap(data, SBC_PROBE_SIZE) >= 1 && sbc_probe(data, &frame) == 0);

}

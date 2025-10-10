#include "audio_player.h"
#include "libsbc/include/sbc.h"
#include "watchdog.h"
#include "utility.h"
#include "file_navigator.h"

extern "C" {
    #include "defines.h"
    #include "py32f0xx.h"
    #include "py32f0xx_hal.h"
}

namespace AudioPlayer {

// =============================================================================
// Constants
// =============================================================================

// Audio buffer configuration
constexpr auto CHANNEL_HALF_BUFFER = SBC_MAX_SAMPLES; // 128
constexpr auto CHANNEL_FULL_BUFFER = CHANNEL_HALF_BUFFER * 2; // 256

// Timer configuration
constexpr uint32_t TIM_PERIOD_48KHZ = (250 - 1)*4;   // 192 kHz PWM frequency
constexpr uint32_t TIM_PERIOD_441KHZ = (272 - 1)*4;  // 176.4 kHz PWM frequency
constexpr uint32_t REPEAT_COUNT = 0;              // Update every 4 PWM cycles (RCR + 1 = 4)

// =============================================================================
// Internal State
// =============================================================================

namespace {
    // Audio buffers
    int16_t pcml[CHANNEL_FULL_BUFFER] = {0};
    int16_t pcmr[CHANNEL_FULL_BUFFER] = {0};
    uint8_t data[SBC_MAX_SAMPLES*sizeof(int16_t)] = {0};
    constexpr auto silence = make_filled_array<int16_t, CHANNEL_FULL_BUFFER, (136*4)>();

    // Synchronization flags
    volatile bool half_transfer = false;
    volatile bool transfer_complete = false;
    volatile PlaybackCommand playback_command = PlaybackCommand::KeepPlaying;
    volatile uint32_t mute_ref = 0;
}

// =============================================================================
// Helper Functions
// =============================================================================

void __attribute__ ((noinline)) handle_state_save_during_playback() {
    // Save PetitFat state to avoid losing track of currently played file
    FATFS petit_state;
    pf_save_state(&petit_state);
    FileNavigator::handle_state_save();
    pf_restore_state(&petit_state);
}

// =============================================================================
// Hardware Initialization
// =============================================================================

void init_timer() {
    __HAL_RCC_TIM1_CLK_ENABLE();

    // Configure TIM1 as PWM output
    TIM1->PSC = 0;                     // No prescaler (48 MHz clock)
    TIM1->ARR = TIM_PERIOD_441KHZ;     // Set Auto-Reload Register for desired frequency
    TIM1->CCR2 = silence[0];                    // Set initial duty cycle to silence
    TIM1->CCR3 = silence[0];                    // Set initial duty cycle to silence
    TIM1->RCR = REPEAT_COUNT;          // Update every 4 PWM cycles

    TIM1->CCMR1 |= TIM_CCMR1_OC2M_2 | TIM_CCMR1_OC2M_1;  // PWM Mode 1 (110)
    TIM1->CCMR2 |= TIM_CCMR2_OC3M_2 | TIM_CCMR2_OC3M_1;  // PWM Mode 1 (110)

    TIM1->CCER |= TIM_CCER_CC2E | TIM_CCER_CC3E;  // Enable CH2 and CH3 output
    TIM1->DIER |= TIM_DIER_UDE;                   // Enable Update DMA Request
    TIM1->BDTR |= TIM_BDTR_MOE;                   // Main Output Enable
    TIM1->CR1 = TIM_CR1_ARPE;                     // Enable timer AUTO PRELOAD

    TIM1->CR1 |= TIM_CR1_CEN;         // Enable the timer
}

void init_dma() {
    __HAL_RCC_DMA_CLK_ENABLE();

    // Configure DMA mapping
    SYSCFG->CFGR3 &= ~(SYSCFG_CFGR3_DMA1_MAP_Msk | SYSCFG_CFGR3_DMA2_MAP_Msk);
    SYSCFG->CFGR3 |= SYSCFG_CFGR3_DMA1_MAP_4 | SYSCFG_CFGR3_DMA2_MAP_4 | 
                        SYSCFG_CFGR3_DMA2_ACKLVL | SYSCFG_CFGR3_DMA1_ACKLVL;

    // Configure DMA Channel 1 (Left channel)
    DMA1_Channel1->CCR = 0;
    DMA1_Channel1->CNDTR = CHANNEL_FULL_BUFFER;
    DMA1_Channel1->CPAR = (uint32_t)&TIM1->CCR2;

    const uint32_t ccr1 = DMA_CCR_MINC |      // Memory increment mode
                            DMA_CCR_DIR |     // Memory-to-peripheral direction
                            DMA_CCR_MSIZE_0 | // Memory size: 16-bit
                            DMA_CCR_PSIZE_0 | // Peripheral size: 16-bit
                            DMA_CCR_CIRC |    // Circular mode
                            DMA_CCR_HTIE |    // Half transfer interrupt
                            DMA_CCR_TCIE |    // Transfer complete interrupt
                            DMA_CCR_EN;       // Enable channel

    // Configure DMA Channel 2 (Right channel)
    DMA1_Channel2->CCR &= ~DMA_CCR_EN;
    DMA1_Channel2->CNDTR = CHANNEL_FULL_BUFFER;
    DMA1_Channel2->CPAR = (uint32_t)&TIM1->CCR3;

    const uint32_t ccr2 = DMA_CCR_MINC |      // Memory increment mode
                            DMA_CCR_DIR |     // Memory-to-peripheral direction
                            DMA_CCR_MSIZE_0 | // Memory size: 16-bit
                            DMA_CCR_PSIZE_0 | // Peripheral size: 16-bit
                            DMA_CCR_CIRC |    // Circular mode
                            DMA_CCR_EN;       // Enable channel

    // Enable DMA channels but not interrupt
    DMA1_Channel1->CMAR = (uint32_t)silence.data();
    DMA1_Channel2->CMAR = (uint32_t)silence.data();

    DMA1_Channel1->CCR = ccr1;
    DMA1_Channel2->CCR = ccr2;
}

void init() {
    init_dma();
    mute(); // by default muted, this lock will be released just before playback
    init_timer();
}

// =============================================================================
// Playback Control
// =============================================================================
void mute() {
    if (mute_ref++ > 0) {
        return; // already muted
    }
    NVIC_DisableIRQ(DMA1_Channel1_IRQn);
    DMA1_Channel1->CMAR = (uint32_t)silence.data();
    DMA1_Channel2->CMAR = (uint32_t)silence.data();
}

void reset_mute() {
    mute_ref = 0;
    mute(); // final ref = 1, this represents playback mute lock (no file is played at the beginning)
    // note: state mute lock (forced off/light low) is applied later, on state change in controller
}

void unmute() {
    if (mute_ref == 0 || --mute_ref > 0) {
        return; // still muted
    }
    NVIC_EnableIRQ(DMA1_Channel1_IRQn);
    DMA1_Channel1->CMAR = (uint32_t)pcml;
    DMA1_Channel2->CMAR = (uint32_t)pcmr;
}

bool muted() {
    return mute_ref > 0;
}

// =============================================================================
// File Playback
// =============================================================================

bool play_file(FILINFO *file, PlaybackCommand &command) {
    // Open file
    FRESULT res;
    res = pf_open_fileinfo(file);
    if (res != FR_OK) {
        return false;
    }

    /* --- Setup decoding --- */
    struct sbc_frame frame = {0};
    sbc_t sbc = {0};

    // reading frame at the beginning to setup frequency
    if (freadwrap(data, SBC_PROBE_SIZE) < 1
            || sbc_probe(data, &frame) < 0) {
        return false;
    }

    int srate_hz = sbc_get_freq_hz(frame.freq);

    sbc_reset(&sbc);

    int pos = 0;

    playback_command = PlaybackCommand::KeepPlaying; // Reset command

    unmute();

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
                if (muted() && FileNavigator::is_state_save_requested()) {
                    handle_state_save_during_playback();
                }
            };
            transfer_complete = false;
        }

        if (pos >= CHANNEL_FULL_BUFFER) {
            // wait for half transfer
            while (!half_transfer) {
                if (muted() && FileNavigator::is_state_save_requested()) {
                    handle_state_save_during_playback();
                }
            };
            half_transfer = false;
            pos = 0;
        }
    }
    while(playback_command == PlaybackCommand::KeepPlaying && freadwrap(data, SBC_PROBE_SIZE) >= 1 && sbc_probe(data, &frame) == 0);
    
    mute();

    command = playback_command;
    return true;
}

void set_playback_command(PlaybackCommand command) {
    playback_command = command;
}

} // namespace AudioPlayer

// =============================================================================
// DMA Interrupt Handler
// =============================================================================

void DMA1_Channel1_IRQHandler() {
    // Check for DMA1 Channel 1 Transfer Complete Interrupt
    if (DMA1->ISR & DMA_ISR_TCIF1) {
        AudioPlayer::transfer_complete = true;
        DMA1->IFCR |= DMA_IFCR_CTCIF1;  // Clear interrupt flag
    }
    
    // Check for DMA1 Channel 1 Half Transfer Interrupt
    if (DMA1->ISR & DMA_ISR_HTIF1) {
        AudioPlayer::half_transfer = true;
        DMA1->IFCR |= DMA_IFCR_CHTIF1;  // Clear interrupt flag
    }
}
